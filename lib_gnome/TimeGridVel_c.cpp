/*
 *  TimeGridVel_c.cpp
 *  gnome
 *
 *  Created by Generic Programmer on 12/1/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "TimeGridVel_c.h"
#include "netcdf.h"
#include "CompFunctions.h"
#include "StringFunctions.h"
#include <math.h>
#include <float.h>
#include "DagTreeIO.h"
#include "OUtils.h"	// for the units

#ifndef pyGNOME
#include "CROSS.H"
#else
#include "Replacements.h"
#endif

using std::cout;

/////////////////////////////////////////////////
Boolean IsGridCurTimeFile (char *path, short *selectedUnitsP)
{
	Boolean bIsValid = false;
	OSErr	err = noErr;
	long line;
	char strLine [256];
	char firstPartOfFile [256];
	long lenToRead,fileLength;
	short selectedUnits = kUndefined, numScanned;
	char unitsStr[64], gridcurStr[64];
	
	err = MyGetFileSize(0,0,path,&fileLength);
	if(err) return false;
	
	lenToRead = _min(256,fileLength);
	
	err = ReadSectionOfFile(0,0,path,0,lenToRead,firstPartOfFile,0);
	firstPartOfFile[lenToRead-1] = 0; // make sure it is a cString
	if (!err)
	{	// must start with [GRIDCURTIME]
		char * strToMatch = "[GRIDCURTIME]";
		NthLineInTextNonOptimized (firstPartOfFile, line = 0, strLine, 256);
		if (!strncmp (strLine,strToMatch,strlen(strToMatch)))
		{
			bIsValid = true;
			*selectedUnitsP = selectedUnits;
			numScanned = sscanf(strLine,"%s%s",gridcurStr,unitsStr);
			if(numScanned != 2) { selectedUnits = kUndefined; goto done; }
			RemoveLeadingAndTrailingWhiteSpace(unitsStr);
			selectedUnits = StrToSpeedUnits(unitsStr);// note we are not supporting cm/sec in gnome
		}
	}
	
done:
	if(bIsValid)
	{
		*selectedUnitsP = selectedUnits;
	}
	return bIsValid;
}

/////////////////////////////////////////////////
Boolean IsPtCurFile (char *path)
{
	Boolean	bIsValid = false;
	OSErr	err = noErr;
	long line;
	char	strLine [256];
	char	firstPartOfFile [256];
	long lenToRead,fileLength;
	
	err = MyGetFileSize(0,0,path,&fileLength);
	if(err) return false;
	
	lenToRead = _min(256,fileLength);
	
	err = ReadSectionOfFile(0,0,path,0,lenToRead,firstPartOfFile,0);
	firstPartOfFile[lenToRead-1] = 0; // make sure it is a cString
	if (!err)
	{	// must start with [FILETYPE] PTCUR
		char * strToMatch = "[FILETYPE]\tPTCUR";
		NthLineInTextNonOptimized (firstPartOfFile, line = 0, strLine, 256);
		if (!strncmp (strLine,strToMatch,strlen(strToMatch)))
			bIsValid = true;
	}
	
	return bIsValid;
}

/////////////////////////////////////////////////
Boolean IsNetCDFPathsFile (char *path, Boolean *isNetCDFPathsFile, char *fileNamesPath, short *gridType)
{
	// NOTE!! if the input variable path does point to a NetCDFPaths file, 
	// the input variable is overwritten with the path to the first NetCDF file.
	// The original input value of path is copied to fileNamesPath in such a case.
	// If the input vatiable does not point to a NetCDFPaths file, the input path is left unchanged.
	Boolean	bIsValid = false;
	OSErr	err = noErr;
	long line = 0;
	char	strLine [512];
	char	firstPartOfFile [512], classicPath[256];
	long lenToRead,fileLength;
	char *key;
	
	*isNetCDFPathsFile = false;
	
	err = MyGetFileSize(0,0,path,&fileLength);
	if(err) return false;
	
	lenToRead = _min(512,fileLength);
	
	err = ReadSectionOfFile(0,0,path,0,lenToRead,firstPartOfFile,0);
	firstPartOfFile[lenToRead-1] = 0; // make sure it is a cString
	
	if(err) {
		// should we report the file i/o err to the user here ?
		return false;
	}
	
	// must start with "NetCDF Files"
	NthLineInTextNonOptimized (firstPartOfFile, line++, strLine, 512);
	RemoveLeadingAndTrailingWhiteSpace(strLine);
	key = "NetCDF Files";
	if (strncmpnocase (strLine, key, strlen(key)) != 0)
		return false;
	
	// next line must be "[FILE] <path>"
	NthLineInTextNonOptimized(firstPartOfFile, line++, strLine, 512); 
	RemoveLeadingAndTrailingWhiteSpace(strLine);
	key = "[FILE]";
	if (strncmpnocase (strLine, key, strlen(key)) != 0)
		return false;
	
	strcpy(fileNamesPath,path); // transfer the input path to this output variable
	
	strcpy(path,strLine+strlen(key)); // this is overwriting the input variable (see NOTE above)
	RemoveLeadingAndTrailingWhiteSpace(path);
	ResolvePathFromInputFile(fileNamesPath,path); // JLM 6/8/10
	
	if(!FileExists(0,0,path)){
		// tell the user the file does not exist
		printError("FileExists returned false for the first path listed in the IsNetCDFPathsFile.");
		return false;
	}
	
	bIsValid = IsNetCDFFile (path, gridType);
	if (bIsValid) *isNetCDFPathsFile = true;
	else{
		// tell the user this is not a NetCDF file
		printError("IsNetCDFFile returned false for the first path listed in the IsNetCDFPathsFile.");
		return false;
	}
	
	return bIsValid;
}

/////////////////////////////////////////////////
Boolean IsNetCDFFile (char *path, short *gridType)	
{
	// separate into IsNetCDFFile and GetGridType
	Boolean	bIsValid = false;
	OSErr err = noErr;
	long line;
	char strLine [512], outPath[256];
	char firstPartOfFile [512], *modelTypeStr=0, *gridTypeStr=0, *sourceStr=0/*, *historyStr=0*/;
	long lenToRead,fileLength;
	int status, ncid;
	size_t t_len, t_len2;
	
	err = MyGetFileSize(0,0,path,&fileLength);
	if(err) return false;
	
	lenToRead = _min(512,fileLength);
	
	err = ReadSectionOfFile(0,0,path,0,lenToRead,firstPartOfFile,0);
	firstPartOfFile[lenToRead-1] = 0; // make sure it is a cString
	if (!err)
	{	// must start with "CDF
		NthLineInTextNonOptimized (firstPartOfFile, line = 0, strLine, 512);
		if (!strncmp (firstPartOfFile, "CDF", 3))
			bIsValid = true;
	}
	
	if (!bIsValid) return false;
	
	// need a global attribute to identify grid type - this won't work for non Navy regular grid
	status = nc_open(path, NC_NOWRITE, &ncid);
	if (status != NC_NOERR) /*{*gridType = CURVILINEAR; goto done;}*/	// this should probably be an error
	{
#if TARGET_API_MAC_CARBON
		err = ConvertTraditionalPathToUnixPath((const char *) path, outPath, kMaxNameLen) ;
		status = nc_open(outPath, NC_NOWRITE, &ncid);
#endif
		//if (status != NC_NOERR) {*gridType = CURVILINEAR; goto done;}	// this should probably be an error
		if (status != NC_NOERR) {*gridType = REGULAR; goto done;}	// this should probably be an error - change default to regular 1/29/09
	}
	
	status = nc_inq_attlen(ncid,NC_GLOBAL,"grid_type",&t_len2);
	if (status == NC_NOERR) /*{*gridType = CURVILINEAR; goto done;}*/
	{
		gridTypeStr = new char[t_len2+1];
		status = nc_get_att_text(ncid, NC_GLOBAL, "grid_type", gridTypeStr);
		//if (status != NC_NOERR) {*gridType = CURVILINEAR; goto done;} 
		if (status != NC_NOERR) {*gridType = REGULAR; goto done;} 
		gridTypeStr[t_len2] = '\0';
		
		//if (!strncmpnocase (gridTypeStr, "REGULAR", 7) || !strncmpnocase (gridTypeStr, "UNIFORM", 7) || !strncmpnocase (gridTypeStr, "RECTANGULAR", 11))
		if (!strncmpnocase (gridTypeStr, "REGULAR", 7) || !strncmpnocase (gridTypeStr, "UNIFORM", 7) || !strncmpnocase (gridTypeStr, "RECTANGULAR", 11) /*|| !strncmpnocase (gridTypeStr, "RECTILINEAR", 11)*/)
			// note CO-OPS uses rectilinear but they have all the data for curvilinear so don't add the grid type
		{
			*gridType = REGULAR;
			goto done;
		}
		if (!strncmpnocase (gridTypeStr, "CURVILINEAR", 11) || !strncmpnocase (gridTypeStr, "RECTILINEAR", 11) || strstrnocase(gridTypeStr,"curv"))// "Rectilinear" is what CO-OPS uses, not one of our keywords. Their data is in curvilinear format. NYHOPS uses "Orthogonal Curv Grid"
		{
			*gridType = CURVILINEAR;
			goto done;
		}
		if (!strncmpnocase (gridTypeStr, "TRIANGULAR", 10))
		{
			*gridType = TRIANGULAR;
			goto done;
		}
	}
	else	// for now don't require global grid identifier since LAS files don't have it
	{
		status = nc_inq_attlen(ncid,NC_GLOBAL,"source",&t_len2);	// for HF Radar use source since no grid_type global
		if (status == NC_NOERR) 
		{
			sourceStr = new char[t_len2+1];
			status = nc_get_att_text(ncid, NC_GLOBAL, "source", sourceStr);
			if (status != NC_NOERR) { } 
			else
			{
				sourceStr[t_len2] = '\0';			
				if (!strncmpnocase (sourceStr, "Surface Ocean HF-Radar", 22)) { *gridType = REGULAR; goto done;}
			}
		}
		/*status = nc_inq_attlen(ncid,NC_GLOBAL,"history",&t_len2);	// LAS uses ferret, would also need to check for coordinate variable...
		 if (status == NC_NOERR) 
		 {
		 historyStr = new char[t_len2+1];
		 status = nc_get_att_text(ncid, NC_GLOBAL, "history", historyStr);
		 if (status != NC_NOERR) { } 
		 else
		 {
		 sourceStr[t_len2] = '\0';			
		 if (strstrnocase (historyStr, "ferret") { *gridType = REGULAR; goto done;}	// could be curvilinear - maybe a ferret flag??
		 }
		 }*/
	}
	status = nc_inq_attlen(ncid,NC_GLOBAL,"generating_model",&t_len);
	if (status != NC_NOERR) {
		status = nc_inq_attlen(ncid,NC_GLOBAL,"generator",&t_len);
		//if (status != NC_NOERR) {*gridType = CURVILINEAR; goto done;}}
		if (status != NC_NOERR) {*gridType = REGULAR; goto done;}}	// changed default to REGULAR 1/29/09
	modelTypeStr = new char[t_len+1];
	status = nc_get_att_text(ncid, NC_GLOBAL, "generating_model", modelTypeStr);
	if (status != NC_NOERR) {
		status = nc_get_att_text(ncid, NC_GLOBAL, "generator", modelTypeStr);
		//if (status != NC_NOERR) {*gridType = CURVILINEAR; goto done;} }
		if (status != NC_NOERR) {*gridType = REGULAR; goto done;} }	// changed default to REGULAR 1/29/09
	modelTypeStr[t_len] = '\0';
	
	if (!strncmp (modelTypeStr, "SWAFS", 5))
		*gridType = REGULAR_SWAFS;
	//else if (!strncmp (modelTypeStr, "NCOM", 4))
	else if (strstr (modelTypeStr, "NCOM"))	// Global NCOM
		*gridType = REGULAR;
	//else if (!strncmp (modelTypeStr, "fictitious test data", strlen("fictitious test data")))
	//*gridType = CURVILINEAR;	// for now, should have overall Navy identifier
	else
		//*gridType = CURVILINEAR;
		*gridType = REGULAR; // change default to REGULAR - 1/29/09
	
done:
	if (modelTypeStr) delete [] modelTypeStr;	
	if (gridTypeStr) delete [] gridTypeStr;	
	if (sourceStr) delete [] sourceStr;	
	//if (historyStr) delete [] historyStr;	
	return bIsValid;
}

/////////////////////////////////////////////////

TimeGridVel_c::TimeGridVel_c ()
{
	memset(&fVar,0,sizeof(fVar));
	fVar.fileScaleFactor = 1.0;
	fVar.gridType = TWO_D; // 2D default
	fVar.maxNumDepths = 1;	// 2D default - may not need this
	
	fGrid = 0;
	fTimeHdl = 0;
	
	fTimeShift = 0;	// assume file is in local time

	fOverLap = false;		// for multiple files case
	fOverLapStartTime = 0;
	
	fFillValue = -1e+34;
	//fIsNavy = false;	
	
	memset(&fStartData,0,sizeof(fStartData));
	fStartData.timeIndex = UNASSIGNEDINDEX; 
	fStartData.dataHdl = 0; 
	memset(&fEndData,0,sizeof(fEndData));
	fEndData.timeIndex = UNASSIGNEDINDEX;
	fEndData.dataHdl = 0;
	
	fInputFilesHdl = 0;	// for multiple files case
	
	fAllowExtrapolationInTime = false;
	
}


long TimeGridVel_c::GetNumTimesInFile()
{
	long numTimes = 0;
	
	if (fTimeHdl) numTimes = _GetHandleSize((Handle)fTimeHdl)/sizeof(**fTimeHdl);
	return numTimes;     
}

long TimeGridVel_c::GetNumFiles()
{
	long numFiles = 0;
	
	if (fInputFilesHdl) numFiles = _GetHandleSize((Handle)fInputFilesHdl)/sizeof(**fInputFilesHdl);
	return numFiles;     
}


Boolean TimeGridVel_c::CheckInterval(long &timeDataInterval, const Seconds& model_time)
{
	Seconds time =  model_time, startTime, endTime;	
	
	long i,numTimes,numFiles = GetNumFiles();
	
	numTimes = this -> GetNumTimesInFile(); 
	if (numTimes==0) {timeDataInterval = 0; return false;}	// really something is wrong, no data exists
	
	// check for constant current
	if (numTimes==1 && !(GetNumFiles()>1)) 
	{
		timeDataInterval = -1; // some flag here
		if(fStartData.timeIndex==0 && fStartData.dataHdl)
			return true;
		else
			return false;
	}
	
	if(fStartData.timeIndex!=UNASSIGNEDINDEX && fEndData.timeIndex!=UNASSIGNEDINDEX)
	{
		if (time>=((*fTimeHdl)[fStartData.timeIndex] + fTimeShift) && time<=((*fTimeHdl)[fEndData.timeIndex] + fTimeShift))
		{	// we already have the right interval loaded
			timeDataInterval = fEndData.timeIndex;
			return true;
		}
	}
	
	if (GetNumFiles()>1 && fOverLap)
	{	
		if (time>=fOverLapStartTime + fTimeShift && time<=(*fTimeHdl)[fEndData.timeIndex] + fTimeShift)
			return true;	// we already have the right interval loaded, time is in between two files
		else fOverLap = false;
	}
	
	//for (i=0;i<numTimes;i++) 
	for (i=0;i<numTimes-1;i++) 
	{	// find the time interval
		if (time>=((*fTimeHdl)[i] + fTimeShift) && time<=((*fTimeHdl)[i+1] + fTimeShift))
		{
			timeDataInterval = i+1; // first interval is between 0 and 1, and so on
			return false;
		}
	}	
	// don't allow time before first or after last
	if (time<((*fTimeHdl)[0] + fTimeShift)) 
	{
		// if number of files > 1 check that first is the one loaded
		timeDataInterval = 0;
		if (numFiles > 0)
		{
			//startTime = (*fInputFilesHdl)[0].startTime + fTimeShift;
			startTime = (*fInputFilesHdl)[0].startTime;
			if ((*fTimeHdl)[0] != startTime)
				return false;
		}
		if (fAllowExtrapolationInTime && fEndData.timeIndex == UNASSIGNEDINDEX && !(fStartData.timeIndex == UNASSIGNEDINDEX))	// way to recognize last interval is set
		{
			//check if time > last model time in all files
			//timeDataInterval = 1;
			return true;
		}
	}
	if (time>((*fTimeHdl)[numTimes-1] + fTimeShift) )
		// code goes here, check if this is last time in all files and user has set flag to continue
		// then if last time is loaded as start time and nothing as end time this is right interval
	{
		// if number of files > 1 check that last is the one loaded
		timeDataInterval = numTimes;
		if (numFiles > 0)
		{
			//endTime = (*fInputFilesHdl)[numFiles-1].endTime + fTimeShift;
			endTime = (*fInputFilesHdl)[numFiles-1].endTime;
			if ((*fTimeHdl)[numTimes-1] != endTime)
				return false;
		}
		if (fAllowExtrapolationInTime && fEndData.timeIndex == UNASSIGNEDINDEX && !(fStartData.timeIndex == UNASSIGNEDINDEX))	// way to recognize last interval is set
		{
			//check if time > last model time in all files
			return true;
		}
	}
	return false;
	
}

void TimeGridVel_c::DisposeTimeHdl()
{	
	if(fTimeHdl) {DisposeHandle((Handle)fTimeHdl); fTimeHdl=0;}
}

void TimeGridVel_c::DisposeLoadedData(LoadedData *dataPtr)
{
	if(dataPtr -> dataHdl) DisposeHandle((Handle) dataPtr -> dataHdl);
	ClearLoadedData(dataPtr);
}

void TimeGridVel_c::DisposeAllLoadedData()
{
	if(fStartData.dataHdl)DisposeLoadedData(&fStartData); 
	if(fEndData.dataHdl)DisposeLoadedData(&fEndData);
}

void TimeGridVel_c::ClearLoadedData(LoadedData *dataPtr)
{
	dataPtr -> dataHdl = 0;
	dataPtr -> timeIndex = UNASSIGNEDINDEX;
}


// for now leave this part out of the python and let the file path list be passed in
OSErr TimeGridVel_c::ReadInputFileNames(char *fileNamesPath)
{
	// for netcdf files, header file just has the paths, the start and end times will be read from the files
	long i,numScanned,line=0, numFiles, numLinesInText;
	DateTimeRec time;
	Seconds timeSeconds;
	OSErr err = 0;
	char s[1024], path[256], outPath[256], classicPath[kMaxNameLen];
	CHARH fileBufH = 0;
	PtCurFileInfoH inputFilesHdl = 0;
	int status, ncid, recid, timeid;
	size_t recs, t_len, t_len2;
	double timeVal;
	char recname[NC_MAX_NAME], *timeUnits=0;	
	static size_t timeIndex;
	Seconds startTime2;
	double timeConversion = 1.;
	char errmsg[256] = "";
	
	if (err = ReadFileContents(TERMINATED,0, 0, fileNamesPath, 0, 0, &fileBufH)) goto done;
	
	numLinesInText = NumLinesInText(*fileBufH);
	numFiles = numLinesInText - 1;	// subtract off the header
	inputFilesHdl = (PtCurFileInfoH)_NewHandle(sizeof(PtCurFileInfo)*numFiles);
	if(!inputFilesHdl) {TechError("TimeGridVel::ReadInputFileNames()", "_NewHandle()", 0); err = memFullErr; goto done;}
	NthLineInTextNonOptimized(*fileBufH, (line)++, s, 1024); 	// header line
	for (i=0;i<numFiles;i++)	// should count files as go along
	{
		NthLineInTextNonOptimized(*fileBufH, (line)++, s, 1024); 	// check it is a [FILE] line
		//strcpy((*inputFilesHdl)[i].pathName,s+strlen("[FILE]\t"));
		RemoveLeadingAndTrailingWhiteSpace(s);
		strcpy((*inputFilesHdl)[i].pathName,s+strlen("[FILE] "));
		RemoveLeadingAndTrailingWhiteSpace((*inputFilesHdl)[i].pathName);
		ResolvePathFromInputFile(fileNamesPath,(*inputFilesHdl)[i].pathName); // JLM 6/8/10
		//strcpy(path,(*inputFilesHdl)[i].pathName);
		if((*inputFilesHdl)[i].pathName[0] && FileExists(0,0,(*inputFilesHdl)[i].pathName))
		{
#if TARGET_API_MAC_CARBON
			err = ConvertTraditionalPathToUnixPath((const char *) (*inputFilesHdl)[i].pathName, outPath, kMaxNameLen) ;
			status = nc_open(outPath, NC_NOWRITE, &ncid);
			strcpy((*inputFilesHdl)[i].pathName,outPath);
#endif
			strcpy(path,(*inputFilesHdl)[i].pathName);
			status = nc_open(path, NC_NOWRITE, &ncid);
			if (status != NC_NOERR) /*{err = -1; goto done;}*/
			{
				if (status != NC_NOERR) {err = -2; goto done;}
			}
			
			status = nc_inq_dimid(ncid, "time", &recid); 
			if (status != NC_NOERR) 
			{
				status = nc_inq_unlimdim(ncid, &recid);	// maybe time is unlimited dimension
				if (status != NC_NOERR) {err = -2; goto done;}
			}
			
			status = nc_inq_varid(ncid, "time", &timeid); 
			if (status != NC_NOERR) {err = -2; goto done;} 
			
			/////////////////////////////////////////////////
			status = nc_inq_attlen(ncid, timeid, "units", &t_len);
			if (status != NC_NOERR) 
			{
				err = -2; goto done;
			}
			else
			{
				DateTimeRec time;
				char unitStr[24], junk[10];
				
				timeUnits = new char[t_len+1];
				status = nc_get_att_text(ncid, timeid, "units", timeUnits);
				if (status != NC_NOERR) {err = -2; goto done;} 
				timeUnits[t_len] = '\0'; // moved this statement before StringSubstitute, JLM 5/2/10
				StringSubstitute(timeUnits, ':', ' ');
				StringSubstitute(timeUnits, '-', ' ');
				
				numScanned=sscanf(timeUnits, "%s %s %hd %hd %hd %hd %hd %hd",
								  unitStr, junk, &time.year, &time.month, &time.day,
								  &time.hour, &time.minute, &time.second) ;
				if (numScanned==5)	
				{time.hour = 0; time.minute = 0; time.second = 0; }
				else if (numScanned==7)	time.second = 0;
				else if (numScanned<8)	
					//if (numScanned!=8)	
				{ err = -1; TechError("TimeGridVel::ReadInputFileNames()", "sscanf() == 8", 0); goto done; }
				DateToSeconds (&time, &startTime2);	// code goes here, which start Time to use ??
				if (!strcmpnocase(unitStr,"HOURS") || !strcmpnocase(unitStr,"HOUR"))
					timeConversion = 3600.;
				else if (!strcmpnocase(unitStr,"MINUTES") || !strcmpnocase(unitStr,"MINUTE"))
					timeConversion = 60.;
				else if (!strcmpnocase(unitStr,"SECONDS") || !strcmpnocase(unitStr,"SECOND"))
					timeConversion = 1.;
				else if (!strcmpnocase(unitStr,"DAYS") || !strcmpnocase(unitStr,"DAY"))
					timeConversion = 24*3600.;
			} 
			
			status = nc_inq_dim(ncid, recid, recname, &recs);
			if (status != NC_NOERR) {err = -2; goto done;}
			{
				Seconds newTime;
				// possible units are, HOURS, MINUTES, SECONDS,...
				timeIndex = 0;	// first time
				status = nc_get_var1_double(ncid, timeid, &timeIndex, &timeVal);
				if (status != NC_NOERR) {strcpy(errmsg,"Error reading times from NetCDF file"); printError(errmsg); err = -1; goto done;}
				newTime = RoundDateSeconds(round(startTime2+timeVal*timeConversion));
				(*inputFilesHdl)[i].startTime = newTime;
				timeIndex = recs-1;	// last time
				status = nc_get_var1_double(ncid, timeid, &timeIndex, &timeVal);
				if (status != NC_NOERR) {strcpy(errmsg,"Error reading times from NetCDF file"); printError(errmsg); err = -1; goto done;}
				newTime = RoundDateSeconds(round(startTime2+timeVal*timeConversion));
				(*inputFilesHdl)[i].endTime = newTime;
			}
			status = nc_close(ncid);
			if (status != NC_NOERR) {err = -2; goto done;}
		}	
		else 
		{
			char msg[256];
			sprintf(msg,"PATH to NetCDF data File does not exist.%s%s",NEWLINESTRING,(*inputFilesHdl)[i].pathName);
			printError(msg);
			err = true;
			goto done;
		}
		
		
	}
	if(fInputFilesHdl) {DisposeHandle((Handle)fInputFilesHdl); fInputFilesHdl=0;}	// so could replace list
	fInputFilesHdl = inputFilesHdl;
	
done:
	if(fileBufH) { DisposeHandle((Handle)fileBufH); fileBufH = 0;}
	if (err)
	{
		if (err==-2) {printError("Error reading netCDF file");}
		if(inputFilesHdl) {DisposeHandle((Handle)inputFilesHdl); inputFilesHdl=0;}
	}
	return err;
}

long TimeGridVelRect_c::GetNumDepthLevelsInFile()
{
	long numDepthLevels = 0;
	
	if (fDepthLevelsHdl) numDepthLevels = _GetHandleSize((Handle)fDepthLevelsHdl)/sizeof(**fDepthLevelsHdl);
	return numDepthLevels;     
}

/////////////////////////////////////////////////////////////////
OSErr TimeGridVelRect_c::TextRead(char *path, char *topFilePath) 
{
	// this code is for regular grids
	// For regridded data files don't have the real latitude/longitude values
	// Also may want to get fill_Value and scale_factor here, rather than every time velocities are read
	OSErr err = 0;
	long i,j, numScanned;
	int status, ncid, latid, lonid, depthid, recid, timeid, numdims;
	int latvarid, lonvarid, depthvarid;
	size_t latLength, lonLength, depthLength, recs, t_len, t_len2;
	double startLat,startLon,endLat,endLon,dLat,dLon,timeVal;
	char recname[NC_MAX_NAME], *timeUnits=0;	
	WorldRect bounds;
	double *lat_vals=0,*lon_vals=0,*depthLevels=0;
	TRectGridVel *rectGrid = nil;
	static size_t latIndex=0,lonIndex=0,timeIndex,ptIndex=0;
	static size_t pt_count[3];
	Seconds startTime, startTime2;
	double timeConversion = 1., scale_factor = 1.;
	char errmsg[256] = "";
	char fileName[64],s[256],*modelTypeStr=0,outPath[256];
	Boolean bStartTimeYearZero = false;
	
	if (!path || !path[0]) return 0;
	strcpy(fVar.pathName,path);
	
	strcpy(s,path);
	//SplitPathFile (s, fileName);
	SplitPathFileName (s, fileName);
	strcpy(fVar.userName, fileName);	// maybe use a name from the file
	
	status = nc_open(path, NC_NOWRITE, &ncid);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	status = nc_inq_dimid(ncid, "time", &recid); //Navy
	if (status != NC_NOERR) 
	{
		status = nc_inq_unlimdim(ncid, &recid);	// issue of time not being unlimited dimension
		if (status != NC_NOERR || recid==-1) {err = -1; goto done;}
	}
	
	status = nc_inq_varid(ncid, "time", &timeid); 
	if (status != NC_NOERR) {status = nc_inq_varid(ncid, "TIME", &timeid);if (status != NC_NOERR) {err = -1; goto done;} } 	// for Ferret files, everything is in CAPS
	/////////////////////////////////////////////////
	
	status = nc_inq_attlen(ncid, timeid, "units", &t_len);
	if (status != NC_NOERR) 
	{
		err = -1; goto done;
	}
	else
	{
		DateTimeRec time;
		char unitStr[24], junk[10];
		
		timeUnits = new char[t_len+1];
		status = nc_get_att_text(ncid, timeid, "units", timeUnits);
		if (status != NC_NOERR) {err = -1; goto done;} 
		timeUnits[t_len] = '\0'; // moved this statement before StringSubstitute, JLM 5/2/10
		StringSubstitute(timeUnits, ':', ' ');
		StringSubstitute(timeUnits, '-', ' ');
		
		numScanned=sscanf(timeUnits, "%s %s %hd %hd %hd %hd %hd %hd",
						  unitStr, junk, &time.year, &time.month, &time.day,
						  &time.hour, &time.minute, &time.second) ;
		if (numScanned==5)	
			//if (numScanned<8)	
		{time.hour = 0; time.minute = 0; time.second = 0; }
		else if (numScanned==7)	time.second = 0;
		else if (numScanned<8)	
			//else if (numScanned!=8)	
		{ err = -1; TechError("TimeGridVel::TextRead()", "sscanf() == 8", 0); goto done; }
		if (/*time.year==0 ||*/ time.year==1) {time.year+=2000; bStartTimeYearZero = true;}
		DateToSeconds (&time, &startTime2);	// code goes here, which start Time to use ??
		if (!strcmpnocase(unitStr,"HOURS") || !strcmpnocase(unitStr,"HOUR"))
			timeConversion = 3600.;
		else if (!strcmpnocase(unitStr,"MINUTES") || !strcmpnocase(unitStr,"MINUTE"))
			timeConversion = 60.;
		else if (!strcmpnocase(unitStr,"SECONDS") || !strcmpnocase(unitStr,"SECOND"))
			timeConversion = 1.;
		else if (!strcmpnocase(unitStr,"DAYS") || !strcmpnocase(unitStr,"DAY"))
			timeConversion = 24*3600.;
	} 
	
	// probably don't need this field anymore
	status = nc_inq_attlen(ncid,NC_GLOBAL,"generating_model",&t_len2);
	if (status != NC_NOERR) {status = nc_inq_attlen(ncid,NC_GLOBAL,"generator",&t_len2); if (status != NC_NOERR) {fIsNavy = false; /*goto done;*/}}	// will need to split for regridded or non-Navy cases
	else 
	{
		fIsNavy = true;
		// may only need to see keyword is there, since already checked grid type
		modelTypeStr = new char[t_len2+1];
		status = nc_get_att_text(ncid, NC_GLOBAL, "generating_model", modelTypeStr);
		if (status != NC_NOERR) {status = nc_inq_attlen(ncid,NC_GLOBAL,"generator",&t_len2); if (status != NC_NOERR) {fIsNavy = false; goto done;}}	// will need to split for regridded or non-Navy cases 
		modelTypeStr[t_len2] = '\0';
		
		strcpy(fVar.userName, modelTypeStr); // maybe use a name from the file
		if (!strncmp (modelTypeStr, "SWAFS", 5) || strstr (modelTypeStr, "NCOM"))
			fIsNavy = true;
		else
			fIsNavy = false;
	}
	
	// changed standard format to match Navy's for regular grid
	status = nc_inq_dimid(ncid, "lat", &latid); //Navy
	if (status != NC_NOERR) 
	{	// add new check if error for LON, LAT with extensions based on subset from LAS 1/29/09
		status = nc_inq_dimid(ncid, "LAT_UV", &latid);	if (status != NC_NOERR) {err = -1; goto LAS;}	// this is for SSH files which have 2 sets of lat,lon (LAT,LON is for SSH)
	}
	status = nc_inq_varid(ncid, "lat", &latvarid); //Navy
	if (status != NC_NOERR) 
	{
		status = nc_inq_varid(ncid, "LAT_UV", &latvarid);	if (status != NC_NOERR) {err = -1; goto done;}
	}
	status = nc_inq_dimlen(ncid, latid, &latLength);
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_inq_dimid(ncid, "lon", &lonid);	//Navy
	if (status != NC_NOERR) 
	{
		status = nc_inq_dimid(ncid, "LON_UV", &lonid);	
		if (status != NC_NOERR) {err = -1; goto done;}	// this is for SSH files which have 2 sets of lat,lon (LAT,LON is for SSH)
	}
	status = nc_inq_varid(ncid, "lon", &lonvarid);	//Navy
	if (status != NC_NOERR) 
	{
		status = nc_inq_varid(ncid, "LON_UV", &lonvarid);	
		if (status != NC_NOERR) {err = -1; goto done;}
	}
	status = nc_inq_dimlen(ncid, lonid, &lonLength);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	status = nc_inq_dimid(ncid, "depth", &depthid);	//3D
	if (status != NC_NOERR) 
	{
		status = nc_inq_dimid(ncid, "levels", &depthid);	//3D
		if (status != NC_NOERR) 
		{depthLength=1;/*err = -1; goto done;*/}	// surface data only
		else
		{
			status = nc_inq_varid(ncid, "depth_levels", &depthvarid); //Navy
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_inq_dimlen(ncid, depthid, &depthLength);
			if (status != NC_NOERR) {err = -1; goto done;}
			if (depthLength>1) fVar.gridType = MULTILAYER;
		}
	}
	else
	{
		status = nc_inq_varid(ncid, "depth", &depthvarid); //Navy
		if (status != NC_NOERR) {err = -1; goto done;}
		status = nc_inq_dimlen(ncid, depthid, &depthLength);
		if (status != NC_NOERR) {err = -1; goto done;}
		if (depthLength>1) fVar.gridType = MULTILAYER;
	}
	
LAS:
	// check number of dimensions - 2D or 3D
	// allow more flexibility with dimension names
	if (err)
	{
		Boolean bLASStyleNames = false, bHaveDepth = false;
		char latname[NC_MAX_NAME],levname[NC_MAX_NAME],lonname[NC_MAX_NAME],dimname[NC_MAX_NAME];
		err = 0;
		status = nc_inq_ndims(ncid, &numdims);
		if (status != NC_NOERR) {err = -1; goto done;}
		for (i=0;i<numdims;i++)
		{
			if (i == recid) continue;
			status = nc_inq_dimname(ncid,i,dimname);
			if (status != NC_NOERR) {err = -1; goto done;}
			if (strstrnocase(dimname,"LON"))
			{
				lonid = i; bLASStyleNames = true;
				strcpy(lonname,dimname);
			}
			if (strstrnocase(dimname,"LAT"))
			{
				latid = i; bLASStyleNames = true;
				strcpy(latname,dimname);
			}
			if (strstrnocase(dimname,"LEV"))
			{
				depthid = i; bHaveDepth = true;
				strcpy(levname,dimname);
			}
		}
		if (bLASStyleNames)
		{
			status = nc_inq_varid(ncid, latname, &latvarid); //Navy
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_inq_dimlen(ncid, latid, &latLength);
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_inq_varid(ncid, lonname, &lonvarid);
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_inq_dimlen(ncid, lonid, &lonLength);
			if (status != NC_NOERR) {err = -1; goto done;}
			if (bHaveDepth)
			{
				status = nc_inq_varid(ncid, levname, &depthvarid);
				if (status != NC_NOERR) {err = -1; goto done;}
				status = nc_inq_dimlen(ncid, depthid, &depthLength);
				if (status != NC_NOERR) {err = -1; goto done;}
				if (depthLength>1) fVar.gridType = MULTILAYER;
			}
			else
			{depthLength=1;/*err = -1; goto done;*/}	// surface data only
		}
		else
		{err = -1; goto done;}
		
	}
	
	pt_count[0] = latLength;
	pt_count[1] = lonLength;
	pt_count[2] = depthLength;
	
	lat_vals = new double[latLength]; 
	lon_vals = new double[lonLength]; 
	if (depthLength>1) {depthLevels = new double[depthLength]; if (!depthLevels) {err = memFullErr; goto done;}}
	if (!lat_vals || !lon_vals) {err = memFullErr; goto done;}
	status = nc_get_vara_double(ncid, latvarid, &ptIndex, &pt_count[0], lat_vals);
	if (status != NC_NOERR) {err=-1; goto done;}
	status = nc_get_vara_double(ncid, lonvarid, &ptIndex, &pt_count[1], lon_vals);
	if (status != NC_NOERR) {err=-1; goto done;}
	if (depthLength>1)
	{
		status = nc_get_vara_double(ncid, depthvarid, &ptIndex, &pt_count[2], depthLevels);
		if (status != NC_NOERR) {err=-1; goto done;}
		status = nc_get_att_double(ncid, depthvarid, "scale_factor", &scale_factor);
		if (status != NC_NOERR) {/*err = -1; goto done;*/}	// don't require scale factor
		
		fDepthLevelsHdl = (FLOATH)_NewHandleClear(depthLength * sizeof(float));
		if (!fDepthLevelsHdl) {err = memFullErr; goto done;}
		for (i=0;i<depthLength;i++)
		{
			INDEXH(fDepthLevelsHdl,i) = (float)depthLevels[i] * scale_factor;
		}
	}
	
	latIndex = 0;
	lonIndex = 0;
	
	status = nc_get_var1_double(ncid, latvarid, &latIndex, &startLat);
	if (status != NC_NOERR) {err=-1; goto done;}
	status = nc_get_var1_double(ncid, lonvarid, &lonIndex, &startLon);
	if (status != NC_NOERR) {err=-1; goto done;}
	latIndex = latLength-1;
	lonIndex = lonLength-1;
	status = nc_get_var1_double(ncid, latvarid, &latIndex, &endLat);
	if (status != NC_NOERR) {err=-1; goto done;}
	status = nc_get_var1_double(ncid, lonvarid, &lonIndex, &endLon);
	if (status != NC_NOERR) {err=-1; goto done;}
	
	status = nc_inq_dim(ncid, recid, recname, &recs);
	if (status != NC_NOERR) {err = -1; goto done;}
	fTimeHdl = (Seconds**)_NewHandleClear(recs*sizeof(Seconds));
	if (!fTimeHdl) {err = memFullErr; goto done;}
	for (i=0;i<recs;i++)
	{
		Seconds newTime;
		// possible units are, HOURS, MINUTES, SECONDS,...
		timeIndex = i;
		status = nc_get_var1_double(ncid, timeid, &timeIndex, &timeVal);
		if (status != NC_NOERR) {strcpy(errmsg,"Error reading times from file"); err = -1; goto done;}
		newTime = RoundDateSeconds(round(startTime2+timeVal*timeConversion));
		if (bStartTimeYearZero) newTime = RoundDateSeconds(round(startTime2+(timeVal-/*730500*/730487)*timeConversion));	// this is assuming time in days since year 1
		INDEXH(fTimeHdl,i) = newTime;	// which start time where?
		if (i==0) startTime = newTime;
	}

	dLat = (endLat - startLat) / (latLength - 1);
	dLon = (endLon - startLon) / (lonLength - 1);
	
	bounds.loLat = ((startLat-dLat/2.))*1e6;
	bounds.hiLat = ((endLat+dLat/2.))*1e6;
	
	if (startLon>180.)	// need to decide how to handle hawaii...
	{
		bounds.loLong = (((startLon-dLon/2.)-360.))*1e6;
		bounds.hiLong = (((endLon+dLon/2.)-360.))*1e6;
	}
	else
	{	// if endLon>180 ask user if he wants to shift
		/*if (endLon>180.)	// if endLon>180 ask user if he wants to shift (e.g. a Hawaii ncom subset might be 170 to 220, but bna is around -180)
		{
			short buttonSelected;
			buttonSelected  = MULTICHOICEALERT(1688,"Do you want to shift the latitudes by 360?",FALSE);
			switch(buttonSelected){
				case 1: // reset model start time
					bounds.loLong = (((startLon-dLon/2.)-360.))*1e6;
					bounds.hiLong = (((endLon+dLon/2.)-360.))*1e6;
					break;  
				case 3: // don't reset model start time
					bounds.loLong = ((startLon-dLon/2.))*1e6;
					bounds.hiLong = ((endLon+dLon/2.))*1e6;
					break;
				case 4: // cancel
					err=-1;// user cancel
					goto done;
			}
		}
		else*/
		{
			bounds.loLong = ((startLon-dLon/2.))*1e6;
			bounds.hiLong = ((endLon+dLon/2.))*1e6;
		}
	}
	rectGrid = new TRectGridVel;
	if (!rectGrid)
	{		
		err = true;
		TechError("Error in TimeGridVel::TextRead()","new TRectGridVel" ,err);
		goto done;
	}
	
	fNumRows = latLength;
	fNumCols = lonLength;
	fNumDepthLevels = depthLength;	//  here also do we want all depths?
	fGrid = (TGridVel*)rectGrid;
	
	rectGrid -> SetBounds(bounds); 
	this -> SetGridBounds(bounds); // setting the grid above is not working for the pyGNOME
	
	status = nc_close(ncid);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	
done:
	if (err)
	{
		if (!errmsg[0]) 
			strcpy(errmsg,"Error opening NetCDF file");
		printNote(errmsg);
		
		if(fGrid)
		{
			fGrid ->Dispose();
			delete fGrid;
			fGrid = 0;
		}
		if(fTimeHdl) {DisposeHandle((Handle)fTimeHdl); fTimeHdl=0;}
		if (fDepthLevelsHdl) {DisposeHandle((Handle)fDepthLevelsHdl); fDepthLevelsHdl=0;}
	}
	
	if (lat_vals) delete [] lat_vals;
	if (lon_vals) delete [] lon_vals;
	if (depthLevels) delete [] depthLevels;
	if (modelTypeStr) delete [] modelTypeStr;
	if (timeUnits) delete [] timeUnits;
	return err;
}

OSErr TimeGridVelRect_c::ReadTimeData(long index,VelocityFH *velocityH, char* errmsg) 
{
	OSErr err = 0;
	long i,j,k;
	char path[256], outPath[256]; 
	char *velUnits=0; 
	int status, ncid, numdims, uv_ndims, numvars;
	int curr_ucmp_id, curr_vcmp_id, depthid;
	static size_t curr_index[] = {0,0,0,0};
	static size_t curr_count[4];
	size_t velunit_len;
	double *curr_uvals=0,*curr_vvals=0, fill_value, velConversion=1.;
	long totalNumberOfVels = fNumRows * fNumCols;
	VelocityFH velH = 0;
	long latlength = fNumRows;
	long lonlength = fNumCols;
	long depthlength = fNumDepthLevels;	// code goes here, do we want all depths? maybe if movermap is a ptcur map??
	double scale_factor = 1.;
	Boolean bDepthIncluded = false;
	
	errmsg[0]=0;
	
	strcpy(path,fVar.pathName);
	if (!path || !path[0]) return -1;
	
	status = nc_open(path, NC_NOWRITE, &ncid);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	status = nc_inq_ndims(ncid, &numdims);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	if (numdims>=4)
	{	// code goes here, do we really want to use all the depths? 
		status = nc_inq_dimid(ncid, "depth", &depthid);	//3D
		if (status != NC_NOERR) 
		{
			status = nc_inq_dimid(ncid, "sigma", &depthid);	//3D - need to check sigma values in TextRead...
			if (status != NC_NOERR) bDepthIncluded = false;
			else bDepthIncluded = true;
		}
		else bDepthIncluded = true;
		// code goes here, might want to check other dimensions (lev), or just how many dimensions uv depend on
	}
	
	curr_index[0] = index;	// time 
	curr_count[0] = 1;	// take one at a time

	if (bDepthIncluded)
	{
		curr_count[1] = depthlength;	// depth
		curr_count[2] = latlength;
		curr_count[3] = lonlength;
	}
	else
	{
		curr_count[1] = latlength;	
		curr_count[2] = lonlength;
	}
	
	curr_uvals = new double[latlength*lonlength*depthlength]; 
	if(!curr_uvals) {TechError("TimeGridVel::ReadTimeData()", "new[]", 0); err = memFullErr; goto done;}
	curr_vvals = new double[latlength*lonlength*depthlength]; 
	if(!curr_vvals) {TechError("TimeGridVel::ReadTimeData()", "new[]", 0); err = memFullErr; goto done;}
	
	status = nc_inq_varid(ncid, "water_u", &curr_ucmp_id);
	if (status != NC_NOERR) 
	{
		status = nc_inq_varid(ncid, "curr_ucmp", &curr_ucmp_id); 
		if (status != NC_NOERR) 
		{
			status = nc_inq_varid(ncid, "u", &curr_ucmp_id); // allow u,v since so many people get confused
			if (status != NC_NOERR) {status = nc_inq_varid(ncid, "U", &curr_ucmp_id); if (status != NC_NOERR)	// ferret uses CAPS
			{err = -1; goto LAS;}}	// broader check for variable names coming out of LAS
		}	
	}
	status = nc_inq_varid(ncid, "water_v", &curr_vcmp_id);	// what if only input one at a time (u,v separate movers)?
	if (status != NC_NOERR) 
	{
		status = nc_inq_varid(ncid, "curr_vcmp", &curr_vcmp_id); 
		if (status != NC_NOERR) 
		{
			status = nc_inq_varid(ncid, "v", &curr_vcmp_id); // allow u,v since so many people get confused
			if (status != NC_NOERR) {status = nc_inq_varid(ncid, "V", &curr_vcmp_id); if (status != NC_NOERR)	// ferret uses CAPS
			{err = -1; goto done;}}
		}	
	}
	
LAS:
	if (err)
	{
		Boolean bLASStyleNames = false;
		char uname[NC_MAX_NAME],vname[NC_MAX_NAME],levname[NC_MAX_NAME],varname[NC_MAX_NAME];
		err = 0;
		status = nc_inq_nvars(ncid, &numvars);
		if (status != NC_NOERR) {err = -1; goto done;}
		for (i=0;i<numvars;i++)
		{
			//if (i == recid) continue;
			status = nc_inq_varname(ncid,i,varname);
			if (status != NC_NOERR) {err = -1; goto done;}
			if (varname[0]=='U' || varname[0]=='u' || strstrnocase(varname,"EVEL"))	// careful here, could end up with wrong u variable (like u_wind for example)
			{
				curr_ucmp_id = i; bLASStyleNames = true;
				strcpy(uname,varname);
			}
			if (varname[0]=='V' || varname[0]=='v' || strstrnocase(varname,"NVEL"))
			{
				curr_vcmp_id = i; bLASStyleNames = true;
				strcpy(vname,varname);
			}
			if (strstrnocase(varname,"LEV"))
			{
				depthid = i; bDepthIncluded = true;
				strcpy(levname,varname);
				curr_count[1] = depthlength;	// depth (set to 1)
				curr_count[2] = latlength;
				curr_count[3] = lonlength;
			}
		}
		if (!bLASStyleNames){err = -1; goto done;}
	}
	
	
	status = nc_inq_varndims(ncid, curr_ucmp_id, &uv_ndims);
	if (status==NC_NOERR){if (uv_ndims < numdims && uv_ndims==3) {curr_count[1] = latlength; curr_count[2] = lonlength;}}	// could have more dimensions than are used in u,v
	if (uv_ndims==4) {curr_count[1] = depthlength;curr_count[2] = latlength;curr_count[3] = lonlength;}
	status = nc_get_vara_double(ncid, curr_ucmp_id, curr_index, curr_count, curr_uvals);
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_get_vara_double(ncid, curr_vcmp_id, curr_index, curr_count, curr_vvals);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	
	status = nc_inq_attlen(ncid, curr_ucmp_id, "units", &velunit_len);
	if (status == NC_NOERR)
	{
		velUnits = new char[velunit_len+1];
		status = nc_get_att_text(ncid, curr_ucmp_id, "units", velUnits);
		if (status == NC_NOERR)
		{
			velUnits[velunit_len] = '\0'; 
			if (!strcmpnocase(velUnits,"cm/s") ||!strcmpnocase(velUnits,"Centimeters per second") )
				velConversion = .01;
			else if (!strcmpnocase(velUnits,"m/s"))
				velConversion = 1.0;
		}
	}
	
	
	status = nc_get_att_double(ncid, curr_ucmp_id, "_FillValue", &fill_value);	// should get this in text_read and store
	if (status != NC_NOERR) 
	{status = nc_get_att_double(ncid, curr_ucmp_id, "FillValue", &fill_value); 
		if (status != NC_NOERR) {status = nc_get_att_double(ncid, curr_ucmp_id, "missing_value", &fill_value); /*if (status != NC_NOERR) {err = -1; goto done;}*/ }}	// require fill value (took this out 12.12.08)
	
	if (_isnan(fill_value))
		fill_value=-99999;
	
	status = nc_get_att_double(ncid, curr_ucmp_id, "scale_factor", &scale_factor);
	if (status != NC_NOERR) {/*err = -1; goto done;*/}	// don't require scale factor
	
	status = nc_close(ncid);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	velH = (VelocityFH)_NewHandleClear(totalNumberOfVels * sizeof(VelocityFRec) * depthlength);
	if (!velH) {err = memFullErr; goto done;}
	for (k=0;k<depthlength;k++)
	{
		for (i=0;i<latlength;i++)
		{
			for (j=0;j<lonlength;j++)
			{
				if (curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]==fill_value)	// should store in current array and check before drawing or moving
					curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]=0.;
				if (curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]==fill_value)
					curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]=0.;

				if (_isnan(curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]))	// should store in current array and check before drawing or moving
					curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]=0.;
				if (_isnan(curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]))
					curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]=0.;

				INDEXH(velH,i*lonlength+j+k*fNumRows*fNumCols).u = (float)curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols] * velConversion;
				INDEXH(velH,i*lonlength+j+k*fNumRows*fNumCols).v = (float)curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols] * velConversion;
			}
		}
	}
	*velocityH = velH;
	fFillValue = fill_value;
	if (scale_factor!=1.) fVar.fileScaleFactor = scale_factor;
	
done:
	if (err)
	{
		strcpy(errmsg,"Error reading current data from NetCDF file");
		// We don't want to put up an error message here because it can lead to an infinite loop of messages.
		//printNote("Error opening NetCDF file");
		if(velH) {DisposeHandle((Handle)velH); velH = 0;}
	}
	if (curr_uvals) delete [] curr_uvals;
	if (curr_vvals) delete [] curr_vvals;
	if (velUnits) {delete [] velUnits;}
	return err;
}


OSErr TimeGridVel_c::SetInterval(char *errmsg, const Seconds& model_time)
{
	long timeDataInterval = 0;
	Boolean intervalLoaded = this -> CheckInterval(timeDataInterval, model_time);	
	long indexOfStart = timeDataInterval-1;
	long indexOfEnd = timeDataInterval;
	long numTimesInFile = this -> GetNumTimesInFile();
	OSErr err = 0;
	
	strcpy(errmsg,"");
	
	if(intervalLoaded) 
		return 0;
	
	//sprintf(errmsg,"Time interval = %ld\n",timeDataInterval);
	//printNote(errmsg);
	// check for constant current 
	if(numTimesInFile==1 && !(GetNumFiles()>1))	//or if(timeDataInterval==-1) 
	{
		indexOfStart = 0;
		indexOfEnd = UNASSIGNEDINDEX;	// should already be -1
	}
	
	if(timeDataInterval == 0 && fAllowExtrapolationInTime)
	{
		indexOfStart = 0;
		indexOfEnd = -1;
	}

	if(timeDataInterval == 0 || timeDataInterval == numTimesInFile /*|| (timeDataInterval==1 && fAllowExtrapolationInTime)*/)
	{	// before the first step in the file
		
		if (GetNumFiles()>1)
		{
			if ((err = CheckAndScanFile(errmsg, model_time)) || fOverLap) goto done;	
			
			intervalLoaded = this -> CheckInterval(timeDataInterval, model_time);	
			
			indexOfStart = timeDataInterval-1;
			indexOfEnd = timeDataInterval;
			numTimesInFile = this -> GetNumTimesInFile();
			if (fAllowExtrapolationInTime && (timeDataInterval==numTimesInFile || timeDataInterval == 0))
			{
				if(intervalLoaded) 
					return 0;
				indexOfEnd = -1;
				if (timeDataInterval == 0) indexOfStart = 0;	// if we allow extrapolation we need to load the first time
			}
		}
		else
		{
			if (fAllowExtrapolationInTime && timeDataInterval == numTimesInFile) 
			{
				fStartData.timeIndex = numTimesInFile-1;//check if time > last model time in all files
				fEndData.timeIndex = UNASSIGNEDINDEX;//check if time > last model time in all files
			}
			else if (fAllowExtrapolationInTime && timeDataInterval == 0) 
			{
				fStartData.timeIndex = 0;//check if time > last model time in all files
				fEndData.timeIndex = UNASSIGNEDINDEX;//check if time > last model time in all files
			}
			else
			{
				err = -1;
				strcpy(errmsg,"Time outside of interval being modeled");
				goto done;
			}
		}
		// code goes here, if time > last time in files allow user to continue
		// leave last two times loaded? move last time to start and nothing for end?
		// redefine as constant or just check if time > last time and some flag set
		// careful with timeAlpha, really just want to use the last time but needs to be loaded
		// want to check so that don't reload at every step, should recognize last time is ok
	}
	//else // load the two intervals
	{
		DisposeLoadedData(&fStartData);
		
		if(indexOfStart == fEndData.timeIndex) // passing into next interval
		{
			fStartData = fEndData;
			ClearLoadedData(&fEndData);
		}
		else
		{
			DisposeLoadedData(&fEndData);
		}
		
		//////////////////
		
		if(fStartData.dataHdl == 0 && indexOfStart >= 0) 
		{ // start data is not loaded
			err = this -> ReadTimeData(indexOfStart,&fStartData.dataHdl,errmsg);
			if(err) goto done;
			fStartData.timeIndex = indexOfStart;
		}	
		
		if(indexOfEnd < numTimesInFile && indexOfEnd != UNASSIGNEDINDEX)  // not past the last interval and not constant current
		{
			err = this -> ReadTimeData(indexOfEnd,&fEndData.dataHdl,errmsg);
			if(err) goto done;
			fEndData.timeIndex = indexOfEnd;
		}
	}
	
done:	
	if(err)
	{
		if(!errmsg[0])strcpy(errmsg,"Error in TimeGridVel::SetInterval()");
		DisposeLoadedData(&fStartData);
		DisposeLoadedData(&fEndData);
	}
	return err;
	
}


OSErr TimeGridVel_c::CheckAndScanFile(char *errmsg, const Seconds& model_time)
{
	Seconds time = model_time, startTime, endTime, lastEndTime, testTime, firstStartTime; 
	
	long i,numFiles = GetNumFiles();
	OSErr err = 0;
	
	errmsg[0]=0;
	if (fEndData.timeIndex!=UNASSIGNEDINDEX)
		testTime = (*fTimeHdl)[fEndData.timeIndex];	// currently loaded end time
	
	firstStartTime = (*fInputFilesHdl)[0].startTime + fTimeShift;
	for (i=0;i<numFiles;i++)
	{
		startTime = (*fInputFilesHdl)[i].startTime + fTimeShift;
		endTime = (*fInputFilesHdl)[i].endTime + fTimeShift;
		if (startTime<=time&&time<=endTime && !(startTime==endTime))
		{
			//if(fTimeHdl) {DisposeHandle((Handle)fTimeHdl); fTimeHdl=0;}
			DisposeTimeHdl();
			err = ScanFileForTimes((*fInputFilesHdl)[i].pathName,&fTimeHdl);	
			
			// code goes here, check that start/end times match
			strcpy(fVar.pathName,(*fInputFilesHdl)[i].pathName);
			fOverLap = false;
			return err;
		}
		if (startTime==endTime && startTime==time)	// one time in file, need to overlap
		{
			long fileNum;
			if (i<numFiles-1)
				fileNum = i+1;
			else
				fileNum = i;
			fOverLapStartTime = (*fInputFilesHdl)[fileNum-1].endTime;	// last entry in previous file
			DisposeLoadedData(&fStartData);
			/*if (fOverLapStartTime==testTime)	// shift end time data to start time data
			 {
			 fStartData = fEndData;
			 ClearLoadedData(&fEndData);
			 }
			 else*/
			{
				//if(fTimeHdl) {DisposeHandle((Handle)fTimeHdl); fTimeHdl=0;}
				DisposeTimeHdl();
				err = ScanFileForTimes((*fInputFilesHdl)[fileNum-1].pathName,&fTimeHdl);	
				
				DisposeLoadedData(&fEndData);
				strcpy(fVar.pathName,(*fInputFilesHdl)[fileNum-1].pathName);
				if (err = this -> ReadTimeData(GetNumTimesInFile()-1,&fStartData.dataHdl,errmsg)) return err;
			}
			fStartData.timeIndex = UNASSIGNEDINDEX;
			//if(fTimeHdl) {DisposeHandle((Handle)fTimeHdl); fTimeHdl=0;}
			DisposeTimeHdl();
			err = ScanFileForTimes((*fInputFilesHdl)[fileNum].pathName,&fTimeHdl);	
			
			strcpy(fVar.pathName,(*fInputFilesHdl)[fileNum].pathName);
			err = this -> ReadTimeData(0,&fEndData.dataHdl,errmsg);
			if(err) return err;
			fEndData.timeIndex = 0;
			fOverLap = true;
			return noErr;
		}
		if (i>0 && (lastEndTime<time && time<startTime))
		{
			fOverLapStartTime = (*fInputFilesHdl)[i-1].endTime;	// last entry in previous file
			DisposeLoadedData(&fStartData);
			if (fOverLapStartTime==testTime)	// shift end time data to start time data
			{
				fStartData = fEndData;
				ClearLoadedData(&fEndData);
			}
			else
			{
				//if(fTimeHdl) {DisposeHandle((Handle)fTimeHdl); fTimeHdl=0;}
				DisposeTimeHdl();
				err = ScanFileForTimes((*fInputFilesHdl)[i-1].pathName,&fTimeHdl);	
				
				DisposeLoadedData(&fEndData);
				strcpy(fVar.pathName,(*fInputFilesHdl)[i-1].pathName);
				if (err = this -> ReadTimeData(GetNumTimesInFile()-1,&fStartData.dataHdl,errmsg)) return err;	
			}
			fStartData.timeIndex = UNASSIGNEDINDEX;
			//if(fTimeHdl) {DisposeHandle((Handle)fTimeHdl); fTimeHdl=0;}
			DisposeTimeHdl();
			err = ScanFileForTimes((*fInputFilesHdl)[i].pathName,&fTimeHdl);	
			
			strcpy(fVar.pathName,(*fInputFilesHdl)[i].pathName);
			err = this -> ReadTimeData(0,&fEndData.dataHdl,errmsg);
			if(err) return err;
			fEndData.timeIndex = 0;
			fOverLap = true;
			return noErr;
		}
		lastEndTime = endTime;
	}
	if (fAllowExtrapolationInTime && time > lastEndTime)
	{
		//if(fTimeHdl) {DisposeHandle((Handle)fTimeHdl); fTimeHdl=0;}
		DisposeTimeHdl();
		err = ScanFileForTimes((*fInputFilesHdl)[numFiles-1].pathName,&fTimeHdl);	
		
		// code goes here, check that start/end times match
		strcpy(fVar.pathName,(*fInputFilesHdl)[numFiles-1].pathName);
		fOverLap = false;
		return err;
	}
	if (fAllowExtrapolationInTime && time < firstStartTime)
	{
		//if(fTimeHdl) {DisposeHandle((Handle)fTimeHdl); fTimeHdl=0;}
		DisposeTimeHdl();
		err = ScanFileForTimes((*fInputFilesHdl)[0].pathName,&fTimeHdl);	
		
		// code goes here, check that start/end times match
		strcpy(fVar.pathName,(*fInputFilesHdl)[0].pathName);
		fOverLap = false;
		return err;
	}
	strcpy(errmsg,"Time outside of interval being modeled");
	return -1;	
}

//OSErr TimeGridVel_c::ScanFileForTimes(char *path,Seconds ***timeH,Boolean setStartTime)
OSErr ScanFileForTimes(char *path,Seconds ***timeH)
{
	OSErr err = 0;
	long i,numScanned,line=0;
	DateTimeRec time;
	Seconds timeSeconds;
	char s[1024], outPath[256];
	CHARH fileBufH = 0;
	int status, ncid, recid, timeid;
	size_t recs, t_len, t_len2;
	double timeVal;
	char recname[NC_MAX_NAME], *timeUnits=0;	
	static size_t timeIndex;
	Seconds startTime2;
	double timeConversion = 1.;
	char errmsg[256] = "";
	Seconds **timeHdl = 0;
	
	status = nc_open(path, NC_NOWRITE, &ncid);
	// code goes here, will need to resolve file paths to unix paths in readinputfilenames
	if (status != NC_NOERR) /*{err = -1; goto done;}*/
	{
#if TARGET_API_MAC_CARBON
		err = ConvertTraditionalPathToUnixPath((const char *) path, outPath, kMaxNameLen) ;
		status = nc_open(outPath, NC_NOWRITE, &ncid);
#endif
		if (status != NC_NOERR) {err = -1; goto done;}
	}
	
	status = nc_inq_dimid(ncid, "time", &recid); 
	if (status != NC_NOERR) 
	{
		status = nc_inq_unlimdim(ncid, &recid);	// maybe time is unlimited dimension
		if (status != NC_NOERR) {err = -1; goto done;}
	}
	
	status = nc_inq_varid(ncid, "time", &timeid); 
	if (status != NC_NOERR) {err = -1; goto done;} 
	
	/////////////////////////////////////////////////
	status = nc_inq_attlen(ncid, timeid, "units", &t_len);
	if (status != NC_NOERR) 
	{
		err = -1; goto done;
	}
	else
	{
		DateTimeRec time;
		char unitStr[24], junk[10];
		
		timeUnits = new char[t_len+1];
		status = nc_get_att_text(ncid, timeid, "units", timeUnits);
		if (status != NC_NOERR) {err = -2; goto done;} 
		timeUnits[t_len] = '\0'; // moved this statement before StringSubstitute, JLM 5/2/10
		StringSubstitute(timeUnits, ':', ' ');
		StringSubstitute(timeUnits, '-', ' ');
		
		numScanned=sscanf(timeUnits, "%s %s %hd %hd %hd %hd %hd %hd",
						  unitStr, junk, &time.year, &time.month, &time.day,
						  &time.hour, &time.minute, &time.second) ;
		if (numScanned==5)	
		{time.hour = 0; time.minute = 0; time.second = 0; }
		else if (numScanned==7)	time.second = 0;
		else if (numScanned<8)	
			//if (numScanned!=8)	
		{ err = -1; TechError("TimeGridVel::ScanFileForTimes()", "sscanf() == 8", 0); goto done; }
		DateToSeconds (&time, &startTime2);	// code goes here, which start Time to use ??
		if (!strcmpnocase(unitStr,"HOURS") || !strcmpnocase(unitStr,"HOUR"))
			timeConversion = 3600.;
		else if (!strcmpnocase(unitStr,"MINUTES") || !strcmpnocase(unitStr,"MINUTE"))
			timeConversion = 60.;
		else if (!strcmpnocase(unitStr,"SECONDS") || !strcmpnocase(unitStr,"SECOND"))
			timeConversion = 1.;
		else if (!strcmpnocase(unitStr,"DAYS") || !strcmpnocase(unitStr,"DAY"))
			timeConversion = 24*3600.;
	} 
	
	status = nc_inq_dim(ncid, recid, recname, &recs);
	if (status != NC_NOERR) {err = -2; goto done;}
	timeHdl = (Seconds**)_NewHandleClear(recs*sizeof(Seconds));
	if (!timeHdl) {err = memFullErr; goto done;}
	for (i=0;i<recs;i++)
	{
		Seconds newTime;
		// possible units are, HOURS, MINUTES, SECONDS,...
		timeIndex = i;
		status = nc_get_var1_double(ncid, timeid, &timeIndex, &timeVal);
		if (status != NC_NOERR) {strcpy(errmsg,"Error reading times from NetCDF file"); err = -2; goto done;}
		newTime = RoundDateSeconds(round(startTime2+timeVal*timeConversion));
		INDEXH(timeHdl,i) = newTime;	// which start time where?
	}
	*timeH = timeHdl;
	status = nc_close(ncid);
	if (status != NC_NOERR) {err = -2; goto done;}
	
	
done:
	if (err)
	{
		if (err==-2) {printError("Error reading times from NetCDF file");}
		if (timeHdl) {DisposeHandle((Handle)timeHdl); timeHdl=0;}
	}
	return err;
}

long TimeGridVelRect_c::GetNumDepthLevels()
{
	long numDepthLevels = 0;
	
	if (fDepthLevelsHdl) numDepthLevels = _GetHandleSize((Handle)fDepthLevelsHdl)/sizeof(**fDepthLevelsHdl);
	else
	{
		long numDepthLevels = 0;
		OSErr err = 0;
		char path[256], outPath[256];
		int status, ncid, sigmaid, sigmavarid;
		size_t sigmaLength=0;
		strcpy(path,fVar.pathName);
		if (!path || !path[0]) return -1;
		
		status = nc_open(path, NC_NOWRITE, &ncid);
		if (status != NC_NOERR) 
		{
#if TARGET_API_MAC_CARBON
			err = ConvertTraditionalPathToUnixPath((const char *) path, outPath, kMaxNameLen) ;
			status = nc_open(outPath, NC_NOWRITE, &ncid);
#endif
			if (status != NC_NOERR) {err = -1; return -1;}
		}
		status = nc_inq_dimid(ncid, "sigma", &sigmaid); 	
		if (status != NC_NOERR) 
		{
			numDepthLevels = 1;	// check for zgrid option here
		}	
		else
		{
			status = nc_inq_varid(ncid, "sigma", &sigmavarid); //Navy
			if (status != NC_NOERR) {numDepthLevels = 1;}	// require variable to match the dimension
			status = nc_inq_dimlen(ncid, sigmaid, &sigmaLength);
			if (status != NC_NOERR) {numDepthLevels = 1;}	// error in file
			numDepthLevels = sigmaLength;
		}
	}
	return numDepthLevels;     
}

long TimeGridVelRect_c::GetNumDepths(void)
{
	long numDepths = 0;
	if (fDepthsH) numDepths = _GetHandleSize((Handle)fDepthsH)/sizeof(**fDepthsH);
	
	return numDepths;
}

VelocityRec TimeGridVelRect_c::GetScaledPatValue(const Seconds& model_time, WorldPoint3D refPoint)
{	// pull out the getpatval part
	double timeAlpha, depthAlpha;
	float topDepth, bottomDepth;
	long index; 
	long depthIndex1,depthIndex2;	// default to -1?
	Seconds startTime,endTime;
	char errmsg[256];
	
	VelocityRec	scaledPatVelocity = {0.,0.};
	OSErr err = 0;
	
	index = GetVelocityIndex(refPoint.p);  // regular grid
	
	if (refPoint.z>0 && fVar.gridType==TWO_D)
	{		
		if (fAllowVerticalExtrapolationOfCurrents && fMaxDepthForExtrapolation >= refPoint.z) 
		{
			// fall through to get the velocity
		}
		else
		{	// may allow 3D currents later
			return scaledPatVelocity; 
		}
	}
	
	GetDepthIndices(0,refPoint.z,&depthIndex1,&depthIndex2);
	if (depthIndex2!=UNASSIGNEDINDEX)
	{
		// Calculate the depth weight factor
		topDepth = INDEXH(fDepthLevelsHdl,depthIndex1);
		bottomDepth = INDEXH(fDepthLevelsHdl,depthIndex2);
		//depthAlpha = (bottomDepth - arrowDepth)/(double)(bottomDepth - topDepth);
		depthAlpha = (bottomDepth - refPoint.z)/(double)(bottomDepth - topDepth);
	}
	
	// Check for constant current 
	if((GetNumTimesInFile()==1 && !(GetNumFiles()>1)) || (fEndData.timeIndex == UNASSIGNEDINDEX && model_time > ((*fTimeHdl)[fStartData.timeIndex] + fTimeShift) && fAllowExtrapolationInTime) || (fEndData.timeIndex == UNASSIGNEDINDEX && model_time < ((*fTimeHdl)[fStartData.timeIndex] + fTimeShift) && fAllowExtrapolationInTime))
	{
		// Calculate the interpolated velocity at the point
		if (index >= 0) 
		{
			if(depthIndex2==UNASSIGNEDINDEX) // surface velocity or special cases
			{
				scaledPatVelocity.u = INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).u;
				scaledPatVelocity.v = INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).v;
			}
			else
			{
				scaledPatVelocity.u = depthAlpha*INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).u+(1-depthAlpha)*INDEXH(fStartData.dataHdl,index+depthIndex2*fNumRows*fNumCols).u;
				scaledPatVelocity.v = depthAlpha*INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).v+(1-depthAlpha)*INDEXH(fStartData.dataHdl,index+depthIndex2*fNumRows*fNumCols).v;
			}
		}
		else	// set vel to zero
		{
			scaledPatVelocity.u = 0.;
			scaledPatVelocity.v = 0.;
		}
	}
	else // time varying current 
	{
		// Calculate the time weight factor
		if (GetNumFiles()>1 && fOverLap)
			startTime = fOverLapStartTime + fTimeShift;
		else
			startTime = (*fTimeHdl)[fStartData.timeIndex] + fTimeShift;
		endTime = (*fTimeHdl)[fEndData.timeIndex] + fTimeShift;
		timeAlpha = (endTime - model_time)/(double)(endTime - startTime);
		
		// Calculate the interpolated velocity at the point
		if (index >= 0) 
		{
			if(depthIndex2==UNASSIGNEDINDEX) // surface velocity or special cases
			{
				scaledPatVelocity.u = timeAlpha*INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index+depthIndex1*fNumRows*fNumCols).u;
				scaledPatVelocity.v = timeAlpha*INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index+depthIndex1*fNumRows*fNumCols).v;
			}
			else	// below surface velocity
			{
				scaledPatVelocity.u = depthAlpha*(timeAlpha*INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index+depthIndex1*fNumRows*fNumCols).u);
				scaledPatVelocity.u += (1-depthAlpha)*(timeAlpha*INDEXH(fStartData.dataHdl,index+depthIndex2*fNumRows*fNumCols).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index+depthIndex2*fNumRows*fNumCols).u);
				scaledPatVelocity.v = depthAlpha*(timeAlpha*INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index+depthIndex1*fNumRows*fNumCols).v);
				scaledPatVelocity.v += (1-depthAlpha)*(timeAlpha*INDEXH(fStartData.dataHdl,index+depthIndex2*fNumRows*fNumCols).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index+depthIndex2*fNumRows*fNumCols).v);
			}
		}
		else	// set vel to zero
		{
			scaledPatVelocity.u = 0.;
			scaledPatVelocity.v = 0.;
		}
	}
	
scale:

	//scaledPatVelocity.u *= fVar.curScale; 
	//scaledPatVelocity.v *= fVar.curScale; // apply this on the outside
	scaledPatVelocity.u *= fVar.fileScaleFactor; 
	scaledPatVelocity.v *= fVar.fileScaleFactor; 
	
	
	return scaledPatVelocity;
}

Seconds TimeGridVel_c::GetTimeValue(long index)
{
	if (index<0) printError("Access violation in TimeGridVel_c::GetTimeValue()");
	Seconds time = (*fTimeHdl)[index] + fTimeShift;
	return time;
}

Seconds TimeGridVel_c::GetStartTimeValue(long index)
{
	if (index<0 || !fInputFilesHdl) printError("Access violation in TimeGridVel_c::GetStartTimeValue()");
	Seconds time = (*fInputFilesHdl)[index].startTime + fTimeShift;	
	return time;
}

long TimeGridVel_c::GetVelocityIndex(WorldPoint p) 
{
	long rowNum, colNum;
	double dRowNum, dColNum;
	VelocityRec	velocity;
	char errmsg[256];

	LongRect		gridLRect, geoRect;
	ScaleRec		thisScaleRec;
	
	// for some reason this is getting garbled in pyGNOME
	//TRectGridVel* rectGrid = dynamic_cast<TRectGridVel*>(fGrid);	// fNumRows, fNumCols members of TimeGridVel_c
	
	//WorldRect bounds = rectGrid->GetBounds();
	WorldRect bounds = this->GetGridBounds();

	SetLRect (&gridLRect, 0, fNumRows, fNumCols, 0);
	SetLRect (&geoRect, bounds.loLong, bounds.loLat, bounds.hiLong, bounds.hiLat);	
	GetLScaleAndOffsets (&geoRect, &gridLRect, &thisScaleRec);
	
	dColNum = (p.pLong * thisScaleRec.XScale + thisScaleRec.XOffset) -.5;
	dRowNum = (p.pLat  * thisScaleRec.YScale + thisScaleRec.YOffset) -.5;

	colNum = round(dColNum);
	rowNum = round(dRowNum);
	
	if (colNum < 0 || colNum >= fNumCols || rowNum < 0 || rowNum >= fNumRows)
		
	{ return -1; }
	
	return rowNum * fNumCols + colNum;
}

// this is only used for VelocityStrAtPoint which is in the gui code
LongPoint TimeGridVel_c::GetVelocityIndices(WorldPoint p) 
{
	long rowNum, colNum;
	double dRowNum, dColNum;
	LongPoint indices = {-1,-1};
	VelocityRec	velocity;
	
	LongRect		gridLRect, geoRect;
	ScaleRec		thisScaleRec;
	
	TRectGridVel* rectGrid = dynamic_cast<TRectGridVel*>(fGrid);	// fNumRows, fNumCols members of TimeGridVel_c
	
	WorldRect bounds = rectGrid->GetBounds();
	
	SetLRect (&gridLRect, 0, fNumRows, fNumCols, 0);
	SetLRect (&geoRect, bounds.loLong, bounds.loLat, bounds.hiLong, bounds.hiLat);	
	GetLScaleAndOffsets (&geoRect, &gridLRect, &thisScaleRec);
	
	dColNum = round((p.pLong * thisScaleRec.XScale + thisScaleRec.XOffset) -.5);
	dRowNum = round((p.pLat  * thisScaleRec.YScale + thisScaleRec.YOffset) -.5);

	colNum = dColNum;
	rowNum = dRowNum;
	
	if (colNum < 0 || colNum >= fNumCols || rowNum < 0 || rowNum >= fNumRows)
		
	{ return indices; }

	indices.h = colNum;
	indices.v = rowNum;
	return indices;
}


/////////////////////////////////////////////////
double TimeGridVel_c::GetStartUVelocity(long index)
{	// 
	double u = 0;
	if (index>=0)
	{
		if (fStartData.dataHdl) u = INDEXH(fStartData.dataHdl,index).u;
		if (u==fFillValue) u = 0;
	}
	return u;
}

double TimeGridVel_c::GetEndUVelocity(long index)
{
	double u = 0;
	if (index>=0)
	{
		if (fEndData.dataHdl) u = INDEXH(fEndData.dataHdl,index).u;
		if (u==fFillValue) u = 0;
	}
	return u;
}

double TimeGridVel_c::GetStartVVelocity(long index)
{
	double v = 0;
	if (index >= 0)
	{
		if (fStartData.dataHdl) v = INDEXH(fStartData.dataHdl,index).v;
		if (v==fFillValue) v = 0;
	}
	return v;
}

double TimeGridVel_c::GetEndVVelocity(long index)
{
	double v = 0;
	if (index >= 0)
	{
		if (fEndData.dataHdl) v = INDEXH(fEndData.dataHdl,index).v;
		if (v==fFillValue) v = 0;
	}
	return v;
}

OSErr TimeGridVel_c::GetStartTime(Seconds *startTime)
{
	OSErr err = 0;
	*startTime = 0;
	if (fStartData.timeIndex != UNASSIGNEDINDEX)
		*startTime = (*fTimeHdl)[fStartData.timeIndex] + fTimeShift;
	else return -1;
	return 0;
}

OSErr TimeGridVel_c::GetEndTime(Seconds *endTime)
{
	OSErr err = 0;
	*endTime = 0;
	if (fEndData.timeIndex != UNASSIGNEDINDEX)
		*endTime = (*fTimeHdl)[fEndData.timeIndex] + fTimeShift;
	else return -1;
	return 0;
}

double TimeGridVelRect_c::GetDepthAtIndex(long depthIndex, double totalDepth)
{	// really can combine and use GetDepthAtIndex - could move to base class
	double depth = 0;
	float sc_r, Cs_r;
	if (fVar.gridType == SIGMA_ROMS)
	{
		sc_r = INDEXH(fDepthLevelsHdl,depthIndex);
		Cs_r = INDEXH(fDepthLevelsHdl2,depthIndex);
		depth = abs(totalDepth*(hc*sc_r+totalDepth*Cs_r))/(totalDepth+hc);
	}
	else
		depth = INDEXH(fDepthLevelsHdl,depthIndex)*totalDepth; // times totalDepth
	
	return depth;
}

float TimeGridVelRect_c::GetMaxDepth()
{
	float maxDepth = 0;
	if (fDepthsH)
	{
		float depth=0;
		long i,numDepths = _GetHandleSize((Handle)fDepthsH)/sizeof(**fDepthsH);
		for (i=0;i<numDepths;i++)
		{
			depth = INDEXH(fDepthsH,i);
			if (depth > maxDepth) 
				maxDepth = depth;
		}
		return maxDepth;
	}
	else
	{
		long numDepthLevels = GetNumDepthLevelsInFile();
		if (numDepthLevels<=0) return maxDepth;
		if (fDepthLevelsHdl) maxDepth = INDEXH(fDepthLevelsHdl,numDepthLevels-1);
	}
	return maxDepth;
}

float TimeGridVelRect_c::GetTotalDepth(WorldPoint wp, long triNum)
{	// z grid only 
#pragma unused(wp)
#pragma unused(triNum)
	long numDepthLevels = GetNumDepthLevelsInFile();
	float totalDepth = 0;

	if (fDepthLevelsHdl && numDepthLevels>0) totalDepth = INDEXH(fDepthLevelsHdl,numDepthLevels-1);
	return totalDepth;
}

void TimeGridVelRect_c::GetDepthIndices(long ptIndex, float depthAtPoint, long *depthIndex1, long *depthIndex2)
{
	long indexToDepthData = 0;
	long numDepthLevels = GetNumDepthLevelsInFile();
	float totalDepth = 0;
	
	
	if (fDepthLevelsHdl && numDepthLevels>0) totalDepth = INDEXH(fDepthLevelsHdl,numDepthLevels-1);
	else
	{
		*depthIndex1 = indexToDepthData;
		*depthIndex2 = UNASSIGNEDINDEX;
		return;
	}

	if (depthAtPoint <= totalDepth) // check data exists at chosen/LE depth for this point
	{
		long j;
		for(j=0;j<numDepthLevels-1;j++)
		{
			if(INDEXH(fDepthLevelsHdl,indexToDepthData+j)<depthAtPoint &&
			   depthAtPoint<=INDEXH(fDepthLevelsHdl,indexToDepthData+j+1))
			{
				*depthIndex1 = indexToDepthData+j;
				*depthIndex2 = indexToDepthData+j+1;
			}
			else if(INDEXH(fDepthLevelsHdl,indexToDepthData+j)==depthAtPoint)
			{
				*depthIndex1 = indexToDepthData+j;
				*depthIndex2 = UNASSIGNEDINDEX;
			}
		}
		if(INDEXH(fDepthLevelsHdl,indexToDepthData)==depthAtPoint)	// handles single depth case
		{
			*depthIndex1 = indexToDepthData;
			*depthIndex2 = UNASSIGNEDINDEX;
		}
		else if(INDEXH(fDepthLevelsHdl,indexToDepthData+numDepthLevels-1)<depthAtPoint)
		{
			*depthIndex1 = indexToDepthData+numDepthLevels-1;
			*depthIndex2 = UNASSIGNEDINDEX; //BOTTOM, for now just extrapolate lowest depth value (at bottom case?)
		}
		else if(INDEXH(fDepthLevelsHdl,indexToDepthData)>depthAtPoint)
		{
			*depthIndex1 = indexToDepthData;
			*depthIndex2 = UNASSIGNEDINDEX; //TOP, for now just extrapolate highest depth value
		}
	}
	else // no data at this point
	{
		*depthIndex1 = UNASSIGNEDINDEX;
		*depthIndex2 = UNASSIGNEDINDEX;
	}
}


TimeGridVelRect_c::TimeGridVelRect_c () : TimeGridVel_c()
{
	fDepthLevelsHdl = 0;	// depth level, sigma, or sc_r
	fDepthLevelsHdl2 = 0;	// Cs_r
	hc = 1.;	// what default?
	
	fIsNavy = false;	
	
	memset(&fStartData,0,sizeof(fStartData));
	fStartData.timeIndex = UNASSIGNEDINDEX; 
	fStartData.dataHdl = 0; 
	memset(&fEndData,0,sizeof(fEndData));
	fEndData.timeIndex = UNASSIGNEDINDEX;
	fEndData.dataHdl = 0;
	
	fDepthsH = 0;
	fDepthDataInfo = 0;
	
	fNumDepthLevels = 1;	// default surface current only
	
	fAllowVerticalExtrapolationOfCurrents = false;
	fMaxDepthForExtrapolation = 0.;	// assume 2D is just surface
	
}


TimeGridVelCurv_c::TimeGridVelCurv_c () : TimeGridVelRect_c()
{
	fVerdatToNetCDFH = 0;	
	fVertexPtsH = 0;
	//bIsCOOPSWaterMask = false;
	bVelocitiesOnNodes = false;
}	

LongPointHdl TimeGridVelCurv_c::GetPointsHdl()
{
	return (dynamic_cast<TTriGridVel*>(fGrid)) -> GetPointsHdl();
	//return ((TTriGridVel*)fGrid) -> GetPointsHdl();
}

long TimeGridVelCurv_c::GetVelocityIndex(WorldPoint wp)
{
	long index = -1;
	if (fGrid) 
	{
		// for now just use the u,v at left and bottom midpoints of grid box as velocity over entire gridbox
		if (bVelocitiesOnNodes)
			index = (dynamic_cast<TTriGridVel*>(fGrid))->GetRectIndexFromTriIndex(wp,fVerdatToNetCDFH,fNumCols);// curvilinear grid
		else
			index = (dynamic_cast<TTriGridVel*>(fGrid))->GetRectIndexFromTriIndex(wp,fVerdatToNetCDFH,fNumCols+1);// curvilinear grid
	}
	return index;
}

// this is only used for VelocityStrAtPoint which is in the gui code
LongPoint TimeGridVelCurv_c::GetVelocityIndices(WorldPoint wp)
{
	LongPoint indices={-1,-1};
	if (fGrid) 
	{
		// for now just use the u,v at left and bottom midpoints of grid box as velocity over entire gridbox
		if (bVelocitiesOnNodes)
			indices = (dynamic_cast<TTriGridVel*>(fGrid))->GetRectIndicesFromTriIndex(wp,fVerdatToNetCDFH,fNumCols);// curvilinear grid
		else
			indices = (dynamic_cast<TTriGridVel*>(fGrid))->GetRectIndicesFromTriIndex(wp,fVerdatToNetCDFH,fNumCols+1);// curvilinear grid
	}
	return indices;
}

VelocityRec TimeGridVelCurv_c::GetScaledPatValue(const Seconds& model_time, WorldPoint3D refPoint)
{	
	double timeAlpha, depthAlpha, depth = refPoint.z;
	float topDepth, bottomDepth;
	long index = -1, depthIndex1, depthIndex2; 
	float totalDepth; 
	Seconds startTime,endTime;
	VelocityRec scaledPatVelocity = {0.,0.};
	InterpolationVal interpolationVal;
	OSErr err = 0;
	
	if (fGrid) 
	{
		if (bVelocitiesOnNodes)
		{
			//index = ((TTriGridVel*)fGrid)->GetRectIndexFromTriIndex(refPoint,fVerdatToNetCDFH,fNumCols);// curvilinear grid
			interpolationVal = fGrid -> GetInterpolationValues(refPoint.p);
			if (interpolationVal.ptIndex1<0) return scaledPatVelocity;
			//ptIndex1 =  (*fVerdatToNetCDFH)[interpolationVal.ptIndex1];	
			//ptIndex2 =  (*fVerdatToNetCDFH)[interpolationVal.ptIndex2];
			//ptIndex3 =  (*fVerdatToNetCDFH)[interpolationVal.ptIndex3];
			index = (*fVerdatToNetCDFH)[interpolationVal.ptIndex1];
		}
		else // for now just use the u,v at left and bottom midpoints of grid box as velocity over entire gridbox
			index = (dynamic_cast<TTriGridVel*>(fGrid))->GetRectIndexFromTriIndex(refPoint.p,fVerdatToNetCDFH,fNumCols+1);// curvilinear grid
	}
	if (index < 0) return scaledPatVelocity;
	
	totalDepth = GetTotalDepth(refPoint.p,index);
	if (index>=0)
		GetDepthIndices(index,depth,totalDepth,&depthIndex1,&depthIndex2);	// if not ?? point is off grid but not beached (map mismatch)
	else 
		return scaledPatVelocity;
	if (depthIndex1==UNASSIGNEDINDEX && depthIndex2==UNASSIGNEDINDEX)
		return scaledPatVelocity;	// no value for this point at chosen depth - should this be an error? question of show currents below surface vs an actual LE moving
	
	if (depthIndex2!=UNASSIGNEDINDEX)
	{
		// Calculate the depth weight factor
		topDepth = GetDepthAtIndex(depthIndex1,totalDepth); // times totalDepth
		bottomDepth = GetDepthAtIndex(depthIndex2,totalDepth);
		if (totalDepth == 0) depthAlpha = 1;
		else
			depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
	}
	
	// Check for constant current 
	if((GetNumTimesInFile()==1 && !(GetNumFiles()>1)) || (fEndData.timeIndex == UNASSIGNEDINDEX && model_time > ((*fTimeHdl)[fStartData.timeIndex] + fTimeShift) && fAllowExtrapolationInTime) || (fEndData.timeIndex == UNASSIGNEDINDEX && model_time < ((*fTimeHdl)[fStartData.timeIndex] + fTimeShift) && fAllowExtrapolationInTime))
		//if(GetNumTimesInFile()==1)
	{
		// Calculate the interpolated velocity at the point
		if (index >= 0 && depthIndex1 >= 0) 
		{
			//scaledPatVelocity.u = INDEXH(fStartData.dataHdl,index).u;
			//scaledPatVelocity.v = INDEXH(fStartData.dataHdl,index).v;
			if(depthIndex2==UNASSIGNEDINDEX) // surface velocity or special cases
			{
				scaledPatVelocity.u = INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).u;
				scaledPatVelocity.v = INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).v;
			}
			else
			{
				scaledPatVelocity.u = depthAlpha*INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).u+(1-depthAlpha)*INDEXH(fStartData.dataHdl,index+depthIndex2*fNumRows*fNumCols).u;
				scaledPatVelocity.v = depthAlpha*INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).v+(1-depthAlpha)*INDEXH(fStartData.dataHdl,index+depthIndex2*fNumRows*fNumCols).v;
			}
		}
		else	// set vel to zero
		{
			scaledPatVelocity.u = 0.;
			scaledPatVelocity.v = 0.;
		}
	}
	else // time varying current 
	{
		// Calculate the time weight factor
		if (GetNumFiles()>1 && fOverLap)
			startTime = fOverLapStartTime + fTimeShift;
		else
			startTime = (*fTimeHdl)[fStartData.timeIndex] + fTimeShift;
		//startTime = (*fTimeHdl)[fStartData.timeIndex] + fTimeShift;
		endTime = (*fTimeHdl)[fEndData.timeIndex] + fTimeShift;
		timeAlpha = (endTime - model_time)/(double)(endTime - startTime);
		
		// Calculate the interpolated velocity at the point
		if (index >= 0 && depthIndex1 >= 0) 
		{
			//scaledPatVelocity.u = timeAlpha*INDEXH(fStartData.dataHdl,index).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index).u;
			//scaledPatVelocity.v = timeAlpha*INDEXH(fStartData.dataHdl,index).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index).v;
			if(depthIndex2==UNASSIGNEDINDEX) // surface velocity or special cases
			{
				scaledPatVelocity.u = timeAlpha*INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index+depthIndex1*fNumRows*fNumCols).u;
				scaledPatVelocity.v = timeAlpha*INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index+depthIndex1*fNumRows*fNumCols).v;
			}
			else	// below surface velocity
			{
				scaledPatVelocity.u = depthAlpha*(timeAlpha*INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index+depthIndex1*fNumRows*fNumCols).u);
				scaledPatVelocity.u += (1-depthAlpha)*(timeAlpha*INDEXH(fStartData.dataHdl,index+depthIndex2*fNumRows*fNumCols).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index+depthIndex2*fNumRows*fNumCols).u);
				scaledPatVelocity.v = depthAlpha*(timeAlpha*INDEXH(fStartData.dataHdl,index+depthIndex1*fNumRows*fNumCols).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index+depthIndex1*fNumRows*fNumCols).v);
				scaledPatVelocity.v += (1-depthAlpha)*(timeAlpha*INDEXH(fStartData.dataHdl,index+depthIndex2*fNumRows*fNumCols).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index+depthIndex2*fNumRows*fNumCols).v);
			}
		}
		else	// set vel to zero
		{
			scaledPatVelocity.u = 0.;
			scaledPatVelocity.v = 0.;
		}
	}
	
scale:
	
	//scaledPatVelocity.u *= fVar.curScale; // is there a dialog scale factor?
	//scaledPatVelocity.v *= fVar.curScale; 
	scaledPatVelocity.u *= fVar.fileScaleFactor; // may want to allow some sort of scale factor, though should be in file
	scaledPatVelocity.v *= fVar.fileScaleFactor; 
			
	return scaledPatVelocity;
}


float TimeGridVelCurv_c::GetTotalDepthFromTriIndex(long triNum)
{
	long index1, index2, index3, index4, numDepths;
	OSErr err = 0;
	float totalDepth = 0;
	Boolean useTriNum = true;
	WorldPoint refPoint = {0.,0.};
	
	if (fVar.gridType == SIGMA_ROMS)	// should always be true
	{
		//if (triNum < 0) useTriNum = false;
		err = (dynamic_cast<TTriGridVel*>(fGrid))->GetRectCornersFromTriIndexOrPoint(&index1, &index2, &index3, &index4, refPoint, triNum, useTriNum, fVerdatToNetCDFH, fNumCols+1);
		
		if (err) return 0;
		if (fDepthsH)
		{	// issue with extended grid not having depths - probably need to rework that idea
			long numCorners = 4;
			numDepths = _GetHandleSize((Handle)fDepthsH)/sizeof(**fDepthsH);
			if (index1<numDepths && index1>=0) totalDepth += INDEXH(fDepthsH,index1); else numCorners--;
			if (index2<numDepths && index2>=0) totalDepth += INDEXH(fDepthsH,index2); else numCorners--;
			if (index3<numDepths && index3>=0) totalDepth += INDEXH(fDepthsH,index3); else numCorners--;
			if (index4<numDepths && index4>=0) totalDepth += INDEXH(fDepthsH,index4); else numCorners--;
			if (numCorners>0)
				totalDepth = totalDepth/(float)numCorners;
		}
	}
	//else totalDepth = INDEXH(fDepthsH,ptIndex);
	return totalDepth;
	
}
float TimeGridVelCurv_c::GetTotalDepth(WorldPoint refPoint,long ptIndex)
{
	long index1, index2, index3, index4, numDepths;
	OSErr err = 0;
	float totalDepth = 0;
	Boolean useTriNum = false;
	long triNum = 0;
	
	if (fVar.gridType == SIGMA_ROMS)
	{
		//if (triNum < 0) useTriNum = false;
		if (bVelocitiesOnNodes)
			err = (dynamic_cast<TTriGridVel*>(fGrid))->GetRectCornersFromTriIndexOrPoint(&index1, &index2, &index3, &index4, refPoint, triNum, useTriNum, fVerdatToNetCDFH, fNumCols);
		else 
			err = (dynamic_cast<TTriGridVel*>(fGrid))->GetRectCornersFromTriIndexOrPoint(&index1, &index2, &index3, &index4, refPoint, triNum, useTriNum, fVerdatToNetCDFH, fNumCols+1);
		
		//if (err) return 0;
		if (err) return -1;
		if (fDepthsH)
		{	// issue with extended grid not having depths - probably need to rework that idea
			long numCorners = 4;
			numDepths = _GetHandleSize((Handle)fDepthsH)/sizeof(**fDepthsH);
			if (index1<numDepths && index1>=0) totalDepth += INDEXH(fDepthsH,index1); else numCorners--;
			if (index2<numDepths && index2>=0) totalDepth += INDEXH(fDepthsH,index2); else numCorners--;
			if (index3<numDepths && index3>=0) totalDepth += INDEXH(fDepthsH,index3); else numCorners--;
			if (index4<numDepths && index4>=0) totalDepth += INDEXH(fDepthsH,index4); else numCorners--;
			if (numCorners>0)
				totalDepth = totalDepth/(float)numCorners;
		}
	}
	else 
	{
		if (fDepthsH) totalDepth = INDEXH(fDepthsH,ptIndex);
	}
	return totalDepth;
	
}
void TimeGridVelCurv_c::GetDepthIndices(long ptIndex, float depthAtPoint, float totalDepth, long *depthIndex1, long *depthIndex2)
{
	// probably eventually switch to base class
	long indexToDepthData = 0;
	long numDepthLevels = GetNumDepthLevelsInFile();
	//float totalDepth = 0;
	//FLOATH depthsH = ((TTriGridVel*)fGrid)->GetDepths();
	
	/*if (fDepthsH)
	 {
	 totalDepth = INDEXH(fDepthsH,ptIndex);
	 }
	 else*/
	if (totalDepth==0)
	{
		*depthIndex1 = indexToDepthData;
		*depthIndex2 = UNASSIGNEDINDEX;
		return;
	}
	
	if (fDepthLevelsHdl && numDepthLevels>0) 
	{
		/*if (fVar.gridType==MULTILAYER)
		 totalDepth = INDEXH(fDepthLevelsHdl,numDepthLevels-1);*/
		// otherwise it's SIGMA_ROMS
	}
	else
	{
		*depthIndex1 = indexToDepthData;
		*depthIndex2 = UNASSIGNEDINDEX;
		return;
	}
	switch(fVar.gridType) 
	{	// function should not be called for TWO_D, haven't used BAROTROPIC yet
			/*case TWO_D:	// no depth data
			 *depthIndex1 = indexToDepthData;
			 *depthIndex2 = UNASSIGNEDINDEX;
			 break;
			 case BAROTROPIC:	// values same throughout column, but limit on total depth
			 if (depthAtPoint <= totalDepth)
			 {
			 *depthIndex1 = indexToDepthData;
			 *depthIndex2 = UNASSIGNEDINDEX;
			 }
			 else
			 {
			 *depthIndex1 = UNASSIGNEDINDEX;
			 *depthIndex2 = UNASSIGNEDINDEX;
			 }
			 break;*/
			//case MULTILAYER: //
			/*case MULTILAYER: //
			 if (depthAtPoint <= totalDepth) // check data exists at chosen/LE depth for this point
			 {	// if depths are measured from the bottom this is confusing
			 long j;
			 for(j=0;j<numDepths-1;j++)
			 {
			 if(INDEXH(fDepthsH,indexToDepthData+j)<depthAtPoint &&
			 depthAtPoint<=INDEXH(fDepthsH,indexToDepthData+j+1))
			 {
			 *depthIndex1 = indexToDepthData+j;
			 *depthIndex2 = indexToDepthData+j+1;
			 }
			 else if(INDEXH(fDepthsH,indexToDepthData+j)==depthAtPoint)
			 {
			 *depthIndex1 = indexToDepthData+j;
			 *depthIndex2 = UNASSIGNEDINDEX;
			 }
			 }
			 if(INDEXH(fDepthsH,indexToDepthData)==depthAtPoint)	// handles single depth case
			 {
			 *depthIndex1 = indexToDepthData;
			 *depthIndex2 = UNASSIGNEDINDEX;
			 }
			 else if(INDEXH(fDepthsH,indexToDepthData+numDepths-1)<depthAtPoint)
			 {
			 *depthIndex1 = indexToDepthData+numDepths-1;
			 *depthIndex2 = UNASSIGNEDINDEX; //BOTTOM, for now just extrapolate lowest depth value (at bottom case?)
			 }
			 else if(INDEXH(fDepthsH,indexToDepthData)>depthAtPoint)
			 {
			 *depthIndex1 = indexToDepthData;
			 *depthIndex2 = UNASSIGNEDINDEX; //TOP, for now just extrapolate highest depth value
			 }
			 }
			 else // no data at this point
			 {
			 *depthIndex1 = UNASSIGNEDINDEX;
			 *depthIndex2 = UNASSIGNEDINDEX;
			 }
			 break;*/
		case MULTILAYER: // 
			if (depthAtPoint<0)
			{	// what is this?
				//*depthIndex1 = indexToDepthData+numDepthLevels-1;
				*depthIndex1 = indexToDepthData;
				*depthIndex2 = UNASSIGNEDINDEX; //BOTTOM, for now just extrapolate lowest depth value (at bottom case?)
				return;
			}
			if (depthAtPoint <= totalDepth) // check data exists at chosen/LE depth for this point
			{	// is sigma always 0-1 ?
				long j;
				float depthAtLevel, depthAtNextLevel;
				for(j=0;j<numDepthLevels-1;j++)
				{
					depthAtLevel = INDEXH(fDepthLevelsHdl,indexToDepthData+j);
					depthAtNextLevel = INDEXH(fDepthLevelsHdl,indexToDepthData+j+1);
					if(depthAtLevel<depthAtPoint &&
					   depthAtPoint<=depthAtNextLevel)
					{
						*depthIndex1 = indexToDepthData+j;
						*depthIndex2 = indexToDepthData+j+1;
						return;
					}
					else if(depthAtLevel==depthAtPoint)
					{
						*depthIndex1 = indexToDepthData+j;
						*depthIndex2 = UNASSIGNEDINDEX;
						return;
					}
				}
				if(INDEXH(fDepthLevelsHdl,indexToDepthData)==depthAtPoint)	// handles single depth case
				{
					*depthIndex1 = indexToDepthData;
					*depthIndex2 = UNASSIGNEDINDEX;
					return;
				}
				else if(INDEXH(fDepthLevelsHdl,indexToDepthData+numDepthLevels-1)<depthAtPoint)
				{
					*depthIndex1 = indexToDepthData+numDepthLevels-1;
					*depthIndex2 = UNASSIGNEDINDEX; //BOTTOM, for now just extrapolate lowest depth value (at bottom case?)
					return;
				}
				else if(INDEXH(fDepthLevelsHdl,indexToDepthData)>depthAtPoint)
				{
					*depthIndex1 = indexToDepthData;
					*depthIndex2 = UNASSIGNEDINDEX; //TOP, for now just extrapolate highest depth value
					return;
				}
			}
			else // no data at this point
			{
				*depthIndex1 = UNASSIGNEDINDEX;
				*depthIndex2 = UNASSIGNEDINDEX;
				return;
			}
			break;
			//break;
		case SIGMA: // 
			// code goes here, add SIGMA_ROMS, using z[k,:,:] = hc * (sc_r-Cs_r) + Cs_r * depth
			if (depthAtPoint<0)
			{	// keep in mind for grids with values at the bottom (rather than mid-cell) they may all be zero
				*depthIndex1 = indexToDepthData+numDepthLevels-1;
				*depthIndex2 = UNASSIGNEDINDEX; //BOTTOM, for now just extrapolate lowest depth value (at bottom case?)
				return;
			}
			if (depthAtPoint <= totalDepth) // check data exists at chosen/LE depth for this point
			{	// is sigma always 0-1 ?
				long j;
				float sigma, sigmaNext, depthAtLevel, depthAtNextLevel;
				for(j=0;j<numDepthLevels-1;j++)
				{
					sigma = INDEXH(fDepthLevelsHdl,indexToDepthData+j);
					sigmaNext = INDEXH(fDepthLevelsHdl,indexToDepthData+j+1);
					depthAtLevel = sigma * totalDepth;
					depthAtNextLevel = sigmaNext * totalDepth;
					if(depthAtLevel<depthAtPoint &&
					   depthAtPoint<=depthAtNextLevel)
					{
						*depthIndex1 = indexToDepthData+j;
						*depthIndex2 = indexToDepthData+j+1;
						return;
					}
					else if(depthAtLevel==depthAtPoint)
					{
						*depthIndex1 = indexToDepthData+j;
						*depthIndex2 = UNASSIGNEDINDEX;
						return;
					}
				}
				if(INDEXH(fDepthLevelsHdl,indexToDepthData)*totalDepth==depthAtPoint)	// handles single depth case
				{
					*depthIndex1 = indexToDepthData;
					*depthIndex2 = UNASSIGNEDINDEX;
					return;
				}
				else if(INDEXH(fDepthLevelsHdl,indexToDepthData+numDepthLevels-1)*totalDepth<depthAtPoint)
				{
					*depthIndex1 = indexToDepthData+numDepthLevels-1;
					*depthIndex2 = UNASSIGNEDINDEX; //BOTTOM, for now just extrapolate lowest depth value (at bottom case?)
					return;
				}
				else if(INDEXH(fDepthLevelsHdl,indexToDepthData)*totalDepth>depthAtPoint)
				{
					*depthIndex1 = indexToDepthData;
					*depthIndex2 = UNASSIGNEDINDEX; //TOP, for now just extrapolate highest depth value
					return;
				}
			}
			else // no data at this point
			{
				*depthIndex1 = UNASSIGNEDINDEX;
				*depthIndex2 = UNASSIGNEDINDEX;
				return;
			}
			//break;
		case SIGMA_ROMS: // 
			// code goes here, add SIGMA_ROMS, using z[k,:,:] = hc * (sc_r-Cs_r) + Cs_r * depth
			//WorldPoint wp; 
			//long triIndex;
			//totalDepth = GetTotalDepth(wp,triIndex);
			if (depthAtPoint<0)
			{
				*depthIndex1 = indexToDepthData;
				*depthIndex2 = UNASSIGNEDINDEX; //BOTTOM, for now just extrapolate lowest depth value (at bottom case?)
				return;
			}
			if (depthAtPoint <= totalDepth) // check data exists at chosen/LE depth for this point
			{	// is sigma always 0-1 ?
				long j;
				float sc_r, sc_r2, Cs_r, Cs_r2, depthAtLevel, depthAtNextLevel;
				//for(j=0;j<numDepthLevels-1;j++)
				for(j=numDepthLevels-1;j>0;j--)
				{
					// sc and Cs are negative so need abs value
					/*float sc_r = INDEXH(fDepthLevelsHdl,indexToDepthData+j);
					 float sc_r2 = INDEXH(fDepthLevelsHdl,indexToDepthData+j+1);
					 float Cs_r = INDEXH(fDepthLevelsHdl2,indexToDepthData+j);
					 float Cs_r2 = INDEXH(fDepthLevelsHdl2,indexToDepthData+j+1);*/
					sc_r = INDEXH(fDepthLevelsHdl,indexToDepthData+j);
					sc_r2 = INDEXH(fDepthLevelsHdl,indexToDepthData+j-1);
					Cs_r = INDEXH(fDepthLevelsHdl2,indexToDepthData+j);
					Cs_r2 = INDEXH(fDepthLevelsHdl2,indexToDepthData+j-1);
					//depthAtLevel = abs(hc * (sc_r-Cs_r) + Cs_r * totalDepth);
					//depthAtNextLevel = abs(hc * (sc_r2-Cs_r2) + Cs_r2 * totalDepth);
					depthAtLevel = abs(totalDepth*(hc*sc_r+totalDepth*Cs_r))/(totalDepth+hc);
					depthAtNextLevel = abs(totalDepth*(hc*sc_r2+totalDepth*Cs_r2))/(totalDepth+hc);
					if(depthAtLevel<depthAtPoint &&
					   depthAtPoint<=depthAtNextLevel)
					{
						*depthIndex1 = indexToDepthData+j;
						//*depthIndex2 = indexToDepthData+j+1;
						*depthIndex2 = indexToDepthData+j-1;
						return;
					}
					else if(depthAtLevel==depthAtPoint)
					{
						*depthIndex1 = indexToDepthData+j;
						*depthIndex2 = UNASSIGNEDINDEX;
						return;
					}
				}
				//if(INDEXH(fDepthLevelsHdl,indexToDepthData)*totalDepth==depthAtPoint)	// handles single depth case
				if(GetDepthAtIndex(indexToDepthData+numDepthLevels-1,totalDepth)==depthAtPoint)	// handles single depth case
					//if(GetTopDepth(indexToDepthData+numDepthLevels-1,totalDepth)==depthAtPoint)	// handles single depth case
				{
					//*depthIndex1 = indexToDepthData;
					*depthIndex1 = indexToDepthData+numDepthLevels-1;
					*depthIndex2 = UNASSIGNEDINDEX;
					return;
				}
				//else if(INDEXH(fDepthLevelsHdl,indexToDepthData+numDepthLevels-1)*totalDepth<depthAtPoint)
				//else if(INDEXH(fDepthLevelsHdl,indexToDepthData)*totalDepth<depthAtPoint)	// 0 is bottom
				else if(GetDepthAtIndex(indexToDepthData,totalDepth)<depthAtPoint)	// 0 is bottom
					//else if(GetBottomDepth(indexToDepthData,totalDepth)<depthAtPoint)	// 0 is bottom
				{
					*depthIndex1 = indexToDepthData;
					*depthIndex2 = UNASSIGNEDINDEX; //BOTTOM, for now just extrapolate lowest depth value (at bottom case?)
					return;
				}
				//else if(INDEXH(fDepthLevelsHdl,indexToDepthData)*totalDepth>depthAtPoint)
				//else if(INDEXH(fDepthLevelsHdl,indexToDepthData+numDepthLevels-1)*totalDepth>depthAtPoint)
				else if(GetDepthAtIndex(indexToDepthData+numDepthLevels-1,totalDepth)>depthAtPoint)
					//else if(GetTopDepth(indexToDepthData+numDepthLevels-1,totalDepth)>depthAtPoint)
				{
					*depthIndex1 = indexToDepthData+numDepthLevels-1;
					*depthIndex2 = UNASSIGNEDINDEX; //TOP, for now just extrapolate highest depth value
					return;
				}
			}
			else // no data at this point
			{
				*depthIndex1 = UNASSIGNEDINDEX;
				*depthIndex2 = UNASSIGNEDINDEX;
				return;
			}
			break;
		default:
			*depthIndex1 = UNASSIGNEDINDEX;
			*depthIndex2 = UNASSIGNEDINDEX;
			break;
	}
}

OSErr TimeGridVelCurv_c::TextRead(char *path, char *topFilePath) 
{
	// this code is for curvilinear grids
	OSErr err = 0;
	long i,j,k, numScanned, indexOfStart = 0;
	int status, ncid, latIndexid, lonIndexid, latid, lonid, recid, timeid, sigmaid, sigmavarid, sigmavarid2, hcvarid, depthid, depthdimid, depthvarid, mask_id, numdims;
	size_t latLength, lonLength, recs, t_len, t_len2, sigmaLength=0;
	float startLat,startLon,endLat,endLon,hc_param=0.;
	char recname[NC_MAX_NAME], *timeUnits=0;	
	char dimname[NC_MAX_NAME], s[256], topPath[256], outPath[256];
	WORLDPOINTFH vertexPtsH=0;
	FLOATH totalDepthsH=0, sigmaLevelsH=0;
	float yearShift=0.;
	double *lat_vals=0,*lon_vals=0,timeVal;
	float *depth_vals=0,*sigma_vals=0,*sigma_vals2=0;
	static size_t latIndex=0,lonIndex=0,timeIndex,ptIndex[2]={0,0},sigmaIndex=0;
	static size_t pt_count[2], sigma_count;
	Seconds startTime, startTime2;
	double timeConversion = 1., scale_factor = 1.;
	char errmsg[256] = "";
	char fileName[64],*modelTypeStr=0;
	Boolean isLandMask = true/*, isCoopsMask = false*/;
	static size_t mask_index[] = {0,0};
	static size_t mask_count[2];
	double *landmask = 0; 
	DOUBLEH landmaskH=0;
	
	if (!path || !path[0]) return 0;
	strcpy(fVar.pathName,path);
	
	strcpy(s,path);
	//SplitPathFile (s, fileName);	// this won't work for unix path right now...
	SplitPathFileName (s, fileName);	
	strcpy(fVar.userName, fileName); // maybe use a name from the file
	status = nc_open(path, NC_NOWRITE, &ncid);
	if (status != NC_NOERR) {err = -1; goto done;}

	// check number of dimensions - 2D or 3D
	status = nc_inq_ndims(ncid, &numdims);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	status = nc_inq_attlen(ncid,NC_GLOBAL,"generating_model",&t_len2);
	if (status != NC_NOERR) {status = nc_inq_attlen(ncid,NC_GLOBAL,"generator",&t_len2); if (status != NC_NOERR) {fIsNavy = false; /*goto done;*/}}	// will need to split for Navy vs LAS
	else 
	{
		fIsNavy = true;
		// may only need to see keyword is there, since already checked grid type
		modelTypeStr = new char[t_len2+1];
		status = nc_get_att_text(ncid, NC_GLOBAL, "generating_model", modelTypeStr);
		if (status != NC_NOERR) {status = nc_inq_attlen(ncid,NC_GLOBAL,"generator",&t_len2); if (status != NC_NOERR) {fIsNavy = false; goto done;}}	// will need to split for regridded or non-Navy cases 
		modelTypeStr[t_len2] = '\0';
		
		strcpy(fVar.userName, modelTypeStr); // maybe use a name from the file
	}
	
	status = nc_inq_dimid(ncid, "time", &recid); //Navy
	if (status != NC_NOERR) 
	{
		status = nc_inq_unlimdim(ncid, &recid);	// issue of time not being unlimited dimension
		if (status != NC_NOERR || recid==-1) {err = -1; goto done;}
	}
	
	//if (fIsNavy)
	status = nc_inq_varid(ncid, "time", &timeid); 
	if (status != NC_NOERR) {status = nc_inq_varid(ncid, "TIME", &timeid);if (status != NC_NOERR) {err = -1; goto done;} /*timeid = recid;*/} 	// for Ferret files, everything is in CAPS
	//if (status != NC_NOERR) {/*err = -1; goto done;*/ timeid = recid;} 	// for LAS files, variable names unstable
	
	//if (!fIsNavy)
	//status = nc_inq_attlen(ncid, recid, "units", &t_len);	// recid is the dimension id not the variable id
	//else	// LAS has them in order, and time is unlimited, but variable/dimension names keep changing so leave this way for now
	status = nc_inq_attlen(ncid, timeid, "units", &t_len);
	if (status != NC_NOERR) 
	{
		//timeUnits = 0;	// files should always have this info
		//timeConversion = 3600.;		// default is hours
		//startTime2 = model->GetStartTime();	// default to model start time
		err = -1; goto done;
	}
	else
	{
		DateTimeRec time;
		char unitStr[24], junk[10];
		
		timeUnits = new char[t_len+1];
		status = nc_get_att_text(ncid, timeid, "units", timeUnits);
		if (status != NC_NOERR) {err = -1; goto done;} 
		timeUnits[t_len] = '\0'; // moved this statement before StringSubstitute, JLM 5/2/10
		StringSubstitute(timeUnits, ':', ' ');
		StringSubstitute(timeUnits, '-', ' ');
		
		numScanned=sscanf(timeUnits, "%s %s %hd %hd %hd %hd %hd %hd",
						  unitStr, junk, &time.year, &time.month, &time.day,
						  &time.hour, &time.minute, &time.second) ;
		if (numScanned==5 || numScanned==4)	
		{time.hour = 0; time.minute = 0; time.second = 0; }
		else if (numScanned==7)	time.second = 0;
		else if (numScanned<8)	
		{ 
			//timeUnits = 0;	// files should always have this info
			//timeConversion = 3600.;		// default is hours
			//startTime2 = model->GetStartTime();	// default to model start time
			err = -1; TechError("TimeGridVelCurv_c::TextRead()", "sscanf() == 8", 0); goto done;
		}
		//else
		{
			// code goes here, trouble with the DAYS since 1900 format, since converts to seconds since 1904
			if (time.year ==1900) {time.year += 40; time.day += 1; /*for the 1900 non-leap yr issue*/ yearShift = 40.;}
			DateToSeconds (&time, &startTime2);	// code goes here, which start Time to use ??
			if (!strcmpnocase(unitStr,"HOURS") || !strcmpnocase(unitStr,"HOUR"))
				timeConversion = 3600.;
			else if (!strcmpnocase(unitStr,"MINUTES") || !strcmpnocase(unitStr,"MINUTE"))
				timeConversion = 60.;
			else if (!strcmpnocase(unitStr,"SECONDS") || !strcmpnocase(unitStr,"SECOND"))
				timeConversion = 1.;
			else if (!strcmpnocase(unitStr,"DAYS") || !strcmpnocase(unitStr,"DAY"))
				timeConversion = 24.*3600.;
		}
	} 
	
	if (fIsNavy)
	{
		status = nc_inq_dimid(ncid, "gridy", &latIndexid); //Navy
		if (status != NC_NOERR) {err = -1; goto done;}
		status = nc_inq_dimlen(ncid, latIndexid, &latLength);
		if (status != NC_NOERR) {err = -1; goto done;}
		status = nc_inq_dimid(ncid, "gridx", &lonIndexid);	//Navy
		if (status != NC_NOERR) {err = -1; goto done;}
		status = nc_inq_dimlen(ncid, lonIndexid, &lonLength);
		if (status != NC_NOERR) {err = -1; goto done;}
		// option to use index values?
		status = nc_inq_varid(ncid, "grid_lat", &latid);
		if (status != NC_NOERR) {err = -1; goto done;}
		status = nc_inq_varid(ncid, "grid_lon", &lonid);
		if (status != NC_NOERR) {err = -1; goto done;}
	}
	else
	{
		for (i=0;i<numdims;i++)
		{
			if (i == recid) continue;
			status = nc_inq_dimname(ncid,i,dimname);
			if (status != NC_NOERR) {err = -1; goto done;}
			//if (!strncmpnocase(dimname,"X",1) || !strncmpnocase(dimname,"LON",3))
			if (!strncmpnocase(dimname,"X",1) || !strncmpnocase(dimname,"LON",3) || !strncmpnocase(dimname,"NX",2))
			{
				lonIndexid = i;
			}
			//if (!strncmpnocase(dimname,"Y",1) || !strncmpnocase(dimname,"LAT",3))
			if (!strncmpnocase(dimname,"Y",1) || !strncmpnocase(dimname,"LAT",3) || !strncmpnocase(dimname,"NY",2))
			{
				latIndexid = i;
			}
		}
		
		status = nc_inq_dimlen(ncid, latIndexid, &latLength);
		if (status != NC_NOERR) {err = -1; goto done;}
		
		status = nc_inq_dimlen(ncid, lonIndexid, &lonLength);
		if (status != NC_NOERR) {err = -1; goto done;}
		
		status = nc_inq_varid(ncid, "LATITUDE", &latid);
		if (status != NC_NOERR) 
		{
			status = nc_inq_varid(ncid, "lat", &latid);
			if (status != NC_NOERR) {err = -1; goto done;}
		}
		status = nc_inq_varid(ncid, "LONGITUDE", &lonid);
		if (status != NC_NOERR) 
		{
			status = nc_inq_varid(ncid, "lon", &lonid);
			if (status != NC_NOERR) {err = -1; goto done;}
		}
	}
	
	pt_count[0] = latLength;
	pt_count[1] = lonLength;
	vertexPtsH = (WorldPointF**)_NewHandleClear(latLength*lonLength*sizeof(WorldPointF));
	if (!vertexPtsH) {err = memFullErr; goto done;}
	lat_vals = new double[latLength*lonLength]; 
	lon_vals = new double[latLength*lonLength]; 
	if (!lat_vals || !lon_vals) {err = memFullErr; goto done;}
	status = nc_get_vara_double(ncid, latid, ptIndex, pt_count, lat_vals);
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_get_vara_double(ncid, lonid, ptIndex, pt_count, lon_vals);
	if (status != NC_NOERR) {err = -1; goto done;}
	for (i=0;i<latLength;i++)
	{
		for (j=0;j<lonLength;j++)
		{
			// grid ordering does matter for creating ptcurmap, assume increases fastest in x/lon, then in y/lat
			INDEXH(vertexPtsH,i*lonLength+j).pLat = lat_vals[(latLength-i-1)*lonLength+j];	
			INDEXH(vertexPtsH,i*lonLength+j).pLong = lon_vals[(latLength-i-1)*lonLength+j];
		}
	}
	fVertexPtsH	 = vertexPtsH;// get first and last, lat/lon values, then last-first/total-1 = dlat/dlon
	
	
	status = nc_inq_dimid(ncid, "sigma", &sigmaid); 	
	if (status != NC_NOERR)
	{
		status = nc_inq_dimid(ncid, "levels", &depthdimid); 
		//status = nc_inq_dimid(ncid, "depth", &depthdimid); 
		if (status != NC_NOERR || fIsNavy) 
		{
			fVar.gridType = TWO_D; /*err = -1; goto done;*/
		}	
		else
		{// check for zgrid option here
			fVar.gridType = MULTILAYER; /*err = -1; goto done;*/
			//status = nc_inq_varid(ncid, "depth", &sigmavarid); //Navy
			status = nc_inq_varid(ncid, "depth_levels", &sigmavarid); //Navy
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_inq_dimlen(ncid, depthdimid, &sigmaLength);
			if (status != NC_NOERR) {err = -1; goto done;}
			fVar.maxNumDepths = sigmaLength;
			sigma_vals = new float[sigmaLength];
			if (!sigma_vals) {err = memFullErr; goto done;}
			sigma_count = sigmaLength;
			status = nc_get_vara_float(ncid, sigmavarid, &sigmaIndex, &sigma_count, sigma_vals);
			if (status != NC_NOERR) {err = -1; goto done;}
		}
	}
	else
	{
		status = nc_inq_varid(ncid, "sigma", &sigmavarid); //Navy
		if (status != NC_NOERR) 
		{
			status = nc_inq_varid(ncid, "sc_r", &sigmavarid);
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_inq_varid(ncid, "Cs_r", &sigmavarid2);
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_inq_dimlen(ncid, sigmaid, &sigmaLength);
			if (status != NC_NOERR) {err = -1; goto done;}
			fVar.gridType = SIGMA_ROMS;
			fVar.maxNumDepths = sigmaLength;
			sigma_vals = new float[sigmaLength];
			if (!sigma_vals) {err = memFullErr; goto done;}
			sigma_vals2 = new float[sigmaLength];
			if (!sigma_vals2) {err = memFullErr; goto done;}
			sigma_count = sigmaLength;
			status = nc_get_vara_float(ncid, sigmavarid, &sigmaIndex, &sigma_count, sigma_vals);
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_get_vara_float(ncid, sigmavarid2, &sigmaIndex, &sigma_count, sigma_vals2);
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_inq_varid(ncid, "hc", &hcvarid);
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_get_var1_float(ncid, hcvarid, &sigmaIndex, &hc_param);
			if (status != NC_NOERR) {err = -1; goto done;}
			//{err = -1; goto done;}
		}
		else
		{
			// code goes here, for SIGMA_ROMS the variable isn't sigma but sc_r and Cs_r, with parameter hc
			status = nc_inq_dimlen(ncid, sigmaid, &sigmaLength);
			if (status != NC_NOERR) {err = -1; goto done;}
			// check if sigmaLength > 1
			fVar.gridType = SIGMA;
			fVar.maxNumDepths = sigmaLength;
			sigma_vals = new float[sigmaLength];
			if (!sigma_vals) {err = memFullErr; goto done;}
			sigma_count = sigmaLength;
			status = nc_get_vara_float(ncid, sigmavarid, &sigmaIndex, &sigma_count, sigma_vals);
			if (status != NC_NOERR) {err = -1; goto done;}
		}
		// once depth is read in 
	}
	
	status = nc_inq_varid(ncid, "depth", &depthid);	// this is required for sigma or multilevel grids
	if (status != NC_NOERR || fIsNavy) {fVar.gridType = TWO_D;/*err = -1; goto done;*/}
	else
	{	
		/*if (fVar.gridType==MULTILAYER)
		 {
		 // for now
		 totalDepthsH = (FLOATH)_NewHandleClear(latLength*lonLength*sizeof(float));
		 if (!totalDepthsH) {err = memFullErr; goto done;}
		 depth_vals = new float[latLength*lonLength];
		 if (!depth_vals) {err = memFullErr; goto done;}
		 for (i=0;i<latLength*lonLength;i++)
		 {
		 INDEXH(totalDepthsH,i)=sigma_vals[sigmaLength-1];
		 depth_vals[i] = INDEXH(totalDepthsH,i);
		 }
		 
		 }
		 else*/
		{
			totalDepthsH = (FLOATH)_NewHandleClear(latLength*lonLength*sizeof(float));
			if (!totalDepthsH) {err = memFullErr; goto done;}
			depth_vals = new float[latLength*lonLength];
			if (!depth_vals) {err = memFullErr; goto done;}
			status = nc_get_vara_float(ncid, depthid, ptIndex,pt_count, depth_vals);
			if (status != NC_NOERR) {err = -1; goto done;}
			
			status = nc_get_att_double(ncid, depthid, "scale_factor", &scale_factor);
			if (status != NC_NOERR) {/*err = -1; goto done;*/}	// don't require scale factor
		}
	}
	
	status = nc_inq_dim(ncid, recid, recname, &recs);
	if (status != NC_NOERR) {err = -1; goto done;}
	if (recs <= 0) {strcpy(errmsg,"No times in file. Error opening NetCDF file"); err = -1; goto done;}
	fTimeHdl = (Seconds**)_NewHandleClear(recs*sizeof(Seconds));
	if (!fTimeHdl) {err = memFullErr; goto done;}
	for (i=0;i<recs;i++)
	{
		Seconds newTime;
		// possible units are, HOURS, MINUTES, SECONDS,...
		timeIndex = i;
		status = nc_get_var1_double(ncid, timeid, &timeIndex, &timeVal);
		if (status != NC_NOERR) {strcpy(errmsg,"Error reading times from NetCDF file"); err = -1; goto done;}
		// get rid of the seconds since they get garbled in the dialogs
		newTime = RoundDateSeconds(round(startTime2+timeVal*timeConversion));
		INDEXH(fTimeHdl,i) = newTime-yearShift*3600.*24.*365.25;	// which start time where?
		if (i==0) startTime = newTime-yearShift*3600.*24.*365.25;
	}
	
	fNumRows = latLength;
	fNumCols = lonLength;
	
	mask_count[0] = latLength;
	mask_count[1] = lonLength;
	
	status = nc_inq_varid(ncid, "mask", &mask_id);
	if (status != NC_NOERR)	{isLandMask = false;}
	
	//status = nc_inq_varid(ncid, "coops_mask", &mask_id);	// should only have one or the other
	//if (status != NC_NOERR)	{isCoopsMask = false;}
	//else {isCoopsMask = true; bIsCOOPSWaterMask = true;}
	
	if (isLandMask /*|| isCoopsMask*/)
	{	// no need to bother with the handle here...
		// maybe should store the mask? we are using it in ReadTimeValues, do we need to?
		landmask = new double[latLength*lonLength]; 
		if(!landmask) {TechError("TimeGridVelCurv_c::TextRead()", "new[]", 0); err = memFullErr; goto done;}
		landmaskH = (double**)_NewHandleClear(latLength*lonLength*sizeof(double));
		if(!landmaskH) {TechError("TimeGridVelCurv_c::TextRead()", "_NewHandleClear()", 0); err = memFullErr; goto done;}
		status = nc_get_vara_double(ncid, mask_id, mask_index, mask_count, landmask);
		if (status != NC_NOERR) {err = -1; goto done;}
		
		for (i=0;i<latLength;i++)
		{
			for (j=0;j<lonLength;j++)
			{
				INDEXH(landmaskH,i*lonLength+j) = landmask[(latLength-i-1)*lonLength+j];
			}
		}
	}
	
	status = nc_close(ncid);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	// either file is sent in ( output from Topology save option) or Topology needs to be generated
	if (topFilePath[0]) 
	{	
		err = (dynamic_cast<TimeGridVelCurv*>(this))->ReadTopology(topFilePath); 
		goto depths;
	}
	
	if (isLandMask) 
	{
		if (!bVelocitiesOnNodes)	// default is velocities on cells
			err = ReorderPoints(landmaskH,errmsg);
	//else if (isCoopsMask) 
		else 
			err = ReorderPointsCOOPSMask(landmaskH,errmsg);
	}
	else err = ReorderPointsNoMask(errmsg);
	
depths:
	if (err) goto done;
	// also translate to fDepthDataInfo and fDepthsH here, using sigma or zgrid info
	
	if (totalDepthsH)
	{
		fDepthsH = (FLOATH)_NewHandle(sizeof(float)*fNumRows*fNumCols);
		if(!fDepthsH){TechError("TimeGridVelCurv_c::TextRead()", "_NewHandle()", 0); err = memFullErr; goto done;}
		for (i=0;i<latLength;i++)
		{
			for (j=0;j<lonLength;j++)
			{
				INDEXH(totalDepthsH,i*lonLength+j) = abs(depth_vals[(latLength-i-1)*lonLength+j]) * scale_factor;	
				INDEXH(fDepthsH,i*lonLength+j) = abs(depth_vals[(latLength-i-1)*lonLength+j]) * scale_factor;	
			}
		}
	}
	
	fNumDepthLevels = sigmaLength;
	if (sigmaLength>1)
	{
		float sigma = 0;
		fDepthLevelsHdl = (FLOATH)_NewHandleClear(sigmaLength * sizeof(float));
		if (!fDepthLevelsHdl) {err = memFullErr; goto done;}
		for (i=0;i<sigmaLength;i++)
		{	// decide what to do here, may be upside down for ROMS
			sigma = sigma_vals[i];
			if (sigma_vals[0]==1) 
				INDEXH(fDepthLevelsHdl,i) = (1-sigma);	// in this case velocities will be upside down too...
			else
			{
				if (fVar.gridType == SIGMA_ROMS)
					INDEXH(fDepthLevelsHdl,i) = sigma;
				else
					INDEXH(fDepthLevelsHdl,i) = abs(sigma);
			}
			
		}
		if (fVar.gridType == SIGMA_ROMS)
		{
			fDepthLevelsHdl2 = (FLOATH)_NewHandleClear(sigmaLength * sizeof(float));
			if (!fDepthLevelsHdl2) {err = memFullErr; goto done;}
			for (i=0;i<sigmaLength;i++)
			{
				sigma = sigma_vals2[i];
				//if (sigma_vals[0]==1) 
				//INDEXH(fDepthLevelsHdl,i) = (1-sigma);	// in this case velocities will be upside down too...
				//else
				INDEXH(fDepthLevelsHdl2,i) = sigma;
			}
			hc = hc_param;
		}
	}
	
	if (totalDepthsH)
	{	// may need to extend the depth grid along with lat/lon grid - not sure what to use for the values though...
		// not sure what map will expect in terms of depths order
		long n,ptIndex,iIndex,jIndex;
		long numPoints = _GetHandleSize((Handle)fVerdatToNetCDFH)/sizeof(**fVerdatToNetCDFH);
		//_SetHandleSize((Handle)totalDepthsH,(fNumRows+1)*(fNumCols+1)*sizeof(float));
		_SetHandleSize((Handle)totalDepthsH,numPoints*sizeof(float));
		
		for (i=0; i<numPoints; i++)
		{	// works okay for simple grid except for far right column (need to extend depths similar to lat/lon)
			// if land use zero, if water use point next to it?
			ptIndex = INDEXH(fVerdatToNetCDFH,i);
			if (bVelocitiesOnNodes)
			{
				iIndex = ptIndex/(fNumCols);
				jIndex = ptIndex%(fNumCols);
			}
			else {
				iIndex = ptIndex/(fNumCols+1);
				jIndex = ptIndex%(fNumCols+1);
			}
			
			//iIndex = ptIndex/(fNumCols+1);
			//jIndex = ptIndex%(fNumCols+1);
			if (iIndex>0 && jIndex<fNumCols)
				//if (iIndex>0 && jIndex<fNumCols)
				ptIndex = (iIndex-1)*(fNumCols)+jIndex;
			else
				ptIndex = -1;
			
			//n = INDEXH(fVerdatToNetCDFH,i);
			//if (n<0 || n>= fNumRows*fNumCols) {printError("indices messed up"); err=-1; goto done;}
			//INDEXH(totalDepthsH,i) = depth_vals[n];
			if (ptIndex<0 || ptIndex>= fNumRows*fNumCols) 
			{
				//printError("indices messed up"); 
				//err=-1; goto done;
				//INDEXH(totalDepthsH,i) = 0;	// need to figure out what to do here...
				if (iIndex==0 && jIndex==fNumCols) ptIndex = jIndex-1;
				else if (iIndex==0) ptIndex = jIndex;
				else if (jIndex==fNumCols)ptIndex = (iIndex-1)*fNumCols+jIndex-1;
				if (ptIndex<0 || ptIndex >= fNumRows*fNumCols)
					INDEXH(totalDepthsH,i) = 0;
				else
					INDEXH(totalDepthsH,i) = INDEXH(fDepthsH,ptIndex);	// need to figure out what to do here...
				continue;
			}
			//INDEXH(totalDepthsH,i) = depth_vals[ptIndex];
			INDEXH(totalDepthsH,i) = INDEXH(fDepthsH,ptIndex);
		}
		if (!bVelocitiesOnNodes)	// code goes here, figure out how to handle depths in this case
			(dynamic_cast<TTriGridVel*>(fGrid))->SetDepths(totalDepthsH);
	}
	
done:
	// code goes here, set bathymetry
	if (err)
	{
		if(!errmsg[0])
			strcpy(errmsg,"Error opening NetCDF file");
		printNote(errmsg);
		//printNote("Error opening NetCDF file");
		if(fGrid)
		{
			fGrid ->Dispose();
			delete fGrid;
			fGrid = 0;
		}
		if(vertexPtsH) {DisposeHandle((Handle)vertexPtsH); vertexPtsH = 0; fVertexPtsH = 0;}
		if(sigmaLevelsH) {DisposeHandle((Handle)sigmaLevelsH); sigmaLevelsH = 0;}
		if (fDepthLevelsHdl) {DisposeHandle((Handle)fDepthLevelsHdl); fDepthLevelsHdl=0;}
		if (fDepthLevelsHdl2) {DisposeHandle((Handle)fDepthLevelsHdl2); fDepthLevelsHdl2=0;}
	}
	
	if (timeUnits) delete [] timeUnits;
	if (lat_vals) delete [] lat_vals;
	if (lon_vals) delete [] lon_vals;
	if (depth_vals) delete [] depth_vals;
	if (sigma_vals) delete [] sigma_vals;
	if (modelTypeStr) delete [] modelTypeStr;
	return err;
}

OSErr TimeGridVelCurv_c::ReadTimeData(long index,VelocityFH *velocityH, char* errmsg) 
{
	OSErr err = 0;
	long i,j,k;
	char path[256], outPath[256];
	char *velUnits=0; 
	int status, ncid, numdims;
	int curr_ucmp_id, curr_vcmp_id, curr_wcmp_id, angle_id, mask_id, uv_ndims;
	static size_t curr_index[] = {0,0,0,0}, angle_index[] = {0,0};
	static size_t curr_count[4], angle_count[2];
	size_t velunit_len;
	double *curr_uvals = 0,*curr_vvals = 0, *curr_wvals = 0, fill_value=-1e+34, test_value=8e+10;
	double *landmask = 0, velConversion=1.;
	double *angle_vals = 0,debug_mask;
	long totalNumberOfVels = fNumRows * fNumCols * fVar.maxNumDepths;
	VelocityFH velH = 0;
	FLOATH wvelH = 0;
	long latlength = fNumRows, numtri = 0;
	long lonlength = fNumCols;
	double scale_factor = 1.,angle = 0.,u_grid,v_grid;
	long numDepths = fVar.maxNumDepths;	// assume will always have full set of depths at each point for now
	Boolean bRotated = true, isLandMask = true, bIsWVel = false;
	
	errmsg[0]=0;
	
	
	strcpy(path,fVar.pathName);
	if (!path || !path[0]) return -1;
	
	status = nc_open(path, NC_NOWRITE, &ncid);
	if (status != NC_NOERR) {err = -1; goto done;}

	status = nc_inq_ndims(ncid, &numdims);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	curr_index[0] = index;	// time 
	curr_count[0] = 1;	// take one at a time
	if (numdims>=4)	// should check what the dimensions are
	{
		//curr_count[1] = 1;	// depth
		curr_count[1] = numDepths;	// depth
		curr_count[2] = latlength;
		curr_count[3] = lonlength;
	}
	else
	{
		curr_count[1] = latlength;	
		curr_count[2] = lonlength;
	}
	angle_count[0] = latlength;
	angle_count[1] = lonlength;
	
	if (fIsNavy)
	{
		numDepths = 1;
		// need to check if type is float or short, if float no scale factor?
		curr_uvals = new double[latlength*lonlength*numDepths]; 
		if(!curr_uvals) {TechError("TimeGridVelCurv_c::ReadTimeData()", "new[]", 0); err = memFullErr; goto done;}
		curr_vvals = new double[latlength*lonlength*numDepths]; 
		if(!curr_vvals) {TechError("TimeGridVelCurv_c::ReadTimeData()", "new[]", 0); err = memFullErr; goto done;}
		angle_vals = new double[latlength*lonlength]; 
		if(!angle_vals) {TechError("TimeGridVelCurv_c::ReadTimeData()", "new[]", 0); err = memFullErr; goto done;}
		status = nc_inq_varid(ncid, "water_gridu", &curr_ucmp_id);
		if (status != NC_NOERR) {err = -1; goto done;}
		status = nc_inq_varid(ncid, "water_gridv", &curr_vcmp_id);	// what if only input one at a time (u,v separate movers)?
		if (status != NC_NOERR) {err = -1; goto done;}
		status = nc_get_vara_double(ncid, curr_ucmp_id, curr_index, curr_count, curr_uvals);
		if (status != NC_NOERR) {err = -1; goto done;}
		status = nc_get_vara_double(ncid, curr_vcmp_id, curr_index, curr_count, curr_vvals);
		if (status != NC_NOERR) {err = -1; goto done;}
		status = nc_get_att_double(ncid, curr_ucmp_id, "_FillValue", &fill_value);
		status = nc_get_att_double(ncid, curr_ucmp_id, "scale_factor", &scale_factor);
		status = nc_inq_varid(ncid, "grid_orient", &angle_id);
		if (status != NC_NOERR) {err = -1; goto done;}
		status = nc_get_vara_double(ncid, angle_id, angle_index, angle_count, angle_vals);
		if (status != NC_NOERR) {/*err = -1; goto done;*/bRotated = false;}
	}
	else
	{
		status = nc_inq_varid(ncid, "mask", &mask_id);
		if (status != NC_NOERR)	{/*err=-1; goto done;*/ isLandMask = false;}
		status = nc_inq_varid(ncid, "ang", &angle_id);
		if (status != NC_NOERR) {/*err = -1; goto done;*/bRotated = false;}
		else
		{
			angle_vals = new double[latlength*lonlength]; 
			if(!angle_vals) {TechError("TimeGridVelCurv_c::ReadTimeData()", "new[ ]", 0); err = memFullErr; goto done;}
			status = nc_get_vara_double(ncid, angle_id, angle_index, angle_count, angle_vals);
			if (status != NC_NOERR) {/*err = -1; goto done;*/bRotated = false;}
		}
		curr_uvals = new double[latlength*lonlength*numDepths]; 
		if(!curr_uvals) 
		{
			TechError("TimeGridVelCurv_c::ReadTimeData()", "new[]", 0); 
			err = memFullErr; 
			goto done;
		}
		curr_vvals = new double[latlength*lonlength*numDepths]; 
		if(!curr_vvals) 
		{
			TechError("TimeGridVelCurv_c::ReadTimeData()", "new[]", 0); 
			err = memFullErr; 
			goto done;
		}
		curr_wvals = new double[latlength*lonlength*numDepths]; 
		if(!curr_wvals) 
		{
			TechError("TimeGridVelCurv_c::ReadTimeData()", "new[]", 0); 
			err = memFullErr; 
			goto done;
		}

		status = nc_inq_varid(ncid, "U", &curr_ucmp_id);
		if (status != NC_NOERR)
		{
			status = nc_inq_varid(ncid, "u", &curr_ucmp_id);
			if (status != NC_NOERR)
			{
				status = nc_inq_varid(ncid, "water_u", &curr_ucmp_id);
				if (status != NC_NOERR)
				{err = -1; goto done;}
			}
		}
		status = nc_inq_varid(ncid, "V", &curr_vcmp_id);
		if (status != NC_NOERR) 
		{
			status = nc_inq_varid(ncid, "v", &curr_vcmp_id);
			if (status != NC_NOERR) 
			{
				status = nc_inq_varid(ncid, "water_v", &curr_vcmp_id);
				if (status != NC_NOERR)
				{err = -1; goto done;}
			}
		}
		status = nc_inq_varid(ncid, "W", &curr_wcmp_id);
		if (status != NC_NOERR)
		{
			status = nc_inq_varid(ncid, "w", &curr_wcmp_id);
			if (status != NC_NOERR)
			{
				status = nc_inq_varid(ncid, "water_w", &curr_wcmp_id);
				if (status != NC_NOERR)
					//{err = -1; goto done;}
					bIsWVel = false;
				else
					bIsWVel = true;
			}
		}
		status = nc_inq_varndims(ncid, curr_ucmp_id, &uv_ndims);
		if (status==NC_NOERR){if (uv_ndims < numdims && uv_ndims==3) {curr_count[1] = latlength; curr_count[2] = lonlength;}}	// could have more dimensions than are used in u,v
		//status = nc_get_vara_float(ncid, curr_ucmp_id, curr_index, curr_count, curr_uvals);
		status = nc_get_vara_double(ncid, curr_ucmp_id, curr_index, curr_count, curr_uvals);
		if (status != NC_NOERR) {err = -1; goto done;}
		//status = nc_get_vara_float(ncid, curr_vcmp_id, curr_index, curr_count, curr_vvals);
		status = nc_get_vara_double(ncid, curr_vcmp_id, curr_index, curr_count, curr_vvals);
		if (status != NC_NOERR) {err = -1; goto done;}
		if (bIsWVel)
		{	
			status = nc_get_vara_double(ncid, curr_wcmp_id, curr_index, curr_count, curr_wvals);
			if (status != NC_NOERR) {err = -1; goto done;}
		}
		status = nc_inq_attlen(ncid, curr_ucmp_id, "units", &velunit_len);
		if (status == NC_NOERR)
		{
			velUnits = new char[velunit_len+1];
			status = nc_get_att_text(ncid, curr_ucmp_id, "units", velUnits);
			if (status == NC_NOERR)
			{
				velUnits[velunit_len] = '\0';
				if (!strcmpnocase(velUnits,"cm/s"))
					velConversion = .01;
				else if (!strcmpnocase(velUnits,"m/s"))
					velConversion = 1.0;
			}
		}
		
		
		status = nc_get_att_double(ncid, curr_ucmp_id, "_FillValue", &fill_value);
		if (status != NC_NOERR) {status = nc_get_att_double(ncid, curr_ucmp_id, "Fill_Value", &fill_value);/*if (status != NC_NOERR){fill_value=-1e+32;}{err = -1; goto done;}*/}	// don't require
		if (status != NC_NOERR) {status = nc_get_att_double(ncid, curr_ucmp_id, "FillValue", &fill_value);/*if (status != NC_NOERR){fill_value=-1e+32;}{err = -1; goto done;}*/}	// don't require
		if (status != NC_NOERR) {status = nc_get_att_double(ncid, curr_ucmp_id, "missing_value", &fill_value);/*if (status != NC_NOERR){fill_value=-1e+32;}{err = -1; goto done;}*/}	// don't require
		//if (status != NC_NOERR) {err = -1; goto done;}	// don't require
		status = nc_get_att_double(ncid, curr_ucmp_id, "scale_factor", &scale_factor);
	}	
	status = nc_close(ncid);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	// NOTE: if allow fill_value as NaN need to be sure to check for it wherever fill_value is used
	if (_isnan(fill_value)) 
		fill_value = -9999.;
	
	velH = (VelocityFH)_NewHandleClear(totalNumberOfVels * sizeof(VelocityFRec));
	if (!velH) 
	{
		err = memFullErr; 
		goto done;
	}
	//for (i=0;i<totalNumberOfVels;i++)
	for (k=0;k<numDepths;k++)
	{
		for (i=0;i<latlength;i++)
		{
			for (j=0;j<lonlength;j++)
			{
				if (fIsNavy)
				{
					if (curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]==fill_value)
						curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]=0.;
					if (curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]==fill_value)
						curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]=0.;
					u_grid = (double)curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols];
					v_grid = (double)curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols];
					if (bRotated) angle = angle_vals[(latlength-i-1)*lonlength+j];
					INDEXH(velH,i*lonlength+j+k*fNumRows*fNumCols).u = u_grid*cos(angle*PI/180.)-v_grid*sin(angle*PI/180.);
					INDEXH(velH,i*lonlength+j+k*fNumRows*fNumCols).v = u_grid*sin(angle*PI/180.)+v_grid*cos(angle*PI/180.);
				}
				else
				{
					if (curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]==fill_value || curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]==fill_value)
						curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols] = curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols] = 0;
					// NOTE: if leave velocity as NaN need to be sure to check for it wherever velocity is used (GetMove,Draw,...)
					if (_isnan(curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]) || _isnan(curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols]))
						curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols] = curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols] = 0;
					/////////////////////////////////////////////////
					if (bRotated)
					{
						u_grid = (double)curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols] * velConversion;
						v_grid = (double)curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols] * velConversion;
						if (bRotated) angle = angle_vals[(latlength-i-1)*lonlength+j];
						INDEXH(velH,i*lonlength+j+k*fNumRows*fNumCols).u = u_grid*cos(angle)-v_grid*sin(angle);	//in radians
						INDEXH(velH,i*lonlength+j+k*fNumRows*fNumCols).v = u_grid*sin(angle)+v_grid*cos(angle);
					}
					else
					{
						INDEXH(velH,i*lonlength+j+k*fNumRows*fNumCols).u = curr_uvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols] * velConversion;	// need units
						INDEXH(velH,i*lonlength+j+k*fNumRows*fNumCols).v = curr_vvals[(latlength-i-1)*lonlength+j+k*fNumRows*fNumCols] * velConversion;
					}
				}
			}
		}
	}
	*velocityH = velH;
	fFillValue = fill_value * velConversion;
	
	//if (scale_factor!=1.) fVar.curScale = scale_factor;	// hmm, this forces a reset of scale factor each time, overriding any set by hand
	if (scale_factor!=1.) fVar.fileScaleFactor = scale_factor;
	
done:
	if (err)
	{
		strcpy(errmsg,"Error reading current data from NetCDF file");
		// We don't want to put up an error message here because it can lead to an infinite loop of messages.
		//printNote("Error opening NetCDF file");
		if(velH) {DisposeHandle((Handle)velH); velH = 0;}
	}
	if (curr_uvals) 
	{
		delete [] curr_uvals; 
		curr_uvals = 0;
	}
	if (curr_vvals) 
	{
		delete [] curr_vvals; 
		curr_vvals = 0;
	}
	if (curr_wvals) 
	{
		delete [] curr_wvals; 
		curr_wvals = 0;
	}
	
	if (landmask) {delete [] landmask; landmask = 0;}
	if (angle_vals) {delete [] angle_vals; angle_vals = 0;}
	if (velUnits) {delete [] velUnits;}
	return err;
}

OSErr TimeGridVelCurv_c::ReorderPoints(DOUBLEH landmaskH, char* errmsg) 
{
	long i, j, n, ntri, numVerdatPts=0;
	long fNumRows_ext = fNumRows+1, fNumCols_ext = fNumCols+1;
	long nv = fNumRows * fNumCols, nv_ext = fNumRows_ext*fNumCols_ext;
	//long currentIsland=0, islandNum, nBoundaryPts=0, nEndPts=0, waterStartPoint;
	//long nSegs, segNum = 0, numIslands, rectIndex; 
	long iIndex,jIndex,index/*,currentIndex,startIndex*/; 
	long triIndex1,triIndex2,waterCellNum=0;
	long ptIndex = 0,cellNum = 0/*,diag = 1*/;
	//Boolean foundPt = false, isOdd;
	OSErr err = 0;
	
	LONGH landWaterInfo = (LONGH)_NewHandleClear(fNumRows * fNumCols * sizeof(long));
	//LONGH maskH2 = (LONGH)_NewHandleClear(nv_ext * sizeof(long));
	
	LONGH ptIndexHdl = (LONGH)_NewHandleClear(nv_ext * sizeof(**ptIndexHdl));
	LONGH verdatPtsH = (LONGH)_NewHandleClear(nv_ext * sizeof(**verdatPtsH));
	GridCellInfoHdl gridCellInfo = (GridCellInfoHdl)_NewHandleClear(nv * sizeof(**gridCellInfo));
	
	TopologyHdl topo=0;
	LongPointHdl pts=0;
	VelocityFH velH = 0;
	DAGTreeStruct tree;
	WorldRect triBounds;
	
	//LONGH boundaryPtsH = 0;
	//LONGH boundaryEndPtsH = 0;
	//LONGH waterBoundaryPtsH = 0;
	//Boolean** segUsed = 0;
	//SegInfoHdl segList = 0;
	//LONGH flagH = 0;
	
	TTriGridVel *triGrid = nil;
	tree.treeHdl = 0;
	TDagTree *dagTree = 0;
	
	
	if (!landWaterInfo || !ptIndexHdl || !gridCellInfo || !verdatPtsH /*|| !maskH2*/) {err = memFullErr; goto done;}
	
	for (i=0;i<fNumRows;i++)
	{
		for (j=0;j<fNumCols;j++)
		{
			if (INDEXH(landmaskH,i*fNumCols+j)==0)	// land point
			{
				INDEXH(landWaterInfo,i*fNumCols+j) = -1;	// may want to mark each separate island with a unique number
			}
			else
			{
				INDEXH(landWaterInfo,i*fNumCols+j) = 1;
				INDEXH(ptIndexHdl,i*fNumCols_ext+j) = -2;	// water box
				INDEXH(ptIndexHdl,i*fNumCols_ext+j+1) = -2;
				INDEXH(ptIndexHdl,(i+1)*fNumCols_ext+j) = -2;
				INDEXH(ptIndexHdl,(i+1)*fNumCols_ext+j+1) = -2;
			}
		}
	}
	
	for (i=0;i<fNumRows_ext;i++)
	{
		for (j=0;j<fNumCols_ext;j++)
		{
			if (INDEXH(ptIndexHdl,i*fNumCols_ext+j) == -2)
			{
				INDEXH(ptIndexHdl,i*fNumCols_ext+j) = ptIndex;	// count up grid points
				ptIndex++;
			}
			else
				INDEXH(ptIndexHdl,i*fNumCols_ext+j) = -1;
		}
	}
	
	for (i=0;i<fNumRows;i++)
	{
		for (j=0;j<fNumCols;j++)
		{
			if (INDEXH(landWaterInfo,i*fNumCols+j)>0)
			{
				INDEXH(gridCellInfo,i*fNumCols+j).cellNum = cellNum;
				cellNum++;
				INDEXH(gridCellInfo,i*fNumCols+j).topLeft = INDEXH(ptIndexHdl,i*fNumCols_ext+j);
				INDEXH(gridCellInfo,i*fNumCols+j).topRight = INDEXH(ptIndexHdl,i*fNumCols_ext+j+1);
				INDEXH(gridCellInfo,i*fNumCols+j).bottomLeft = INDEXH(ptIndexHdl,(i+1)*fNumCols_ext+j);
				INDEXH(gridCellInfo,i*fNumCols+j).bottomRight = INDEXH(ptIndexHdl,(i+1)*fNumCols_ext+j+1);
			}
			else INDEXH(gridCellInfo,i*fNumCols+j).cellNum = -1;
		}
	}
	ntri = cellNum*2;	// each water cell is split into two triangles
	if(!(topo = (TopologyHdl)_NewHandleClear(ntri * sizeof(Topology)))){err = memFullErr; goto done;}	
	for (i=0;i<nv_ext;i++)
	{
		if (INDEXH(ptIndexHdl,i) != -1)
		{
			INDEXH(verdatPtsH,numVerdatPts) = i;
			//INDEXH(verdatPtsH,INDEXH(ptIndexHdl,i)) = i;
			numVerdatPts++;
		}
	}
	_SetHandleSize((Handle)verdatPtsH,numVerdatPts*sizeof(**verdatPtsH));
	pts = (LongPointHdl)_NewHandle(sizeof(LongPoint)*(numVerdatPts));
	if(pts == nil)
	{
		strcpy(errmsg,"Not enough memory to triangulate data.");
		return -1;
	}
	
	for (i=0; i<=numVerdatPts; i++)	// make a list of grid points that will be used for triangles
	{
		float fLong, fLat, fDepth, dLon, dLat, dLon1, dLon2, dLat1, dLat2;
		double val, u=0., v=0.;
		LongPoint vertex;
		
		if(i < numVerdatPts) 
		{	// since velocities are defined at the lower left corner of each grid cell
			// need to add an extra row/col at the top/right of the grid
			// set lat/lon based on distance between previous two points 
			// these are just for boundary/drawing purposes, velocities are set to zero
			index = i+1;
			n = INDEXH(verdatPtsH,i);
			iIndex = n/fNumCols_ext;
			jIndex = n%fNumCols_ext;
			if (iIndex==0)
			{
				if (jIndex<fNumCols)
				{
					dLat = INDEXH(fVertexPtsH,fNumCols+jIndex).pLat - INDEXH(fVertexPtsH,jIndex).pLat;
					fLat = INDEXH(fVertexPtsH,jIndex).pLat - dLat;
					dLon = INDEXH(fVertexPtsH,fNumCols+jIndex).pLong - INDEXH(fVertexPtsH,jIndex).pLong;
					fLong = INDEXH(fVertexPtsH,jIndex).pLong - dLon;
				}
				else
				{
					dLat1 = (INDEXH(fVertexPtsH,jIndex-1).pLat - INDEXH(fVertexPtsH,jIndex-2).pLat);
					dLat2 = INDEXH(fVertexPtsH,fNumCols+jIndex-1).pLat - INDEXH(fVertexPtsH,fNumCols+jIndex-2).pLat;
					fLat = 2*(INDEXH(fVertexPtsH,jIndex-1).pLat + dLat1)-(INDEXH(fVertexPtsH,fNumCols+jIndex-1).pLat+dLat2);
					dLon1 = INDEXH(fVertexPtsH,fNumCols+jIndex-1).pLong - INDEXH(fVertexPtsH,jIndex-1).pLong;
					dLon2 = (INDEXH(fVertexPtsH,fNumCols+jIndex-2).pLong - INDEXH(fVertexPtsH,jIndex-2).pLong);
					fLong = 2*(INDEXH(fVertexPtsH,jIndex-1).pLong-dLon1)-(INDEXH(fVertexPtsH,jIndex-2).pLong-dLon2);
				}
			}
			else 
			{
				if (jIndex<fNumCols)
				{
					fLat = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex).pLat;
					fLong = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex).pLong;
				}
				else
				{
					dLat = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-1).pLat - INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-2).pLat;
					fLat = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-1).pLat + dLat;
					dLon = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-1).pLong - INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-2).pLong;
					fLong = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-1).pLong + dLon;
				}
			}
			vertex.v = (long)(fLat*1e6);
			vertex.h = (long)(fLong*1e6);
			
			fDepth = 1.;
			INDEXH(pts,i) = vertex;
		}
		else { // for outputting a verdat the last line should be all zeros
			//index = 0;
			//fLong = fLat = fDepth = 0.0;
		}
		/////////////////////////////////////////////////
		
	}

	// figure out the bounds
	triBounds = voidWorldRect;
	if(pts) 
	{
		LongPoint	thisLPoint;
		
		if(numVerdatPts > 0)
		{
			WorldPoint  wp;
			for(i=0;i<numVerdatPts;i++)
			{
				thisLPoint = INDEXH(pts,i);
				wp.pLat = thisLPoint.v;
				wp.pLong = thisLPoint.h;
				AddWPointToWRect(wp.pLat, wp.pLong, &triBounds);
			}
		}
	}
	
	DisplayMessage("NEXTMESSAGETEMP");
	DisplayMessage("Making Triangles");
	
	/////////////////////////////////////////////////
	for (i=0;i<fNumRows;i++)
	{
		for (j=0;j<fNumCols;j++)
		{
			if (INDEXH(landWaterInfo,i*fNumCols+j)==-1)
				continue;
			waterCellNum = INDEXH(gridCellInfo,i*fNumCols+j).cellNum;	// split each cell into 2 triangles
			triIndex1 = 2*waterCellNum;
			triIndex2 = 2*waterCellNum+1;
			// top/left tri in rect
			(*topo)[triIndex1].vertex1 = INDEXH(gridCellInfo,i*fNumCols+j).topRight;
			(*topo)[triIndex1].vertex2 = INDEXH(gridCellInfo,i*fNumCols+j).topLeft;
			(*topo)[triIndex1].vertex3 = INDEXH(gridCellInfo,i*fNumCols+j).bottomLeft;
			if (j==0 || INDEXH(gridCellInfo,i*fNumCols+j-1).cellNum == -1)
				(*topo)[triIndex1].adjTri1 = -1;
			else
			{
				(*topo)[triIndex1].adjTri1 = INDEXH(gridCellInfo,i*fNumCols+j-1).cellNum * 2 + 1;
			}
			(*topo)[triIndex1].adjTri2 = triIndex2;
			if (i==0 || INDEXH(gridCellInfo,(i-1)*fNumCols+j).cellNum==-1)
				(*topo)[triIndex1].adjTri3 = -1;
			else
			{
				(*topo)[triIndex1].adjTri3 = INDEXH(gridCellInfo,(i-1)*fNumCols+j).cellNum * 2 + 1;
			}
			// bottom/right tri in rect
			(*topo)[triIndex2].vertex1 = INDEXH(gridCellInfo,i*fNumCols+j).bottomLeft;
			(*topo)[triIndex2].vertex2 = INDEXH(gridCellInfo,i*fNumCols+j).bottomRight;
			(*topo)[triIndex2].vertex3 = INDEXH(gridCellInfo,i*fNumCols+j).topRight;
			if (j==fNumCols-1 || INDEXH(gridCellInfo,i*fNumCols+j+1).cellNum == -1)
				(*topo)[triIndex2].adjTri1 = -1;
			else
			{
				(*topo)[triIndex2].adjTri1 = INDEXH(gridCellInfo,i*fNumCols+j+1).cellNum * 2;
			}
			(*topo)[triIndex2].adjTri2 = triIndex1;
			if (i==fNumRows-1 || INDEXH(gridCellInfo,(i+1)*fNumCols+j).cellNum == -1)
				(*topo)[triIndex2].adjTri3 = -1;
			else
			{
				(*topo)[triIndex2].adjTri3 = INDEXH(gridCellInfo,(i+1)*fNumCols+j).cellNum * 2;
			}
		}
	}	
	
	DisplayMessage("NEXTMESSAGETEMP");
	DisplayMessage("Making Dag Tree");
	/////////////////////////////////////////////////
	//if (this -> moverMap != model -> uMap) goto setFields;	// don't try to create a map
	//goto setFields;
	/////////////////////////////////////////////////
	// go through topo look for -1, and list corresponding boundary sides
	// then reorder as contiguous boundary segments - need to group boundary rects by islands
	// will need a new field for list of boundary points since there can be duplicates, can't just order and list segment endpoints
	
/*	nSegs = 2*ntri; //number of -1's in topo
	boundaryPtsH = (LONGH)_NewHandleClear(nv_ext * sizeof(**boundaryPtsH));
	boundaryEndPtsH = (LONGH)_NewHandleClear(nv_ext * sizeof(**boundaryEndPtsH));
	waterBoundaryPtsH = (LONGH)_NewHandleClear(nv_ext * sizeof(**waterBoundaryPtsH));
	flagH = (LONGH)_NewHandleClear(nv_ext * sizeof(**flagH));
	segUsed = (Boolean**)_NewHandleClear(nSegs * sizeof(Boolean));
	segList = (SegInfoHdl)_NewHandleClear(nSegs * sizeof(**segList));
	// first go through rectangles and group by island
	// do this before making dagtree, 
	DisplayMessage("NEXTMESSAGETEMP");
	DisplayMessage("Numbering Islands");
	MySpinCursor(); // JLM 8/4/99
	//err = NumberIslands(&maskH2, velocityH, landWaterInfo, fNumRows, fNumCols, &numIslands);	// numbers start at 3 (outer boundary)
	err = NumberIslands(&maskH2, landmaskH, landWaterInfo, fNumRows, fNumCols, &numIslands);	// numbers start at 3 (outer boundary)
	MySpinCursor(); // JLM 8/4/99
	if (err) goto done;
	for (i=0;i<ntri;i++)
	{
		if ((i+1)%2==0) isOdd = 0; else isOdd = 1;
		// the middle neighbor triangle is always the other half of the rectangle so can't be land or outside the map
		// odd - left/top, even - bottom/right the 1-2 segment is top/bot, the 2-3 segment is right/left
		if ((*topo)[i].adjTri1 == -1)
		{
			// add segment pt 2 - pt 3 to list, need points, triNum and whether it's L/W boundary (boundary num)
			(*segList)[segNum].pt1 = (*topo)[i].vertex2;
			(*segList)[segNum].pt2 = (*topo)[i].vertex3;
			// check which land block this segment borders and mark the island
			if (isOdd) 
			{
				// check left rectangle for L/W border 
				rectIndex = INDEXH(verdatPtsH,(*topo)[i].vertex3);	// to get back into original grid for L/W info - use maskH2
				iIndex = rectIndex/fNumCols_ext;
				jIndex = rectIndex%fNumCols_ext;
				if (jIndex>0 && INDEXH(maskH2,iIndex*fNumCols_ext + jIndex-1)>=3)
				{
					(*segList)[segNum].isWater = 0;
					(*segList)[segNum].islandNumber = INDEXH(maskH2,iIndex*fNumCols_ext + jIndex-1);	
				}
				else
				{
					(*segList)[segNum].isWater = 1;
					(*segList)[segNum].islandNumber = 1;	
				}
			}
			else 
			{	
				// check right rectangle for L/W border convert back to row/col
				rectIndex = INDEXH(verdatPtsH,(*topo)[i].vertex1);
				iIndex = rectIndex/fNumCols_ext;
				jIndex = rectIndex%fNumCols_ext;
				if (jIndex<fNumCols && INDEXH(maskH2,iIndex*fNumCols_ext + jIndex+1)>=3)
				{
					(*segList)[segNum].isWater = 0;
					(*segList)[segNum].islandNumber = INDEXH(maskH2,iIndex*fNumCols_ext + jIndex+1);	
				}
				else
				{
					(*segList)[segNum].isWater = 1;
					(*segList)[segNum].islandNumber = 1;	
				}
			}
			segNum++;
		}
		
		if ((*topo)[i].adjTri3 == -1)
		{
			// add segment pt 1 - pt 2 to list
			// odd top, even bottom
			(*segList)[segNum].pt1 = (*topo)[i].vertex1;
			(*segList)[segNum].pt2 = (*topo)[i].vertex2;
			// check which land block this segment borders and mark the island
			if (isOdd) 
			{
				// check top rectangle for L/W border
				rectIndex = INDEXH(verdatPtsH,(*topo)[i].vertex3);	// to get back into original grid for L/W info - use maskH2
				iIndex = rectIndex/fNumCols_ext;
				jIndex = rectIndex%fNumCols_ext;
				if (iIndex>0 && INDEXH(maskH2,(iIndex-1)*fNumCols_ext + jIndex)>=3)
				{
					(*segList)[segNum].isWater = 0;
					(*segList)[segNum].islandNumber = INDEXH(maskH2,(iIndex-1)*fNumCols_ext + jIndex);
				}
				else
				{
					(*segList)[segNum].isWater = 1;
					(*segList)[segNum].islandNumber = 1;
				}
			}
			else 
			{
				// check bottom rectangle for L/W border
				rectIndex = INDEXH(verdatPtsH,(*topo)[i].vertex1);
				iIndex = rectIndex/fNumCols_ext;
				jIndex = rectIndex%fNumCols_ext;
				if (iIndex<fNumRows && INDEXH(maskH2,(iIndex+1)*fNumCols_ext + jIndex)>=3)
				{
					(*segList)[segNum].isWater = 0;
					(*segList)[segNum].islandNumber = INDEXH(maskH2,(iIndex+1)*fNumCols_ext + jIndex);		// this should be the neighbor's value
				}
				else
				{
					(*segList)[segNum].isWater = 1;
					(*segList)[segNum].islandNumber = 1;		
				}
			}
			segNum++;
		}
	}
	nSegs = segNum;
	_SetHandleSize((Handle)segList,nSegs*sizeof(**segList));
	_SetHandleSize((Handle)segUsed,nSegs*sizeof(**segUsed));
	// go through list of segments, and make list of boundary segments
	// as segment is taken mark so only use each once
	// get a starting point, add the first and second to the list
	islandNum = 3;
findnewstartpoint:
	if (islandNum > numIslands) 
	{
		_SetHandleSize((Handle)boundaryPtsH,nBoundaryPts*sizeof(**boundaryPtsH));
		_SetHandleSize((Handle)waterBoundaryPtsH,nBoundaryPts*sizeof(**waterBoundaryPtsH));
		_SetHandleSize((Handle)boundaryEndPtsH,nEndPts*sizeof(**boundaryEndPtsH));
		goto setFields;	// off by 2 - 0,1,2 are water cells, 3 and up are land
	}
	foundPt = false;
	for (i=0;i<nSegs;i++)
	{
		if ((*segUsed)[i]) continue;
		waterStartPoint = nBoundaryPts;
		(*boundaryPtsH)[nBoundaryPts++] = (*segList)[i].pt1;
		(*flagH)[(*segList)[i].pt1] = 1;
		(*waterBoundaryPtsH)[nBoundaryPts] = (*segList)[i].isWater+1;
		(*boundaryPtsH)[nBoundaryPts++] = (*segList)[i].pt2;
		(*flagH)[(*segList)[i].pt2] = 1;
		currentIndex = (*segList)[i].pt2;
		startIndex = (*segList)[i].pt1;
		currentIsland = (*segList)[i].islandNumber;	
		foundPt = true;
		(*segUsed)[i] = true;
		break;
	}
	if (!foundPt)
	{
		printNote("Lost trying to set boundaries");
		// clean up handles and set grid without a map
		if (boundaryPtsH) {DisposeHandle((Handle)boundaryPtsH); boundaryPtsH = 0;}
		if (boundaryEndPtsH) {DisposeHandle((Handle)boundaryEndPtsH); boundaryEndPtsH = 0;}
		if (waterBoundaryPtsH) {DisposeHandle((Handle)waterBoundaryPtsH); waterBoundaryPtsH = 0;}
		goto setFields;
	}
	
findnextpoint:
	for (i=0;i<nSegs;i++)
	{
		// look for second point of the previous selected segment, add the second to point list
		if ((*segUsed)[i]) continue;
		if ((*segList)[i].islandNumber > 3 && (*segList)[i].islandNumber != currentIsland) continue;
		if ((*segList)[i].islandNumber > 3 && currentIsland <= 3) continue;
		index = (*segList)[i].pt1;
		if (index == currentIndex)	// found next point
		{
			currentIndex = (*segList)[i].pt2;
			(*segUsed)[i] = true;
			if (currentIndex == startIndex) // completed a segment
			{
				islandNum++;
				(*boundaryEndPtsH)[nEndPts++] = nBoundaryPts-1;
				(*waterBoundaryPtsH)[waterStartPoint] = (*segList)[i].isWater+1;	// need to deal with this
				goto findnewstartpoint;
			}
			else
			{
				(*boundaryPtsH)[nBoundaryPts] = (*segList)[i].pt2;
				(*flagH)[(*segList)[i].pt2] = 1;
				(*waterBoundaryPtsH)[nBoundaryPts] = (*segList)[i].isWater+1;
				nBoundaryPts++;
				goto findnextpoint;
			}
		}
	}
	// shouldn't get here unless there's a problem...
	_SetHandleSize((Handle)boundaryPtsH,nBoundaryPts*sizeof(**boundaryPtsH));
	_SetHandleSize((Handle)waterBoundaryPtsH,nBoundaryPts*sizeof(**waterBoundaryPtsH));
	_SetHandleSize((Handle)boundaryEndPtsH,nEndPts*sizeof(**boundaryEndPtsH));
*/	
//setFields:	
	
	fVerdatToNetCDFH = verdatPtsH;
	
	
	/////////////////////////////////////////////////
	
	triGrid = new TTriGridVel;
	if (!triGrid)
	{		
		err = true;
		TechError("Error in TimeGridVelCurv_c::ReorderPoints()","new TTriGridVel",err);
		goto done;
	}
	
	fGrid = (TTriGridVel*)triGrid;
	
	this->SetGridBounds(triBounds);
	triGrid -> SetBounds(triBounds); 
	
	MySpinCursor(); // JLM 8/4/99
	tree = MakeDagTree(topo, (LongPoint**)pts, errmsg); 
	MySpinCursor(); // JLM 8/4/99
	if (errmsg[0])	
	{err = -1; goto done;} 
	// sethandle size of the fTreeH to be tree.fNumBranches, the rest are zeros
	_SetHandleSize((Handle)tree.treeHdl,tree.numBranches*sizeof(DAG));
	dagTree = new TDagTree(pts,topo,tree.treeHdl,velH,tree.numBranches); 
	if(!dagTree)
	{
		err = -1;
		printError("Unable to create dag tree.");
		goto done;
	}
	
	triGrid -> SetDagTree(dagTree);
	//triGrid -> SetDepths(totalDepthH);	// used by PtCurMap to check vertical movement
	
	pts = 0;	// because fGrid is now responsible for it
	topo = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	tree.treeHdl = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	
	// code goes here, do we want to store the grid boundary and land/water information?
	/*if (waterBoundaryPtsH)	
	{
		PtCurMap *map = CreateAndInitPtCurMap(fVar.pathName,triBounds); // the map bounds are the same as the grid bounds
		if (!map) {err=-1; goto done;}
		// maybe move up and have the map read in the boundary information
		map->SetBoundarySegs(boundaryEndPtsH);	
		map->SetWaterBoundaries(waterBoundaryPtsH);
		map->SetBoundaryPoints(boundaryPtsH);
		
		*newMap = map;
	}
	else*/
	{
		//if (waterBoundaryPtsH) {DisposeHandle((Handle)waterBoundaryPtsH); waterBoundaryPtsH=0;}
		//if (boundaryEndPtsH) {DisposeHandle((Handle)boundaryEndPtsH); boundaryEndPtsH=0;}
		//if (boundaryPtsH) {DisposeHandle((Handle)boundaryPtsH); boundaryPtsH=0;}
	}
	
	/////////////////////////////////////////////////
done:
	if (landWaterInfo) {DisposeHandle((Handle)landWaterInfo); landWaterInfo=0;}
	if (ptIndexHdl) {DisposeHandle((Handle)ptIndexHdl); ptIndexHdl = 0;}
	if (gridCellInfo) {DisposeHandle((Handle)gridCellInfo); gridCellInfo = 0;}
	//if (segUsed) {DisposeHandle((Handle)segUsed); segUsed = 0;}
	//if (segList) {DisposeHandle((Handle)segList); segList = 0;}
	//if (flagH) {DisposeHandle((Handle)flagH); flagH = 0;}
	
	if(err)
	{
		if(!errmsg[0])
			strcpy(errmsg,"An error occurred in TimeGridVelCurv_c::ReorderPoints");
		printError(errmsg); 
		if(pts) {DisposeHandle((Handle)pts); pts=0;}
		if(topo) {DisposeHandle((Handle)topo); topo=0;}
		if(velH) {DisposeHandle((Handle)velH); velH=0;}
		if(tree.treeHdl) {DisposeHandle((Handle)tree.treeHdl); tree.treeHdl=0;}
		
		if(fGrid)
		{
			fGrid ->Dispose();
			delete fGrid;
			fGrid = 0;
		}
		if (landWaterInfo) {DisposeHandle((Handle)landWaterInfo); landWaterInfo=0;}
		if (ptIndexHdl) {DisposeHandle((Handle)ptIndexHdl); ptIndexHdl = 0;}
		if (gridCellInfo) {DisposeHandle((Handle)gridCellInfo); gridCellInfo = 0;}
		if (verdatPtsH) {DisposeHandle((Handle)verdatPtsH); verdatPtsH = 0;}
		//if (maskH2) {DisposeHandle((Handle)maskH2); maskH2 = 0;}
		
		//if (boundaryPtsH) {DisposeHandle((Handle)boundaryPtsH); boundaryPtsH = 0;}
		//if (boundaryEndPtsH) {DisposeHandle((Handle)boundaryEndPtsH); boundaryEndPtsH = 0;}
		//if (waterBoundaryPtsH) {DisposeHandle((Handle)waterBoundaryPtsH); waterBoundaryPtsH = 0;}
	}
	return err;
}

OSErr TimeGridVelCurv_c::ReorderPointsNoMask(char* errmsg) 
{
	long i, j, n, ntri, numVerdatPts=0;
	long fNumRows_ext = fNumRows+1, fNumCols_ext = fNumCols+1;
	long nv = fNumRows * fNumCols, nv_ext = fNumRows_ext*fNumCols_ext;
	long iIndex, jIndex, index; 
	long triIndex1, triIndex2, waterCellNum=0;
	long ptIndex = 0, cellNum = 0;
	long indexOfStart = 0;
	OSErr err = 0;
	
	LONGH landWaterInfo = (LONGH)_NewHandleClear(fNumRows * fNumCols * sizeof(long));
	
	LONGH ptIndexHdl = (LONGH)_NewHandleClear(nv_ext * sizeof(**ptIndexHdl));
	LONGH verdatPtsH = (LONGH)_NewHandleClear(nv_ext * sizeof(**verdatPtsH));
	GridCellInfoHdl gridCellInfo = (GridCellInfoHdl)_NewHandleClear(nv * sizeof(**gridCellInfo));
	
	TopologyHdl topo=0;
	LongPointHdl pts=0;
	VelocityFH velH = 0;
	DAGTreeStruct tree;
	WorldRect triBounds;
	
	TTriGridVel *triGrid = nil;
	tree.treeHdl = 0;
	TDagTree *dagTree = 0;
	
	VelocityFH velocityH = 0;
	/////////////////////////////////////////////////
	
	
	if (!landWaterInfo || !ptIndexHdl || !gridCellInfo || !verdatPtsH) {err = memFullErr; goto done;}
	
	err = this -> ReadTimeData(indexOfStart,&velocityH,errmsg);	// try to use velocities to set grid
	
	for (i=0;i<fNumRows;i++)
	{
		for (j=0;j<fNumCols;j++)
		{
			// no land/water mask so assume land is zero velocity points
			if (INDEXH(velocityH,i*fNumCols+j).u==0 && INDEXH(velocityH,i*fNumCols+j).v==0)	// land point
			{
				INDEXH(landWaterInfo,i*fNumCols+j) = -1;	// may want to mark each separate island with a unique number
			}
			else
			{
				INDEXH(landWaterInfo,i*fNumCols+j) = 1;
				INDEXH(ptIndexHdl,i*fNumCols_ext+j) = -2;	// water box
				INDEXH(ptIndexHdl,i*fNumCols_ext+j+1) = -2;
				INDEXH(ptIndexHdl,(i+1)*fNumCols_ext+j) = -2;
				INDEXH(ptIndexHdl,(i+1)*fNumCols_ext+j+1) = -2;
			}
		}
	}
	
	for (i=0;i<fNumRows_ext;i++)
	{
		for (j=0;j<fNumCols_ext;j++)
		{
			if (INDEXH(ptIndexHdl,i*fNumCols_ext+j) == -2)
			{
				INDEXH(ptIndexHdl,i*fNumCols_ext+j) = ptIndex;	// count up grid points
				ptIndex++;
			}
			else
				INDEXH(ptIndexHdl,i*fNumCols_ext+j) = -1;
		}
	}
	
	for (i=0;i<fNumRows;i++)
	{
		for (j=0;j<fNumCols;j++)
		{
			if (INDEXH(landWaterInfo,i*fNumCols+j)>0)
			{
				INDEXH(gridCellInfo,i*fNumCols+j).cellNum = cellNum;
				cellNum++;
				INDEXH(gridCellInfo,i*fNumCols+j).topLeft = INDEXH(ptIndexHdl,i*fNumCols_ext+j);
				INDEXH(gridCellInfo,i*fNumCols+j).topRight = INDEXH(ptIndexHdl,i*fNumCols_ext+j+1);
				INDEXH(gridCellInfo,i*fNumCols+j).bottomLeft = INDEXH(ptIndexHdl,(i+1)*fNumCols_ext+j);
				INDEXH(gridCellInfo,i*fNumCols+j).bottomRight = INDEXH(ptIndexHdl,(i+1)*fNumCols_ext+j+1);
			}
			else INDEXH(gridCellInfo,i*fNumCols+j).cellNum = -1;
		}
	}
	ntri = cellNum*2;	// each water cell is split into two triangles
	if(!(topo = (TopologyHdl)_NewHandleClear(ntri * sizeof(Topology)))){err = memFullErr; goto done;}	
	for (i=0;i<nv_ext;i++)
	{
		if (INDEXH(ptIndexHdl,i) != -1)
		{
			INDEXH(verdatPtsH,numVerdatPts) = i;
			//INDEXH(verdatPtsH,INDEXH(ptIndexHdl,i)) = i;
			numVerdatPts++;
		}
	}
	_SetHandleSize((Handle)verdatPtsH,numVerdatPts*sizeof(**verdatPtsH));
	pts = (LongPointHdl)_NewHandle(sizeof(LongPoint)*(numVerdatPts));
	if(pts == nil)
	{
		strcpy(errmsg,"Not enough memory to triangulate data.");
		return -1;
	}
	
	/////////////////////////////////////////////////
	//index = 0;
	for (i=0; i<=numVerdatPts; i++)	// make a list of grid points that will be used for triangles
	{
		float fLong, fLat, /*fDepth, */dLon, dLat, dLon1, dLon2, dLat1, dLat2;
		double val, u=0., v=0.;
		LongPoint vertex;
		
		if(i < numVerdatPts) 
		{	// since velocities are defined at the lower left corner of each grid cell
			// need to add an extra row/col at the top/right of the grid
			// set lat/lon based on distance between previous two points 
			// these are just for boundary/drawing purposes, velocities are set to zero
			//index = i+1;
			n = INDEXH(verdatPtsH,i);
			iIndex = n/fNumCols_ext;
			jIndex = n%fNumCols_ext;
			if (iIndex==0)
			{
				if (jIndex<fNumCols)
				{
					dLat = INDEXH(fVertexPtsH,fNumCols+jIndex).pLat - INDEXH(fVertexPtsH,jIndex).pLat;
					fLat = INDEXH(fVertexPtsH,jIndex).pLat - dLat;
					dLon = INDEXH(fVertexPtsH,fNumCols+jIndex).pLong - INDEXH(fVertexPtsH,jIndex).pLong;
					fLong = INDEXH(fVertexPtsH,jIndex).pLong - dLon;
				}
				else
				{
					dLat1 = (INDEXH(fVertexPtsH,jIndex-1).pLat - INDEXH(fVertexPtsH,jIndex-2).pLat);
					dLat2 = INDEXH(fVertexPtsH,fNumCols+jIndex-1).pLat - INDEXH(fVertexPtsH,fNumCols+jIndex-2).pLat;
					fLat = 2*(INDEXH(fVertexPtsH,jIndex-1).pLat + dLat1)-(INDEXH(fVertexPtsH,fNumCols+jIndex-1).pLat+dLat2);
					dLon1 = INDEXH(fVertexPtsH,fNumCols+jIndex-1).pLong - INDEXH(fVertexPtsH,jIndex-1).pLong;
					dLon2 = (INDEXH(fVertexPtsH,fNumCols+jIndex-2).pLong - INDEXH(fVertexPtsH,jIndex-2).pLong);
					fLong = 2*(INDEXH(fVertexPtsH,jIndex-1).pLong-dLon1)-(INDEXH(fVertexPtsH,jIndex-2).pLong-dLon2);
				}
			}
			else 
			{
				if (jIndex<fNumCols)
				{
					fLat = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex).pLat;
					fLong = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex).pLong;
					u = INDEXH(velocityH,(iIndex-1)*fNumCols+jIndex).u;
					v = INDEXH(velocityH,(iIndex-1)*fNumCols+jIndex).v;
				}
				else
				{
					dLat = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-1).pLat - INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-2).pLat;
					fLat = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-1).pLat + dLat;
					dLon = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-1).pLong - INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-2).pLong;
					fLong = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-1).pLong + dLon;
				}
			}
			vertex.v = (long)(fLat*1e6);
			vertex.h = (long)(fLong*1e6);
			
			//fDepth = 1.;
			INDEXH(pts,i) = vertex;
		}
		else { // for outputting a verdat the last line should be all zeros
			//index = 0;
			//fLong = fLat = fDepth = 0.0;
		}
		/////////////////////////////////////////////////
		
	}

	// figure out the bounds
	triBounds = voidWorldRect;
	if(pts) 
	{
		LongPoint	thisLPoint;
		
		if(numVerdatPts > 0)
		{
			WorldPoint  wp;
			for(i=0;i<numVerdatPts;i++)
			{
				thisLPoint = INDEXH(pts,i);
				wp.pLat = thisLPoint.v;
				wp.pLong = thisLPoint.h;
				AddWPointToWRect(wp.pLat, wp.pLong, &triBounds);
			}
		}
	}
	
	DisplayMessage("NEXTMESSAGETEMP");
	DisplayMessage("Making Triangles");
	
	/////////////////////////////////////////////////
	for (i=0;i<fNumRows;i++)
	{
		for (j=0;j<fNumCols;j++)
		{
			if (INDEXH(landWaterInfo,i*fNumCols+j)==-1)
				continue;
			waterCellNum = INDEXH(gridCellInfo,i*fNumCols+j).cellNum;	// split each cell into 2 triangles
			triIndex1 = 2*waterCellNum;
			triIndex2 = 2*waterCellNum+1;
			// top/left tri in rect
			(*topo)[triIndex1].vertex1 = INDEXH(gridCellInfo,i*fNumCols+j).topRight;
			(*topo)[triIndex1].vertex2 = INDEXH(gridCellInfo,i*fNumCols+j).topLeft;
			(*topo)[triIndex1].vertex3 = INDEXH(gridCellInfo,i*fNumCols+j).bottomLeft;
			if (j==0 || INDEXH(gridCellInfo,i*fNumCols+j-1).cellNum == -1)
				(*topo)[triIndex1].adjTri1 = -1;
			else
			{
				(*topo)[triIndex1].adjTri1 = INDEXH(gridCellInfo,i*fNumCols+j-1).cellNum * 2 + 1;
			}
			(*topo)[triIndex1].adjTri2 = triIndex2;
			if (i==0 || INDEXH(gridCellInfo,(i-1)*fNumCols+j).cellNum==-1)
				(*topo)[triIndex1].adjTri3 = -1;
			else
			{
				(*topo)[triIndex1].adjTri3 = INDEXH(gridCellInfo,(i-1)*fNumCols+j).cellNum * 2 + 1;
			}
			// bottom/right tri in rect
			(*topo)[triIndex2].vertex1 = INDEXH(gridCellInfo,i*fNumCols+j).bottomLeft;
			(*topo)[triIndex2].vertex2 = INDEXH(gridCellInfo,i*fNumCols+j).bottomRight;
			(*topo)[triIndex2].vertex3 = INDEXH(gridCellInfo,i*fNumCols+j).topRight;
			if (j==fNumCols-1 || INDEXH(gridCellInfo,i*fNumCols+j+1).cellNum == -1)
				(*topo)[triIndex2].adjTri1 = -1;
			else
			{
				(*topo)[triIndex2].adjTri1 = INDEXH(gridCellInfo,i*fNumCols+j+1).cellNum * 2;
			}
			(*topo)[triIndex2].adjTri2 = triIndex1;
			if (i==fNumRows-1 || INDEXH(gridCellInfo,(i+1)*fNumCols+j).cellNum == -1)
				(*topo)[triIndex2].adjTri3 = -1;
			else
			{
				(*topo)[triIndex2].adjTri3 = INDEXH(gridCellInfo,(i+1)*fNumCols+j).cellNum * 2;
			}
		}
	}
	
	DisplayMessage("NEXTMESSAGETEMP");
	DisplayMessage("Making Dag Tree");
	MySpinCursor(); // JLM 8/4/99
	tree = MakeDagTree(topo, (LongPoint**)pts, errmsg); 
	MySpinCursor(); // JLM 8/4/99
	if (errmsg[0])	
	{err = -1; goto done;} 
	// sethandle size of the fTreeH to be tree.fNumBranches, the rest are zeros
	_SetHandleSize((Handle)tree.treeHdl,tree.numBranches*sizeof(DAG));
	/////////////////////////////////////////////////
	
	fVerdatToNetCDFH = verdatPtsH;
	
	/////////////////////////////////////////////////
	
	triGrid = new TTriGridVel;
	if (!triGrid)
	{		
		err = true;
		TechError("Error in TimeGridVelCurv_c::ReorderPoints()","new TTriGridVel",err);
		goto done;
	}
	
	fGrid = (TTriGridVel*)triGrid;
	
	triGrid -> SetBounds(triBounds); 
	this->SetGridBounds(triBounds);
	dagTree = new TDagTree(pts,topo,tree.treeHdl,velH,tree.numBranches); 
	if(!dagTree)
	{
		err = -1;
		printError("Unable to create dag tree.");
		goto done;
	}
	
	triGrid -> SetDagTree(dagTree);
	//triGrid -> SetDepths(totalDepthH);	// used by PtCurMap to check vertical movement
	
	pts = 0;	// because fGrid is now responsible for it
	topo = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	tree.treeHdl = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	
	/////////////////////////////////////////////////
done:
	if (landWaterInfo) {DisposeHandle((Handle)landWaterInfo); landWaterInfo=0;}
	if (ptIndexHdl) {DisposeHandle((Handle)ptIndexHdl); ptIndexHdl = 0;}
	if (gridCellInfo) {DisposeHandle((Handle)gridCellInfo); gridCellInfo = 0;}
	
	if(err)
	{
		if(!errmsg[0])
			strcpy(errmsg,"An error occurred in TimeGridVelCurv_c::ReorderPoints");
		printError(errmsg); 
		if(pts) {DisposeHandle((Handle)pts); pts=0;}
		if(topo) {DisposeHandle((Handle)topo); topo=0;}
		if(velH) {DisposeHandle((Handle)velH); velH=0;}
		if(tree.treeHdl) {DisposeHandle((Handle)tree.treeHdl); tree.treeHdl=0;}
		
		if(fGrid)
		{
			fGrid ->Dispose();
			delete fGrid;
			fGrid = 0;
		}
		if (verdatPtsH) {DisposeHandle((Handle)verdatPtsH); verdatPtsH = 0;}
	}
	if (velocityH) {DisposeHandle((Handle)velocityH); velocityH = 0;}
	return err;
}

OSErr TimeGridVelCurv_c::ReorderPointsCOOPSMask(DOUBLEH landmaskH, char* errmsg) 
{
	OSErr err = 0;
	long i,j,k;
	char *velUnits=0; 
	long latlength = fNumRows, numtri = 0;
	long lonlength = fNumCols;
	float fDepth1, fLat1, fLong1;
	long index1=0;
	
	errmsg[0]=0;
	
	long n, ntri, numVerdatPts=0;
	long fNumRows_minus1 = fNumRows-1, fNumCols_minus1 = fNumCols-1;
	long nv = fNumRows * fNumCols;
	long nCells = fNumRows_minus1 * fNumCols_minus1;
	long iIndex, jIndex, index; 
	long triIndex1, triIndex2, waterCellNum=0;
	long ptIndex = 0, cellNum = 0;
	
	//long currentIsland=0, islandNum, nBoundaryPts=0, nEndPts=0, waterStartPoint;
	//long nSegs, segNum = 0, numIslands, rectIndex; 
	//long currentIndex,startIndex; 
	//long diag = 1;
	//Boolean foundPt = false, isOdd;
	
	LONGH landWaterInfo = (LONGH)_NewHandleClear(nCells * sizeof(long));
	//LONGH maskH2 = (LONGH)_NewHandleClear(nv * sizeof(long));
	
	LONGH ptIndexHdl = (LONGH)_NewHandleClear(nv * sizeof(**ptIndexHdl));
	LONGH verdatPtsH = (LONGH)_NewHandleClear(nv * sizeof(**verdatPtsH));
	GridCellInfoHdl gridCellInfo = (GridCellInfoHdl)_NewHandleClear(nCells * sizeof(**gridCellInfo));
	
	TopologyHdl topo=0;
	LongPointHdl pts=0;
	VelocityFH velH = 0;
	DAGTreeStruct tree;
	WorldRect triBounds;
	
	//LONGH boundaryPtsH = 0;
	//LONGH boundaryEndPtsH = 0;
	//LONGH waterBoundaryPtsH = 0;
	//Boolean** segUsed = 0;
	//SegInfoHdl segList = 0;
	//LONGH flagH = 0;
	
	TTriGridVel *triGrid = nil;
	tree.treeHdl = 0;
	TDagTree *dagTree = 0;
	
	/////////////////////////////////////////////////
	
	if (!landmaskH) return -1;
	
	if (!landWaterInfo || !ptIndexHdl || !gridCellInfo || !verdatPtsH /*|| !maskH2*/) {err = memFullErr; goto done;}
	
	index1 = 0;
	for (i=0;i<fNumRows-1;i++)
	{
		for (j=0;j<fNumCols-1;j++)
		{
			if (INDEXH(landmaskH,i*fNumCols+j)==0)	// land point
			{
				INDEXH(landWaterInfo,i*fNumCols_minus1+j) = -1;	// may want to mark each separate island with a unique number
			}
			else
			{
				if (INDEXH(landmaskH,(i+1)*fNumCols+j)==0 || INDEXH(landmaskH,i*fNumCols+j+1)==0 || INDEXH(landmaskH,(i+1)*fNumCols+j+1)==0)
				{
					INDEXH(landWaterInfo,i*fNumCols_minus1+j) = -1;	// may want to mark each separate island with a unique number
				}
				else
				{
					INDEXH(landWaterInfo,i*fNumCols_minus1+j) = 1;
					INDEXH(ptIndexHdl,i*fNumCols+j) = -2;	// water box
					INDEXH(ptIndexHdl,i*fNumCols+j+1) = -2;
					INDEXH(ptIndexHdl,(i+1)*fNumCols+j) = -2;
					INDEXH(ptIndexHdl,(i+1)*fNumCols+j+1) = -2;
				}
			}
		}
	}
	
	for (i=0;i<fNumRows;i++)
	{
		for (j=0;j<fNumCols;j++)
		{
			if (INDEXH(ptIndexHdl,i*fNumCols+j) == -2)
			{
				INDEXH(ptIndexHdl,i*fNumCols+j) = ptIndex;	// count up grid points
				ptIndex++;
			}
			else
				INDEXH(ptIndexHdl,i*fNumCols+j) = -1;
		}
	}
	
	for (i=0;i<fNumRows-1;i++)
	{
		for (j=0;j<fNumCols-1;j++)
		{
			if (INDEXH(landWaterInfo,i*fNumCols_minus1+j)>0)
			{
				INDEXH(gridCellInfo,i*fNumCols_minus1+j).cellNum = cellNum;
				cellNum++;
				INDEXH(gridCellInfo,i*fNumCols_minus1+j).topLeft = INDEXH(ptIndexHdl,i*fNumCols+j);
				INDEXH(gridCellInfo,i*fNumCols_minus1+j).topRight = INDEXH(ptIndexHdl,i*fNumCols+j+1);
				INDEXH(gridCellInfo,i*fNumCols_minus1+j).bottomLeft = INDEXH(ptIndexHdl,(i+1)*fNumCols+j);
				INDEXH(gridCellInfo,i*fNumCols_minus1+j).bottomRight = INDEXH(ptIndexHdl,(i+1)*fNumCols+j+1);
			}
			else INDEXH(gridCellInfo,i*fNumCols_minus1+j).cellNum = -1;
		}
	}
	ntri = cellNum*2;	// each water cell is split into two triangles
	if(!(topo = (TopologyHdl)_NewHandleClear(ntri * sizeof(Topology)))){err = memFullErr; goto done;}	
	for (i=0;i<nv;i++)
	{
		if (INDEXH(ptIndexHdl,i) != -1)
		{
			INDEXH(verdatPtsH,numVerdatPts) = i;
			numVerdatPts++;
		}
	}
	_SetHandleSize((Handle)verdatPtsH,numVerdatPts*sizeof(**verdatPtsH));
	pts = (LongPointHdl)_NewHandle(sizeof(LongPoint)*(numVerdatPts));
	if(pts == nil)
	{
		strcpy(errmsg,"Not enough memory to triangulate data.");
		return -1;
	}
	
	/////////////////////////////////////////////////
	//index = 0;
	for (i=0; i<=numVerdatPts; i++)	// make a list of grid points that will be used for triangles
	{
		float fLong, fLat, /*fDepth,*/ dLon, dLat, dLon1, dLon2, dLat1, dLat2;
		double val, u=0., v=0.;
		LongPoint vertex;
		
		if(i < numVerdatPts) 
		{	// since velocities are defined at the lower left corner of each grid cell
			// need to add an extra row/col at the top/right of the grid
			// set lat/lon based on distance between previous two points 
			// these are just for boundary/drawing purposes, velocities are set to zero
			index = i+1;
			n = INDEXH(verdatPtsH,i);
			iIndex = n/fNumCols;
			jIndex = n%fNumCols;
			//fLat = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex).pLat;
			//fLong = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex).pLong;
			fLat = INDEXH(fVertexPtsH,(iIndex)*fNumCols+jIndex).pLat;
			fLong = INDEXH(fVertexPtsH,(iIndex)*fNumCols+jIndex).pLong;

			vertex.v = (long)(fLat*1e6);
			vertex.h = (long)(fLong*1e6);
			
			//fDepth = 1.;
			INDEXH(pts,i) = vertex;
		}
		else { // for outputting a verdat the last line should be all zeros
			//index = 0;
			//fLong = fLat = fDepth = 0.0;
		}
		/////////////////////////////////////////////////
		
	}
	// figure out the bounds
	triBounds = voidWorldRect;
	if(pts) 
	{
		LongPoint	thisLPoint;
		
		if(numVerdatPts > 0)
		{
			WorldPoint  wp;
			for(i=0;i<numVerdatPts;i++)
			{
				thisLPoint = INDEXH(pts,i);
				wp.pLat = thisLPoint.v;
				wp.pLong = thisLPoint.h;
				AddWPointToWRect(wp.pLat, wp.pLong, &triBounds);
			}
		}
	}
	
	DisplayMessage("NEXTMESSAGETEMP");
	DisplayMessage("Making Triangles");
	
	/////////////////////////////////////////////////
	for (i=0;i<fNumRows_minus1;i++)
	{
		for (j=0;j<fNumCols_minus1;j++)
		{
			if (INDEXH(landWaterInfo,i*fNumCols_minus1+j)==-1)
				continue;
			waterCellNum = INDEXH(gridCellInfo,i*fNumCols_minus1+j).cellNum;	// split each cell into 2 triangles
			triIndex1 = 2*waterCellNum;
			triIndex2 = 2*waterCellNum+1;
			// top/left tri in rect
			(*topo)[triIndex1].vertex1 = INDEXH(gridCellInfo,i*fNumCols_minus1+j).topRight;
			(*topo)[triIndex1].vertex2 = INDEXH(gridCellInfo,i*fNumCols_minus1+j).topLeft;
			(*topo)[triIndex1].vertex3 = INDEXH(gridCellInfo,i*fNumCols_minus1+j).bottomLeft;
			if (j==0 || INDEXH(gridCellInfo,i*fNumCols_minus1+j-1).cellNum == -1)
				(*topo)[triIndex1].adjTri1 = -1;
			else
			{
				(*topo)[triIndex1].adjTri1 = INDEXH(gridCellInfo,i*fNumCols_minus1+j-1).cellNum * 2 + 1;
			}
			(*topo)[triIndex1].adjTri2 = triIndex2;
			if (i==0 || INDEXH(gridCellInfo,(i-1)*fNumCols_minus1+j).cellNum==-1)
				(*topo)[triIndex1].adjTri3 = -1;
			else
			{
				(*topo)[triIndex1].adjTri3 = INDEXH(gridCellInfo,(i-1)*fNumCols_minus1+j).cellNum * 2 + 1;
			}
			// bottom/right tri in rect
			(*topo)[triIndex2].vertex1 = INDEXH(gridCellInfo,i*fNumCols_minus1+j).bottomLeft;
			(*topo)[triIndex2].vertex2 = INDEXH(gridCellInfo,i*fNumCols_minus1+j).bottomRight;
			(*topo)[triIndex2].vertex3 = INDEXH(gridCellInfo,i*fNumCols_minus1+j).topRight;
			if (j==fNumCols-2 || INDEXH(gridCellInfo,i*fNumCols_minus1+j+1).cellNum == -1)
				(*topo)[triIndex2].adjTri1 = -1;
			else
			{
				(*topo)[triIndex2].adjTri1 = INDEXH(gridCellInfo,i*fNumCols_minus1+j+1).cellNum * 2;
			}
			(*topo)[triIndex2].adjTri2 = triIndex1;
			if (i==fNumRows-2 || INDEXH(gridCellInfo,(i+1)*fNumCols_minus1+j).cellNum == -1)
				(*topo)[triIndex2].adjTri3 = -1;
			else
			{
				(*topo)[triIndex2].adjTri3 = INDEXH(gridCellInfo,(i+1)*fNumCols_minus1+j).cellNum * 2;
			}
		}
	}
	
	DisplayMessage("NEXTMESSAGETEMP");
	DisplayMessage("Making Dag Tree");
	MySpinCursor(); // JLM 8/4/99
	tree = MakeDagTree(topo, (LongPoint**)pts, errmsg); 
	MySpinCursor(); // JLM 8/4/99
	if (errmsg[0])	
	{err = -1; goto done;} 
	// sethandle size of the fTreeH to be tree.fNumBranches, the rest are zeros
	_SetHandleSize((Handle)tree.treeHdl,tree.numBranches*sizeof(DAG));
	/////////////////////////////////////////////////
	
	/////////////////////////////////////////////////
	//if (this -> moverMap != model -> uMap) goto setFields;	// don't try to create a map
	/////////////////////////////////////////////////
	// go through topo look for -1, and list corresponding boundary sides
	// then reorder as contiguous boundary segments - need to group boundary rects by islands
	// will need a new field for list of boundary points since there can be duplicates, can't just order and list segment endpoints
	//goto setFields;
	
	/*nSegs = 2*ntri; //number of -1's in topo
	boundaryPtsH = (LONGH)_NewHandleClear(nv * sizeof(**boundaryPtsH));
	boundaryEndPtsH = (LONGH)_NewHandleClear(nv * sizeof(**boundaryEndPtsH));
	waterBoundaryPtsH = (LONGH)_NewHandleClear(nv * sizeof(**waterBoundaryPtsH));
	flagH = (LONGH)_NewHandleClear(nv * sizeof(**flagH));
	segUsed = (Boolean**)_NewHandleClear(nSegs * sizeof(Boolean));
	segList = (SegInfoHdl)_NewHandleClear(nSegs * sizeof(**segList));
	// first go through rectangles and group by island
	// do this before making dagtree, 
	DisplayMessage("NEXTMESSAGETEMP");
	DisplayMessage("Numbering Islands");
	MySpinCursor(); // JLM 8/4/99
	//err = NumberIslands(&maskH2, velocityH, landWaterInfo, fNumRows_minus1, fNumCols_minus1, &numIslands);	// numbers start at 3 (outer boundary)
	err = NumberIslands(&maskH2, landmaskH, landWaterInfo, fNumRows_minus1, fNumCols_minus1, &numIslands);	// numbers start at 3 (outer boundary)
	MySpinCursor(); // JLM 8/4/99
	if (err) goto done;
	for (i=0;i<ntri;i++)
	{
		if ((i+1)%2==0) isOdd = 0; else isOdd = 1;
		// the middle neighbor triangle is always the other half of the rectangle so can't be land or outside the map
		// odd - left/top, even - bottom/right the 1-2 segment is top/bot, the 2-3 segment is right/left
		if ((*topo)[i].adjTri1 == -1)
		{
			// add segment pt 2 - pt 3 to list, need points, triNum and whether it's L/W boundary (boundary num)
			(*segList)[segNum].pt1 = (*topo)[i].vertex2;
			(*segList)[segNum].pt2 = (*topo)[i].vertex3;
			// check which land block this segment borders and mark the island
			if (isOdd) 
			{
				// check left rectangle for L/W border 
				rectIndex = INDEXH(verdatPtsH,(*topo)[i].vertex3);	// to get back into original grid for L/W info - use maskH2
				iIndex = rectIndex/fNumCols;
				jIndex = rectIndex%fNumCols;
				if (jIndex>0 && INDEXH(maskH2,iIndex*fNumCols + jIndex-1)>=3)
				{
					(*segList)[segNum].isWater = 0;
					(*segList)[segNum].islandNumber = INDEXH(maskH2,iIndex*fNumCols + jIndex-1);	
				}
				else
				{
					(*segList)[segNum].isWater = 1;
					(*segList)[segNum].islandNumber = 1;	
				}
			}
			else 
			{	
				// check right rectangle for L/W border convert back to row/col
				rectIndex = INDEXH(verdatPtsH,(*topo)[i].vertex1);
				iIndex = rectIndex/fNumCols;
				jIndex = rectIndex%fNumCols;
				//if (jIndex<fNumCols && INDEXH(maskH2,iIndex*fNumCols + jIndex+1)>=3)
				if (jIndex<fNumCols_minus1 && INDEXH(maskH2,iIndex*fNumCols + jIndex+1)>=3)
				{
					(*segList)[segNum].isWater = 0;
					//(*segList)[segNum].islandNumber = INDEXH(maskH2,iIndex*fNumCols + jIndex+1);	
					(*segList)[segNum].islandNumber = INDEXH(maskH2,iIndex*fNumCols + jIndex+1);	
				}
				else
				{
					(*segList)[segNum].isWater = 1;
					(*segList)[segNum].islandNumber = 1;	
				}
			}
			segNum++;
		}
		
		if ((*topo)[i].adjTri3 == -1)
		{
			// add segment pt 1 - pt 2 to list
			// odd top, even bottom
			(*segList)[segNum].pt1 = (*topo)[i].vertex1;
			(*segList)[segNum].pt2 = (*topo)[i].vertex2;
			// check which land block this segment borders and mark the island
			if (isOdd) 
			{
				// check top rectangle for L/W border
				rectIndex = INDEXH(verdatPtsH,(*topo)[i].vertex3);	// to get back into original grid for L/W info - use maskH2
				iIndex = rectIndex/fNumCols;
				jIndex = rectIndex%fNumCols;
				if (iIndex>0 && INDEXH(maskH2,(iIndex-1)*fNumCols + jIndex)>=3)
				{
					(*segList)[segNum].isWater = 0;
					(*segList)[segNum].islandNumber = INDEXH(maskH2,(iIndex-1)*fNumCols + jIndex);
				}
				else
				{
					(*segList)[segNum].isWater = 1;
					(*segList)[segNum].islandNumber = 1;
				}
			}
			else 
			{
				// check bottom rectangle for L/W border
				rectIndex = INDEXH(verdatPtsH,(*topo)[i].vertex1);
				iIndex = rectIndex/fNumCols;
				jIndex = rectIndex%fNumCols;
				//if (iIndex<fNumRows && INDEXH(maskH2,(iIndex+1)*fNumCols + jIndex)>=3)
				if (iIndex<fNumRows_minus1 && INDEXH(maskH2,(iIndex+1)*fNumCols + jIndex)>=3)
				{
					(*segList)[segNum].isWater = 0;
					//(*segList)[segNum].islandNumber = INDEXH(maskH2,(iIndex+1)*fNumCols + jIndex);		// this should be the neighbor's value
					(*segList)[segNum].islandNumber = INDEXH(maskH2,(iIndex+1)*fNumCols + jIndex);		// this should be the neighbor's value
				}
				else
				{
					(*segList)[segNum].isWater = 1;
					(*segList)[segNum].islandNumber = 1;		
				}
			}
			segNum++;
		}
	}
	nSegs = segNum;
	_SetHandleSize((Handle)segList,nSegs*sizeof(**segList));
	_SetHandleSize((Handle)segUsed,nSegs*sizeof(**segUsed));
	// go through list of segments, and make list of boundary segments
	// as segment is taken mark so only use each once
	// get a starting point, add the first and second to the list
	islandNum = 3;
findnewstartpoint:
	if (islandNum > numIslands) 
	{
		_SetHandleSize((Handle)boundaryPtsH,nBoundaryPts*sizeof(**boundaryPtsH));
		_SetHandleSize((Handle)waterBoundaryPtsH,nBoundaryPts*sizeof(**waterBoundaryPtsH));
		_SetHandleSize((Handle)boundaryEndPtsH,nEndPts*sizeof(**boundaryEndPtsH));
		goto setFields;	// off by 2 - 0,1,2 are water cells, 3 and up are land
	}
	foundPt = false;
	for (i=0;i<nSegs;i++)
	{
		if ((*segUsed)[i]) continue;
		waterStartPoint = nBoundaryPts;
		(*boundaryPtsH)[nBoundaryPts++] = (*segList)[i].pt1;
		(*flagH)[(*segList)[i].pt1] = 1;
		(*waterBoundaryPtsH)[nBoundaryPts] = (*segList)[i].isWater+1;
		(*boundaryPtsH)[nBoundaryPts++] = (*segList)[i].pt2;
		(*flagH)[(*segList)[i].pt2] = 1;
		currentIndex = (*segList)[i].pt2;
		startIndex = (*segList)[i].pt1;
		currentIsland = (*segList)[i].islandNumber;	
		foundPt = true;
		(*segUsed)[i] = true;
		break;
	}
	if (!foundPt)
	{
		printNote("Lost trying to set boundaries");
		// clean up handles and set grid without a map
		if (boundaryPtsH) {DisposeHandle((Handle)boundaryPtsH); boundaryPtsH = 0;}
		if (boundaryEndPtsH) {DisposeHandle((Handle)boundaryEndPtsH); boundaryEndPtsH = 0;}
		if (waterBoundaryPtsH) {DisposeHandle((Handle)waterBoundaryPtsH); waterBoundaryPtsH = 0;}
		goto setFields;
	}
	
findnextpoint:
	for (i=0;i<nSegs;i++)
	{
		// look for second point of the previous selected segment, add the second to point list
		if ((*segUsed)[i]) continue;
		if ((*segList)[i].islandNumber > 3 && (*segList)[i].islandNumber != currentIsland) continue;
		if ((*segList)[i].islandNumber > 3 && currentIsland <= 3) continue;
		index = (*segList)[i].pt1;
		if (index == currentIndex)	// found next point
		{
			currentIndex = (*segList)[i].pt2;
			(*segUsed)[i] = true;
			if (currentIndex == startIndex) // completed a segment
			{
				islandNum++;
				(*boundaryEndPtsH)[nEndPts++] = nBoundaryPts-1;
				(*waterBoundaryPtsH)[waterStartPoint] = (*segList)[i].isWater+1;	// need to deal with this
				goto findnewstartpoint;
			}
			else
			{
				(*boundaryPtsH)[nBoundaryPts] = (*segList)[i].pt2;
				(*flagH)[(*segList)[i].pt2] = 1;
				(*waterBoundaryPtsH)[nBoundaryPts] = (*segList)[i].isWater+1;
				nBoundaryPts++;
				goto findnextpoint;
			}
		}
	}
	// shouldn't get here unless there's a problem...
	_SetHandleSize((Handle)boundaryPtsH,nBoundaryPts*sizeof(**boundaryPtsH));
	_SetHandleSize((Handle)waterBoundaryPtsH,nBoundaryPts*sizeof(**waterBoundaryPtsH));
	_SetHandleSize((Handle)boundaryEndPtsH,nEndPts*sizeof(**boundaryEndPtsH));
*/	
setFields:	
	
	fVerdatToNetCDFH = verdatPtsH;
		
	/////////////////////////////////////////////////
	
	triGrid = new TTriGridVel;
	if (!triGrid)
	{		
		err = true;
		TechError("Error in TimeGridVelCurv_c::ReorderPointsCOOPSMask()","new TTriGridVel",err);
		goto done;
	}
	
	fGrid = (TTriGridVel*)triGrid;
	
	triGrid -> SetBounds(triBounds); 
	this->SetGridBounds(triBounds);
	dagTree = new TDagTree(pts,topo,tree.treeHdl,velH,tree.numBranches); 
	if(!dagTree)
	{
		err = -1;
		printError("Unable to create dag tree.");
		goto done;
	}
	
	triGrid -> SetDagTree(dagTree);
	
	// code goes here, do we want to store grid boundary and land/water information?
	/*if (waterBoundaryPtsH)
	{
		PtCurMap *map = CreateAndInitPtCurMap(fVar.pathName,triBounds); // the map bounds are the same as the grid bounds
		if (!map) {err=-1; goto done;}
		// maybe move up and have the map read in the boundary information
		map->SetBoundarySegs(boundaryEndPtsH);	
		map->SetWaterBoundaries(waterBoundaryPtsH);
		map->SetBoundaryPoints(boundaryPtsH);
		
		*newMap = map;
	}
	else*/
	{
		//if (waterBoundaryPtsH) {DisposeHandle((Handle)waterBoundaryPtsH); waterBoundaryPtsH=0;}
		//if (boundaryEndPtsH) {DisposeHandle((Handle)boundaryEndPtsH); boundaryEndPtsH=0;}
		//if (boundaryPtsH) {DisposeHandle((Handle)boundaryPtsH); boundaryPtsH=0;}
	}
	
	pts = 0;	// because fGrid is now responsible for it
	topo = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	tree.treeHdl = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	
	/////////////////////////////////////////////////
done:
	if (landWaterInfo) {DisposeHandle((Handle)landWaterInfo); landWaterInfo=0;}
	if (ptIndexHdl) {DisposeHandle((Handle)ptIndexHdl); ptIndexHdl = 0;}
	if (gridCellInfo) {DisposeHandle((Handle)gridCellInfo); gridCellInfo = 0;}
	//if (segUsed) {DisposeHandle((Handle)segUsed); segUsed = 0;}
	//if (segList) {DisposeHandle((Handle)segList); segList = 0;}
	//if (flagH) {DisposeHandle((Handle)flagH); flagH = 0;}
	
	if(err)
	{
		if(!errmsg[0])
			strcpy(errmsg,"An error occurred in TimeGridVelCurv_c::ReorderPointsCOOPSMask");
		printError(errmsg); 
		if(pts) {DisposeHandle((Handle)pts); pts=0;}
		if(topo) {DisposeHandle((Handle)topo); topo=0;}
		if(velH) {DisposeHandle((Handle)velH); velH=0;}
		if(tree.treeHdl) {DisposeHandle((Handle)tree.treeHdl); tree.treeHdl=0;}
		
		if(fGrid)
		{
			fGrid ->Dispose();
			delete fGrid;
			fGrid = 0;
		}
		if (landWaterInfo) {DisposeHandle((Handle)landWaterInfo); landWaterInfo=0;}
		if (ptIndexHdl) {DisposeHandle((Handle)ptIndexHdl); ptIndexHdl = 0;}
		if (gridCellInfo) {DisposeHandle((Handle)gridCellInfo); gridCellInfo = 0;}
		if (verdatPtsH) {DisposeHandle((Handle)verdatPtsH); verdatPtsH = 0;}
		//if (maskH2) {DisposeHandle((Handle)maskH2); maskH2 = 0;}
		
		//if (boundaryPtsH) {DisposeHandle((Handle)boundaryPtsH); boundaryPtsH = 0;}
		//if (boundaryEndPtsH) {DisposeHandle((Handle)boundaryEndPtsH); boundaryEndPtsH = 0;}
		//if (waterBoundaryPtsH) {DisposeHandle((Handle)waterBoundaryPtsH); waterBoundaryPtsH = 0;}
	}
	
	
	
	
	return err;	
}

OSErr TimeGridVelCurv_c::GetLatLonFromIndex(long iIndex, long jIndex, WorldPoint *wp)
{
	float dLat, dLon, dLat1, dLon1, dLat2, dLon2, fLat, fLong;
	
	if (iIndex<0 || jIndex>fNumCols) return -1;
	if (iIndex==0)	// along the outer top or right edge need to add on dlat/dlon
	{					// velocities at a gridpoint correspond to lower left hand corner of a grid box, draw in grid center
		if (jIndex<fNumCols)
		{
			dLat = INDEXH(fVertexPtsH,fNumCols+jIndex).pLat - INDEXH(fVertexPtsH,jIndex).pLat;
			fLat = INDEXH(fVertexPtsH,jIndex).pLat - dLat;
			dLon = INDEXH(fVertexPtsH,fNumCols+jIndex).pLong - INDEXH(fVertexPtsH,jIndex).pLong;
			fLong = INDEXH(fVertexPtsH,jIndex).pLong - dLon;
		}
		else
		{
			dLat1 = (INDEXH(fVertexPtsH,jIndex-1).pLat - INDEXH(fVertexPtsH,jIndex-2).pLat);
			dLat2 = INDEXH(fVertexPtsH,fNumCols+jIndex-1).pLat - INDEXH(fVertexPtsH,fNumCols+jIndex-2).pLat;
			fLat = 2*(INDEXH(fVertexPtsH,jIndex-1).pLat + dLat1)-(INDEXH(fVertexPtsH,fNumCols+jIndex-1).pLat+dLat2);
			dLon1 = INDEXH(fVertexPtsH,fNumCols+jIndex-1).pLong - INDEXH(fVertexPtsH,jIndex-1).pLong;
			dLon2 = (INDEXH(fVertexPtsH,fNumCols+jIndex-2).pLong - INDEXH(fVertexPtsH,jIndex-2).pLong);
			fLong = 2*(INDEXH(fVertexPtsH,jIndex-1).pLong-dLon1)-(INDEXH(fVertexPtsH,jIndex-2).pLong-dLon2);
		}
	}
	else 
	{
		if (jIndex<fNumCols)
		{
			fLat = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex).pLat;
			fLong = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex).pLong;
		}
		else
		{
			dLat = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-1).pLat - INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-2).pLat;
			fLat = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-1).pLat + dLat;
			dLon = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-1).pLong - INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-2).pLong;
			fLong = INDEXH(fVertexPtsH,(iIndex-1)*fNumCols+jIndex-1).pLong + dLon;
		}
	}
	(*wp).pLat = (long)(fLat*1e6);
	(*wp).pLong = (long)(fLong*1e6);
	
	return noErr;
}

long TimeGridVelCurv_c::GetNumDepthLevels()
{
	// should have only one version of this for all grid types, but will have to redo the regular grid stuff with depth levels
	// and check both sigma grid and multilayer grid (and maybe others)
	long numDepthLevels = 0;
	OSErr err = 0;
	char path[256], outPath[256];
	int status, ncid, sigmaid, sigmavarid;
	size_t sigmaLength=0;
	//if (fDepthLevelsHdl) numDepthLevels = _GetHandleSize((Handle)fDepthLevelsHdl)/sizeof(**fDepthLevelsHdl);
	//status = nc_open(fVar.pathName, NC_NOWRITE, &ncid);
	strcpy(path,fVar.pathName);
	if (!path || !path[0]) return -1;
	status = nc_open(path, NC_NOWRITE, &ncid);
	if (status != NC_NOERR)/* {err = -1; goto done;}*/
	{
#if TARGET_API_MAC_CARBON
		err = ConvertTraditionalPathToUnixPath((const char *) path, outPath, kMaxNameLen) ;
		status = nc_open(outPath, NC_NOWRITE, &ncid);
#endif
		if (status != NC_NOERR) {err = -1; return -1;}
	}
	
	//if (status != NC_NOERR) {/*err = -1; goto done;*/return -1;}
	status = nc_inq_dimid(ncid, "sigma", &sigmaid); 	
	if (status != NC_NOERR) 
	{
		numDepthLevels = 1;	// check for zgrid option here
	}	
	else
	{
		status = nc_inq_varid(ncid, "sigma", &sigmavarid); //Navy
		if (status != NC_NOERR) 
		{
			status = nc_inq_varid(ncid, "sc_r", &sigmavarid);
			status = nc_inq_dimlen(ncid, sigmaid, &sigmaLength);
			if (status != NC_NOERR) {numDepthLevels = 1;}	// require variable to match the dimension
			else numDepthLevels = sigmaLength;
		}
		else
		{
			status = nc_inq_dimlen(ncid, sigmaid, &sigmaLength);
			if (status != NC_NOERR) {numDepthLevels = 1;}	// error in file
			//fVar.gridType = SIGMA;	// in theory we should track this on initial read...
			//fVar.maxNumDepths = sigmaLength;
			else numDepthLevels = sigmaLength;
			//status = nc_get_vara_float(ncid, sigmavarid, &ptIndex, &sigma_count, sigma_vals);
			//if (status != NC_NOERR) {err = -1; goto done;}
			// once depth is read in 
		}
	}
	
	//done:
	return numDepthLevels;     
}

OSErr TimeGridVelCurv_c::ReadTopology(char* path)
{
	// import NetCDF curvilinear info so don't have to regenerate
	char s[1024], errmsg[256]/*, s[256], topPath[256]*/;
	long i, numPoints, numTopoPoints, line = 0, numPts;
	CHARH f = 0;
	OSErr err = 0;
	
	TopologyHdl topo=0;
	LongPointHdl pts=0;
	FLOATH depths=0;
	VelocityFH velH = 0;
	DAGTreeStruct tree;
	WorldRect bounds = voidWorldRect;
	
	TTriGridVel *triGrid = nil;
	tree.treeHdl = 0;
	TDagTree *dagTree = 0;
	
	long numWaterBoundaries, numBoundaryPts, numBoundarySegs;
	LONGH boundarySegs=0, waterBoundaries=0, boundaryPts=0;
	
	errmsg[0]=0;
	
	if (!path || !path[0]) return 0;
	
	if (err = ReadFileContents(TERMINATED,0, 0, path, 0, 0, &f)) {
		TechError("TimeGridVelCurv::ReadTopology()", "ReadFileContents()", err);
		goto done;
	}
	
	_HLock((Handle)f); // JLM 8/4/99
	
	// No header
	// start with transformation array and vertices
	MySpinCursor(); // JLM 8/4/99
	NthLineInTextOptimized(*f, (line)++, s, 1024); 
	if(IsTransposeArrayHeaderLine(s,&numPts)) // 
	{
		if (err = ReadTransposeArray(f,&line,&fVerdatToNetCDFH,numPts,errmsg)) 
		{strcpy(errmsg,"Error in ReadTransposeArray"); goto done;}
	}
	else {err=-1; strcpy(errmsg,"Error in Transpose header line"); goto done;}
	
	if(err = ReadTVertices(f,&line,&pts,&depths,errmsg)) goto done;
	
	if(pts) 
	{
		LongPoint	thisLPoint;
		
		numPts = _GetHandleSize((Handle)pts)/sizeof(LongPoint);
		if(numPts > 0)
		{
			WorldPoint  wp;
			for(i=0;i<numPts;i++)
			{
				thisLPoint = INDEXH(pts,i);
				wp.pLat = thisLPoint.v;
				wp.pLong = thisLPoint.h;
				AddWPointToWRect(wp.pLat, wp.pLong, &bounds);
			}
		}
	}
	MySpinCursor();
	
	NthLineInTextOptimized(*f, (line)++, s, 1024); 
	if(IsBoundarySegmentHeaderLine(s,&numBoundarySegs)) // Boundary data from CATs
	{
		MySpinCursor();
		if (numBoundarySegs>0)
			err = ReadBoundarySegs(f,&line,&boundarySegs,numBoundarySegs,errmsg);
		if(err) goto done;
		NthLineInTextOptimized(*f, (line)++, s, 1024); 
	}
	else
	{
		//err = -1;
		//strcpy(errmsg,"Error in Boundary segment header line");
		//goto done;
		// not needed for 2D files, but we require for now
	}
	MySpinCursor(); // JLM 8/4/99
	
	if(IsWaterBoundaryHeaderLine(s,&numWaterBoundaries,&numBoundaryPts)) // Boundary types from CATs
	{
		MySpinCursor();
		if (numBoundaryPts>0)
			err = ReadWaterBoundaries(f,&line,&waterBoundaries,numWaterBoundaries,numBoundaryPts,errmsg);
		if(err) goto done;
		NthLineInTextOptimized(*f, (line)++, s, 1024); 
	}
	else
	{
		//err = -1;
		//strcpy(errmsg,"Error in Water boundaries header line");
		//goto done;
		// not needed for 2D files, but we require for now
	}
	MySpinCursor(); // JLM 8/4/99
	//NthLineInTextOptimized(*f, (line)++, s, 1024); 
	
	if(IsBoundaryPointsHeaderLine(s,&numBoundaryPts)) // Boundary data from CATs
	{
		MySpinCursor();
		if (numBoundaryPts>0)
			err = ReadBoundaryPts(f,&line,&boundaryPts,numBoundaryPts,errmsg);
		if(err) goto done;
		NthLineInTextOptimized(*f, (line)++, s, 1024); 
	}
	else
	{
		//err = -1;
		//strcpy(errmsg,"Error in Boundary segment header line");
		//goto done;
		// not always needed ? probably always needed for curvilinear
	}
	MySpinCursor(); // JLM 8/4/99
	
	if(IsTTopologyHeaderLine(s,&numTopoPoints)) // Topology from CATs
	{
		MySpinCursor();
		err = ReadTTopologyBody(f,&line,&topo,&velH,errmsg,numTopoPoints,FALSE);
		if(err) goto done;
		NthLineInTextOptimized(*f, (line)++, s, 1024); 
	}
	else
	{
		err = -1; // for now we require TTopology
		strcpy(errmsg,"Error in topology header line");
		if(err) goto done;
	}
	MySpinCursor(); // JLM 8/4/99
	
	
	//NthLineInTextOptimized(*f, (line)++, s, 1024); 
	
	if(IsTIndexedDagTreeHeaderLine(s,&numPoints))  // DagTree from CATs
	{
		MySpinCursor();
		err = ReadTIndexedDagTreeBody(f,&line,&tree,errmsg,numPoints);
		if(err) goto done;
	}
	else
	{
		err = -1; // for now we require TIndexedDagTree
		strcpy(errmsg,"Error in dag tree header line");
		if(err) goto done;
	}
	MySpinCursor(); // JLM 8/4/99
	
	/////////////////////////////////////////////////
	// code goes here, do we want to store the grid boundary and land/water information?
	/*if (waterBoundaries && waterBoundaries && boundaryPts)
	 {
	 //PtCurMap *map = CreateAndInitPtCurMap(fVar.userName,bounds); // the map bounds are the same as the grid bounds
	 PtCurMap *map = CreateAndInitPtCurMap("Extended Topology",bounds); // the map bounds are the same as the grid bounds
	 if (!map) {strcpy(errmsg,"Error creating ptcur map"); goto done;}
	 // maybe move up and have the map read in the boundary information
	 map->SetBoundarySegs(boundarySegs);	
	 map->SetBoundaryPoints(boundaryPts);	
	 map->SetWaterBoundaries(waterBoundaries);
	 
	 *newMap = map;
	 }	
	 else*/	
	{
		if (waterBoundaries) {DisposeHandle((Handle)waterBoundaries); waterBoundaries=0;}
		if (boundarySegs) {DisposeHandle((Handle)boundarySegs); boundarySegs=0;}
		if (boundaryPts) {DisposeHandle((Handle)boundaryPts); boundaryPts=0;}
	}
	
	/////////////////////////////////////////////////
	
	
	triGrid = new TTriGridVel;
	if (!triGrid)
	{		
		err = true;
		TechError("Error in TimeGridVelCurv::ReadTopology()","new TTriGridVel" ,err);
		goto done;
	}
	
	fGrid = (TTriGridVel*)triGrid;
	
	triGrid -> SetBounds(bounds); 
	this->SetGridBounds(bounds);
	//triGrid -> SetDepths(depths);
	
	dagTree = new TDagTree(pts,topo,tree.treeHdl,velH,tree.numBranches); 
	if(!dagTree)
	{
		err = -1;
		printError("Unable to read Extended Topology file.");
		goto done;
	}
	
	triGrid -> SetDagTree(dagTree);
	
	pts = 0;	// because fGrid is now responsible for it
	topo = 0; // because fGrid is now responsible for it
	tree.treeHdl = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	//depths = 0;
	
done:
	
	if(depths) {DisposeHandle((Handle)depths); depths=0;}
	if(f) 
	{
		_HUnlock((Handle)f); 
		DisposeHandle((Handle)f); 
		f = 0;
	}
	
	if(err)
	{
		if(!errmsg[0])
			strcpy(errmsg,"An error occurred in TimeGridVelCurv::ReadTopology");
		printError(errmsg); 
		if(pts) {DisposeHandle((Handle)pts); pts=0;}
		if(topo) {DisposeHandle((Handle)topo); topo=0;}
		if(velH) {DisposeHandle((Handle)velH); velH=0;}
		if(depths) {DisposeHandle((Handle)depths); depths=0;}
		if(tree.treeHdl) {DisposeHandle((Handle)tree.treeHdl); tree.treeHdl=0;}
		
		if(fGrid)
		{
			fGrid ->Dispose();
			delete fGrid;
			fGrid = 0;
		}
		/*if (*newMap) 
		 {
		 (*newMap)->Dispose();
		 delete *newMap;
		 *newMap=0;
		 }*/
		if (waterBoundaries) {DisposeHandle((Handle)waterBoundaries); waterBoundaries=0;}
		if (boundarySegs) {DisposeHandle((Handle)boundarySegs); boundarySegs = 0;}
		if (boundaryPts) {DisposeHandle((Handle)boundaryPts); boundaryPts = 0;}
	}
	return err;
}


TimeGridVelTri_c::TimeGridVelTri_c () : TimeGridVelCurv_c(), TimeGridVel_c()
{
	fNumNodes = 0;
	fNumEles = 0;
	bVelocitiesOnTriangles = false;
}

LongPointHdl TimeGridVelTri_c::GetPointsHdl()
{
	return (dynamic_cast<TTriGridVel*>(fGrid)) -> GetPointsHdl();
}

float TimeGridVelTri_c::GetTotalDepth(WorldPoint refPoint, long triNum)
{
#pragma unused(refPoint)
	float totalDepth = 0;
	if (fDepthDataInfo) 
	{
		//indexToDepthData = (*fDepthDataInfo)[ptIndex].indexToDepthData;
		//numDepths = (*fDepthDataInfo)[ptIndex].numDepths;
		totalDepth = (*fDepthDataInfo)[triNum].totalDepth;
	}
	return totalDepth; // this should be an error
}
// probably eventually switch to base class

void TimeGridVelTri_c::GetDepthIndices(long ptIndex, float depthAtPoint, long *depthIndex1, long *depthIndex2)
{
	long indexToDepthData;
	long numDepths;
	float totalDepth;
	
	if (fDepthDataInfo) 
	{
		indexToDepthData = (*fDepthDataInfo)[ptIndex].indexToDepthData;
		numDepths = (*fDepthDataInfo)[ptIndex].numDepths;
		totalDepth = (*fDepthDataInfo)[ptIndex].totalDepth;
	}
	else
		return; // this should be an error
	
	switch(fVar.gridType) 
	{
		case TWO_D:	// no depth data
			*depthIndex1 = indexToDepthData;
			*depthIndex2 = UNASSIGNEDINDEX;
			break;
		case BAROTROPIC:	// values same throughout column, but limit on total depth
			if (depthAtPoint <= totalDepth)
			{
				*depthIndex1 = indexToDepthData;
				*depthIndex2 = UNASSIGNEDINDEX;
			}
			else
			{
				*depthIndex1 = UNASSIGNEDINDEX;
				*depthIndex2 = UNASSIGNEDINDEX;
			}
			break;
		case MULTILAYER: //
			if (depthAtPoint <= totalDepth) // check data exists at chosen/LE depth for this point
			{	// if depths are measured from the bottom this is confusing
				long j;
				for(j=0;j<numDepths-1;j++)
				{
					if(INDEXH(fDepthsH,indexToDepthData+j)<depthAtPoint &&
					   depthAtPoint<=INDEXH(fDepthsH,indexToDepthData+j+1))
					{
						*depthIndex1 = indexToDepthData+j;
						*depthIndex2 = indexToDepthData+j+1;
					}
					else if(INDEXH(fDepthsH,indexToDepthData+j)==depthAtPoint)
					{
						*depthIndex1 = indexToDepthData+j;
						*depthIndex2 = UNASSIGNEDINDEX;
					}
				}
				if(INDEXH(fDepthsH,indexToDepthData)==depthAtPoint)	// handles single depth case
				{
					*depthIndex1 = indexToDepthData;
					*depthIndex2 = UNASSIGNEDINDEX;
				}
				else if(INDEXH(fDepthsH,indexToDepthData+numDepths-1)<depthAtPoint)
				{
					*depthIndex1 = indexToDepthData+numDepths-1;
					*depthIndex2 = UNASSIGNEDINDEX; //BOTTOM, for now just extrapolate lowest depth value (at bottom case?)
				}
				else if(INDEXH(fDepthsH,indexToDepthData)>depthAtPoint)
				{
					*depthIndex1 = indexToDepthData;
					*depthIndex2 = UNASSIGNEDINDEX; //TOP, for now just extrapolate highest depth value
				}
			}
			else // no data at this point
			{
				*depthIndex1 = UNASSIGNEDINDEX;
				*depthIndex2 = UNASSIGNEDINDEX;
			}
			break;
		case SIGMA: // should rework the sigma to match Gnome_beta's simpler method
			if (depthAtPoint <= totalDepth) // check data exists at chosen/LE depth for this point
			{
				long j;
				for(j=0;j<numDepths-1;j++)
				{
					if(INDEXH(fDepthsH,indexToDepthData+j)<depthAtPoint &&
					   depthAtPoint<=INDEXH(fDepthsH,indexToDepthData+j+1))
					{
						*depthIndex1 = indexToDepthData+j;
						*depthIndex2 = indexToDepthData+j+1;
					}
					else if(INDEXH(fDepthsH,indexToDepthData+j)==depthAtPoint)
					{
						*depthIndex1 = indexToDepthData+j;
						*depthIndex2 = UNASSIGNEDINDEX;
					}
				}
				if(INDEXH(fDepthsH,indexToDepthData)==depthAtPoint)	// handles single depth case
				{
					*depthIndex1 = indexToDepthData;
					*depthIndex2 = UNASSIGNEDINDEX;
				}
				else if(INDEXH(fDepthsH,indexToDepthData+numDepths-1)<depthAtPoint)
				{
					*depthIndex1 = indexToDepthData+numDepths-1;
					*depthIndex2 = UNASSIGNEDINDEX; //BOTTOM, for now just extrapolate lowest depth value (at bottom case?)
				}
				else if(INDEXH(fDepthsH,indexToDepthData)>depthAtPoint)
				{
					*depthIndex1 = indexToDepthData;
					*depthIndex2 = UNASSIGNEDINDEX; //TOP, for now just extrapolate highest depth value
				}
			}
			else // no data at this point
			{
				*depthIndex1 = UNASSIGNEDINDEX;
				*depthIndex2 = UNASSIGNEDINDEX;
			}
			break;
		default:
			*depthIndex1 = UNASSIGNEDINDEX;
			*depthIndex2 = UNASSIGNEDINDEX;
			break;
	}
}

VelocityRec TimeGridVelTri_c::GetScaledPatValue(const Seconds& model_time, WorldPoint3D refPoint)
{
	double timeAlpha, depth = refPoint.z;
	long ptIndex1,ptIndex2,ptIndex3,triIndex; 
	long index = -1; 
	Seconds startTime,endTime;
	InterpolationVal interpolationVal;
	VelocityRec scaledPatVelocity = {0.,0.};
	OSErr err = 0;
	
	// Get the interpolation coefficients, alpha1,ptIndex1,alpha2,ptIndex2,alpha3,ptIndex3
	if (!bVelocitiesOnTriangles)
		interpolationVal = fGrid -> GetInterpolationValues(refPoint.p);
	else
	{
		LongPoint lp;
		TDagTree *dagTree = 0;
		dagTree = (dynamic_cast<TTriGridVel*>(fGrid)) -> GetDagTree();
		if(!dagTree) return scaledPatVelocity;
		lp.h = refPoint.p.pLong;
		lp.v = refPoint.p.pLat;
		triIndex = dagTree -> WhatTriAmIIn(lp);
		interpolationVal.ptIndex1 = -1;
	}
	
	if (interpolationVal.ptIndex1 >= 0)  // if negative corresponds to negative ntri
	{
		// this is only section that's different from ptcur
		ptIndex1 =  interpolationVal.ptIndex1;	
		ptIndex2 =  interpolationVal.ptIndex2;
		ptIndex3 =  interpolationVal.ptIndex3;
		if (fVerdatToNetCDFH)
		{
			ptIndex1 =  (*fVerdatToNetCDFH)[interpolationVal.ptIndex1];	
			ptIndex2 =  (*fVerdatToNetCDFH)[interpolationVal.ptIndex2];
			ptIndex3 =  (*fVerdatToNetCDFH)[interpolationVal.ptIndex3];
		}
	}
	else
	{
		if (!bVelocitiesOnTriangles)
			return scaledPatVelocity;	// set to zero, avoid any accidental access violation
	}
	
	// code goes here, need interpolation in z if LE is below surface
	// what kind of weird things can triangles do below the surface ??
	if (/*depth>0 &&*/ interpolationVal.ptIndex1 >= 0) 
	{
		scaledPatVelocity = GetScaledPatValue3D(model_time, interpolationVal,depth);
		goto scale;
	}						
	if (depth > 0) return scaledPatVelocity;	// set subsurface spill with no subsurface velocity

	// Check for constant current 
	if((GetNumTimesInFile()==1 && !(GetNumFiles()>1)) || (fEndData.timeIndex == UNASSIGNEDINDEX && model_time > ((*fTimeHdl)[fStartData.timeIndex] + fTimeShift) && fAllowExtrapolationInTime) || (fEndData.timeIndex == UNASSIGNEDINDEX && model_time < ((*fTimeHdl)[fStartData.timeIndex] + fTimeShift) && fAllowExtrapolationInTime))
	{
		// Calculate the interpolated velocity at the point
		if (interpolationVal.ptIndex1 >= 0) 
		{
			scaledPatVelocity.u = interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,ptIndex1).u)
			+interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,ptIndex2).u)
			+interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,ptIndex3).u );
			scaledPatVelocity.v = interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,ptIndex1).v)
			+interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,ptIndex2).v)
			+interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,ptIndex3).v);
		}
		else	// if negative corresponds to negative ntri, set vel to zero
		{
			if (bVelocitiesOnTriangles && triIndex > 0)
			{
				scaledPatVelocity.u = INDEXH(fStartData.dataHdl,triIndex).u;
				scaledPatVelocity.v = INDEXH(fStartData.dataHdl,triIndex).v;
			}
			else
			{
				scaledPatVelocity.u = 0.;
				scaledPatVelocity.v = 0.;
			}
		}
	}
	else // time varying current 
	{
		// Calculate the time weight factor
		if (GetNumFiles()>1 && fOverLap)
			startTime = fOverLapStartTime + fTimeShift;
		else
			startTime = (*fTimeHdl)[fStartData.timeIndex] + fTimeShift;
		endTime = (*fTimeHdl)[fEndData.timeIndex] + fTimeShift;
		timeAlpha = (endTime - model_time)/(double)(endTime - startTime);
		
		// Calculate the interpolated velocity at the point
		if (interpolationVal.ptIndex1 >= 0) 
		{
			scaledPatVelocity.u = interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,ptIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,ptIndex1).u)
			+interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,ptIndex2).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,ptIndex2).u)
			+interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,ptIndex3).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,ptIndex3).u);
			scaledPatVelocity.v = interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,ptIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,ptIndex1).v)
			+interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,ptIndex2).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,ptIndex2).v)
			+interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,ptIndex3).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,ptIndex3).v);
		}
		else	// if negative corresponds to negative ntri, set vel to zero
		{
			if (bVelocitiesOnTriangles && triIndex > 0)
			{
				scaledPatVelocity.u = timeAlpha*INDEXH(fStartData.dataHdl,triIndex).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,triIndex).u;
				scaledPatVelocity.v = timeAlpha*INDEXH(fStartData.dataHdl,triIndex).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,triIndex).v;
			}
			else
			{
				scaledPatVelocity.u = 0.;
				scaledPatVelocity.v = 0.;
			}
		}
	}
	
scale:
	
	//scaledPatVelocity.u *= fVar.curScale; // may want to allow some sort of scale factor, though should be in file
	//scaledPatVelocity.v *= fVar.curScale; 
	scaledPatVelocity.u *= fVar.fileScaleFactor; // may want to allow some sort of scale factor, though should be in file
	scaledPatVelocity.v *= fVar.fileScaleFactor; 
	
	return scaledPatVelocity;
}

VelocityRec TimeGridVelTri_c::GetScaledPatValue3D(const Seconds& model_time, InterpolationVal interpolationVal,float depth)
{
	// figure out which depth values the LE falls between
	// will have to interpolate in lat/long for both levels first
	// and some sort of check on the returned indices, what to do if one is below bottom?
	// for sigma model might have different depth values at each point
	// for multilayer they should be the same, so only one interpolation would be needed
	// others don't have different velocities at different depths so no interpolation is needed
	// in theory the surface case should be a subset of this case, may eventually combine
	
	long pt1depthIndex1, pt1depthIndex2, pt2depthIndex1, pt2depthIndex2, pt3depthIndex1, pt3depthIndex2;
	long ptIndex1, ptIndex2, ptIndex3, amtOfDepthData = 0;
	double topDepth, bottomDepth, depthAlpha, timeAlpha;
	VelocityRec pt1interp = {0.,0.}, pt2interp = {0.,0.}, pt3interp = {0.,0.};
	VelocityRec scaledPatVelocity = {0.,0.};
	Seconds startTime, endTime;
	
	if (interpolationVal.ptIndex1 >= 0)  // if negative corresponds to negative ntri
	{
		// this is only section that's different from ptcur
		ptIndex1 =  interpolationVal.ptIndex1;	
		ptIndex2 =  interpolationVal.ptIndex2;
		ptIndex3 =  interpolationVal.ptIndex3;
		if (fVerdatToNetCDFH)
		{
			ptIndex1 =  (*fVerdatToNetCDFH)[interpolationVal.ptIndex1];	
			ptIndex2 =  (*fVerdatToNetCDFH)[interpolationVal.ptIndex2];
			ptIndex3 =  (*fVerdatToNetCDFH)[interpolationVal.ptIndex3];
		}
	}
	else
		return scaledPatVelocity;
	
	if (fDepthDataInfo) amtOfDepthData = _GetHandleSize((Handle)fDepthDataInfo)/sizeof(**fDepthDataInfo);
 	if (amtOfDepthData>0)
 	{
		GetDepthIndices(ptIndex1,depth,&pt1depthIndex1,&pt1depthIndex2);	
		GetDepthIndices(ptIndex2,depth,&pt2depthIndex1,&pt2depthIndex2);	
		GetDepthIndices(ptIndex3,depth,&pt3depthIndex1,&pt3depthIndex2);	
	}
 	else
 	{	// old version that didn't use fDepthDataInfo, must be 2D
 		pt1depthIndex1 = ptIndex1;	pt1depthIndex2 = -1;
 		pt2depthIndex1 = ptIndex2;	pt2depthIndex2 = -1;
 		pt3depthIndex1 = ptIndex3;	pt3depthIndex2 = -1;
 	}
	
 	// the contributions from each point will default to zero if the depth indicies
	// come back negative (ie the LE depth is out of bounds at the grid point)
	if(GetNumTimesInFile()==1 && !(GetNumFiles()>1) || (fEndData.timeIndex == UNASSIGNEDINDEX && model_time > ((*fTimeHdl)[fStartData.timeIndex] + fTimeShift) && fAllowExtrapolationInTime) || (fEndData.timeIndex == UNASSIGNEDINDEX && model_time < ((*fTimeHdl)[fStartData.timeIndex] + fTimeShift) && fAllowExtrapolationInTime))
	{
		if (pt1depthIndex1!=-1)
		{
			if (pt1depthIndex2!=-1) 
			{
				topDepth = INDEXH(fDepthsH,pt1depthIndex1);	
				bottomDepth = INDEXH(fDepthsH,pt1depthIndex2);
				depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
				pt1interp.u = depthAlpha*(interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,pt1depthIndex1).u))
				+ (1-depthAlpha)*(interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,pt1depthIndex2).u));
				pt1interp.v = depthAlpha*(interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,pt1depthIndex1).v))
				+ (1-depthAlpha)*(interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,pt1depthIndex2).v));
			}
			else
			{
				pt1interp.u = interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,pt1depthIndex1).u); 
				pt1interp.v = interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,pt1depthIndex1).v); 
			}
		}
		
		if (pt2depthIndex1!=-1)
		{
			if (pt2depthIndex2!=-1) 
			{
				topDepth = INDEXH(fDepthsH,pt2depthIndex1);	
				bottomDepth = INDEXH(fDepthsH,pt2depthIndex2);
				depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
				pt2interp.u = depthAlpha*(interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,pt2depthIndex1).u))
				+ (1-depthAlpha)*(interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,pt2depthIndex2).u));
				pt2interp.v = depthAlpha*(interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,pt2depthIndex1).v))
				+ (1-depthAlpha)*(interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,pt2depthIndex2).v));
			}
			else
			{
				pt2interp.u = interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,pt2depthIndex1).u); 
				pt2interp.v = interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,pt2depthIndex1).v);
			}
		}
		
		if (pt3depthIndex1!=-1) 
		{
			if (pt3depthIndex2!=-1) 
			{
				topDepth = INDEXH(fDepthsH,pt3depthIndex1);	
				bottomDepth = INDEXH(fDepthsH,pt3depthIndex2);
				depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
				pt3interp.u = depthAlpha*(interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,pt3depthIndex1).u))
				+ (1-depthAlpha)*(interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,pt3depthIndex2).u));
				pt3interp.v = depthAlpha*(interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,pt3depthIndex1).v))
				+ (1-depthAlpha)*(interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,pt3depthIndex2).v));
			}
			else
			{
				pt3interp.u = interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,pt3depthIndex1).u); 
				pt3interp.v = interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,pt3depthIndex1).v); 
			}
		}
	}
	
	else // time varying current 
	{
		// Calculate the time weight factor
		if (GetNumFiles()>1 && fOverLap)
			startTime = fOverLapStartTime;
		else
			startTime = (*fTimeHdl)[fStartData.timeIndex];
		endTime = (*fTimeHdl)[fEndData.timeIndex];
		timeAlpha = (endTime - model_time)/(double)(endTime - startTime);
		
		if (pt1depthIndex1!=-1)
		{
			if (pt1depthIndex2!=-1) 
			{
				topDepth = INDEXH(fDepthsH,pt1depthIndex1);	
				bottomDepth = INDEXH(fDepthsH,pt1depthIndex2);
				depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
				pt1interp.u = depthAlpha*(interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,pt1depthIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt1depthIndex1).u))
				+ (1-depthAlpha)*(interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,pt1depthIndex2).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt1depthIndex2).u));
				pt1interp.v = depthAlpha*(interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,pt1depthIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt1depthIndex1).v))
				+ (1-depthAlpha)*(interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,pt1depthIndex2).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt1depthIndex2).v));
			}
			else
			{
				pt1interp.u = interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,pt1depthIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt1depthIndex1).u); 
				pt1interp.v = interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,pt1depthIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt1depthIndex1).v); 
			}
		}
		
		if (pt2depthIndex1!=-1)
		{
			if (pt2depthIndex2!=-1) 
			{
				topDepth = INDEXH(fDepthsH,pt2depthIndex1);	
				bottomDepth = INDEXH(fDepthsH,pt2depthIndex2);
				depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
				pt2interp.u = depthAlpha*(interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,pt2depthIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt2depthIndex1).u))
				+ (1-depthAlpha)*(interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,pt2depthIndex2).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt2depthIndex2).u));
				pt2interp.v = depthAlpha*(interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,pt2depthIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt2depthIndex1).v))
				+ (1-depthAlpha)*(interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,pt2depthIndex2).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt2depthIndex2).v));
			}
			else
			{
				pt2interp.u = interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,pt2depthIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt2depthIndex1).u); 
				pt2interp.v = interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,pt2depthIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt2depthIndex1).v); 
			}
		}
		
		if (pt3depthIndex1!=-1) 
		{
			if (pt3depthIndex2!=-1)
			{
				topDepth = INDEXH(fDepthsH,pt3depthIndex1);	
				bottomDepth = INDEXH(fDepthsH,pt3depthIndex2);
				depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
				pt3interp.u = depthAlpha*(interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,pt3depthIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt3depthIndex1).u))
				+ (1-depthAlpha)*(interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,pt3depthIndex2).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt3depthIndex2).u));
				pt3interp.v = depthAlpha*(interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,pt3depthIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt3depthIndex1).v))
				+ (1-depthAlpha)*(interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,pt3depthIndex2).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt3depthIndex2).v));
			}
			else
			{
				pt3interp.u = interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,pt3depthIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt3depthIndex1).u); 
				pt3interp.v = interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,pt3depthIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt3depthIndex1).v); 
			}
		}
	}
	scaledPatVelocity.u = pt1interp.u + pt2interp.u + pt3interp.u;
	scaledPatVelocity.v = pt1interp.v + pt2interp.v + pt3interp.v;
	
	return scaledPatVelocity;
}

OSErr TimeGridVelTri_c::TextRead(char *path, char *topFilePath) 
{
	// needs to be updated once triangle grid format is set
	
	OSErr err = 0;
	long i, numScanned;
	int status, ncid, nodeid, nbndid, bndid, neleid, latid, lonid, recid, timeid, sigmaid, sigmavarid, depthid, nv_varid, nbe_varid;
	int curr_ucmp_id, uv_dimid[3], uv_ndims;
	size_t nodeLength, nbndLength, neleLength, recs, t_len, sigmaLength=0;
	float timeVal;
	char recname[NC_MAX_NAME], *timeUnits=0, *topOrder=0;;	
	WORLDPOINTFH vertexPtsH=0;
	FLOATH totalDepthsH=0, sigmaLevelsH=0;
	float *lat_vals=0,*lon_vals=0,*depth_vals=0, *sigma_vals=0;
	long *bndry_indices=0, *bndry_nums=0, *bndry_type=0, *top_verts=0, *top_neighbors=0;
	static size_t latIndex=0,lonIndex=0,timeIndex,ptIndex=0,bndIndex[2]={0,0};
	static size_t pt_count, bnd_count[2], sigma_count,topIndex[2]={0,0}, top_count[2];
	Seconds startTime, startTime2;
	double timeConversion = 1., scale_factor = 1.;
	char errmsg[256] = "";
	char fileName[64],s[256],topPath[256], outPath[256];
	
	char *modelTypeStr=0;
	Boolean bTopInfoInFile = false, isCCW = true;
	
	if (!path || !path[0]) return 0;
	strcpy(fVar.pathName,path);
	
	strcpy(s,path);
	//SplitPathFile (s, fileName);
	SplitPathFileName (s, fileName);
	strcpy(fVar.userName, fileName); // maybe use a name from the file
	
	status = nc_open(path, NC_NOWRITE, &ncid);
	if (status != NC_NOERR) {err = -1; goto done;}
	/*if (status != NC_NOERR) 
	{	
#if TARGET_API_MAC_CARBON
		err = ConvertTraditionalPathToUnixPath((const char *) path, outPath, kMaxNameLen) ;
		status = nc_open(outPath, NC_NOWRITE, &ncid);
#endif
		if (status != NC_NOERR) {err = -1; goto done;}
	}*/
	
	status = nc_inq_dimid(ncid, "time", &recid); 
	if (status != NC_NOERR) 
	{
		status = nc_inq_unlimdim(ncid, &recid);	// maybe time is unlimited dimension
		if (status != NC_NOERR) {err = -1; goto done;}
	}
	
	status = nc_inq_varid(ncid, "time", &timeid); 
	if (status != NC_NOERR) {err = -1; goto done;} 
	
	status = nc_inq_attlen(ncid, timeid, "units", &t_len);
	if (status != NC_NOERR) 
	{
		err = -1; goto done;
	}
	else
	{
		DateTimeRec time;
		char unitStr[24], junk[10];
		
		timeUnits = new char[t_len+1];
		//status = nc_get_att_text(ncid, recid, "units", timeUnits);// recid is the dimension id not the variable id
		status = nc_get_att_text(ncid, timeid, "units", timeUnits);
		if (status != NC_NOERR) {err = -1; goto done;} 
		timeUnits[t_len] = '\0'; // moved this statement before StringSubstitute, JLM 5/2/10
		StringSubstitute(timeUnits, ':', ' ');
		StringSubstitute(timeUnits, '-', ' ');
		
		numScanned=sscanf(timeUnits, "%s %s %hd %hd %hd %hd %hd %hd",
						  unitStr, junk, &time.year, &time.month, &time.day,
						  &time.hour, &time.minute, &time.second) ;
		if (numScanned==5)	
		{time.hour = 0; time.minute = 0; time.second = 0; }
		else if (numScanned==7) // has two extra time entries ??	
			time.second = 0;
		else if (numScanned<8) // has two extra time entries ??	
			//if (numScanned<8) // has two extra time entries ??	
		{ err = -1; TechError("TimeGridVelTri_c::TextRead()", "sscanf() == 8", 0); goto done; }
		DateToSeconds (&time, &startTime2);	// code goes here, which start Time to use ??
		if (!strcmpnocase(unitStr,"HOURS") || !strcmpnocase(unitStr,"HOUR"))
			timeConversion = 3600.;
		else if (!strcmpnocase(unitStr,"MINUTES") || !strcmpnocase(unitStr,"MINUTE"))
			timeConversion = 60.;
		else if (!strcmpnocase(unitStr,"SECONDS") || !strcmpnocase(unitStr,"SECOND"))
			timeConversion = 1.;
		else if (!strcmpnocase(unitStr,"DAYS") || !strcmpnocase(unitStr,"DAY"))
			timeConversion = 24*3600.;
	} 
	
	status = nc_inq_dimid(ncid, "node", &nodeid); 
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_inq_dimlen(ncid, nodeid, &nodeLength);
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_inq_dimid(ncid, "nbnd", &nbndid);	
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_inq_varid(ncid, "bnd", &bndid);	
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_inq_dimlen(ncid, nbndid, &nbndLength);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	bnd_count[0] = nbndLength;
	bnd_count[1] = 1;
	bndry_indices = new long[nbndLength]; 
	bndry_nums = new long[nbndLength]; 
	bndry_type = new long[nbndLength]; 
	if (!bndry_indices || !bndry_nums || !bndry_type) {err = memFullErr; goto done;}
	bndIndex[1] = 1;	// take second point of boundary segments instead, so that water boundaries work out
	status = nc_get_vara_long(ncid, bndid, bndIndex, bnd_count, bndry_indices);
	if (status != NC_NOERR) {err = -1; goto done;}
	bndIndex[1] = 2;
	status = nc_get_vara_long(ncid, bndid, bndIndex, bnd_count, bndry_nums);
	if (status != NC_NOERR) {err = -1; goto done;}
	bndIndex[1] = 3;
	status = nc_get_vara_long(ncid, bndid, bndIndex, bnd_count, bndry_type);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	//status = nc_inq_dimid(ncid, "nele", &neleid);	
	//if (status != NC_NOERR) {err = -1; goto done;}	// not using these right now so not required
	//status = nc_inq_dimlen(ncid, neleid, &neleLength);
	//if (status != NC_NOERR) {err = -1; goto done;}	// not using these right now so not required
	
	status = nc_inq_dimid(ncid, "sigma", &sigmaid); 	
	if (status != NC_NOERR) 
	{
		status = nc_inq_dimid(ncid, "zloc", &sigmaid); 	
		if (status != NC_NOERR) 
		{
			fVar.gridType = TWO_D; /*err = -1; goto done;*/
		}
		else
		{	// might change names to depth rather than sigma here
			status = nc_inq_varid(ncid, "zloc", &sigmavarid); //Navy
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_inq_dimlen(ncid, sigmaid, &sigmaLength);
			if (status != NC_NOERR) {err = -1; goto done;}
			fVar.gridType = MULTILAYER;
			fVar.maxNumDepths = sigmaLength;
			sigma_vals = new float[sigmaLength];
			if (!sigma_vals) {err = memFullErr; goto done;}
			sigma_count = sigmaLength;
			status = nc_get_vara_float(ncid, sigmavarid, &ptIndex, &sigma_count, sigma_vals);
			if (status != NC_NOERR) {err = -1; goto done;}
			fDepthLevelsHdl = (FLOATH)_NewHandleClear(sigmaLength * sizeof(float));
			if (!fDepthLevelsHdl) {err = memFullErr; goto done;}
			for (i=0;i<sigmaLength;i++)
			{
				INDEXH(fDepthLevelsHdl,i) = (float)sigma_vals[i];
			}
			fNumDepthLevels = sigmaLength;	//  here also do we want all depths?
			// once depth is read in 
		}
	}	// check for zgrid option here
	else
	{
		status = nc_inq_varid(ncid, "sigma", &sigmavarid); //Navy
		if (status != NC_NOERR) {err = -1; goto done;}
		status = nc_inq_dimlen(ncid, sigmaid, &sigmaLength);
		if (status != NC_NOERR) {err = -1; goto done;}
		fVar.gridType = SIGMA;
		fVar.maxNumDepths = sigmaLength;
		sigma_vals = new float[sigmaLength];
		if (!sigma_vals) {err = memFullErr; goto done;}
		sigma_count = sigmaLength;
		status = nc_get_vara_float(ncid, sigmavarid, &ptIndex, &sigma_count, sigma_vals);
		if (status != NC_NOERR) {err = -1; goto done;}
		// once depth is read in 
	}
	
	// option to use index values?
	status = nc_inq_varid(ncid, "lat", &latid);
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_inq_varid(ncid, "lon", &lonid);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	pt_count = nodeLength;
	vertexPtsH = (WorldPointF**)_NewHandleClear(nodeLength*sizeof(WorldPointF));
	if (!vertexPtsH) {err = memFullErr; goto done;}
	lat_vals = new float[nodeLength]; 
	lon_vals = new float[nodeLength]; 
	if (!lat_vals || !lon_vals) {err = memFullErr; goto done;}
	status = nc_get_vara_float(ncid, latid, &ptIndex, &pt_count, lat_vals);
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_get_vara_float(ncid, lonid, &ptIndex, &pt_count, lon_vals);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	status = nc_inq_varid(ncid, "depth", &depthid);	// this is required for sigma or multilevel grids
	if (status != NC_NOERR) {fVar.gridType = TWO_D;/*err = -1; goto done;*/}
	else
	{	
		totalDepthsH = (FLOATH)_NewHandleClear(nodeLength*sizeof(float));
		if (!totalDepthsH) {err = memFullErr; goto done;}
		depth_vals = new float[nodeLength];
		if (!depth_vals) {err = memFullErr; goto done;}
		status = nc_get_vara_float(ncid, depthid, &ptIndex, &pt_count, depth_vals);
		if (status != NC_NOERR) {err = -1; goto done;}
		
		status = nc_get_att_double(ncid, depthid, "scale_factor", &scale_factor);
		if (status != NC_NOERR) {/*err = -1; goto done;*/}	// don't require scale factor
		
	}
	
	for (i=0;i<nodeLength;i++)
	{
		INDEXH(vertexPtsH,i).pLat = lat_vals[i];	
		INDEXH(vertexPtsH,i).pLong = lon_vals[i];
	}
	fVertexPtsH	 = vertexPtsH;// get first and last, lat/lon values, then last-first/total-1 = dlat/dlon
	
	status = nc_inq_dim(ncid, recid, recname, &recs);
	if (status != NC_NOERR) {err = -1; goto done;}
	fTimeHdl = (Seconds**)_NewHandleClear(recs*sizeof(Seconds));
	if (!fTimeHdl) {err = memFullErr; goto done;}
	for (i=0;i<recs;i++)
	{
		Seconds newTime;
		// possible units are, HOURS, MINUTES, SECONDS,...
		timeIndex = i;
		//status = nc_get_var1_float(ncid, recid, &timeIndex, &timeVal);	// recid is the dimension id not the variable id
		status = nc_get_var1_float(ncid, timeid, &timeIndex, &timeVal);
		if (status != NC_NOERR) {strcpy(errmsg,"Error reading times from NetCDF file"); err = -1; goto done;}
		newTime = RoundDateSeconds(round(startTime2+timeVal*timeConversion));
		//newTime = startTime2+timeVal*timeConversion;
		INDEXH(fTimeHdl,i) = newTime;	// which start time where?
		if (i==0) startTime = newTime + fTimeShift;
		//INDEXH(fTimeHdl,i) = startTime2+timeVal*timeConversion;	// which start time where?
		//if (i==0) startTime = startTime2+timeVal*timeConversion + fTimeShift;
	}
	
	fNumNodes = nodeLength;
	
	// check if file has topology in it
	{
		status = nc_inq_varid(ncid, "nv", &nv_varid); //Navy
		if (status != NC_NOERR) {/*err = -1; goto done;*/}
		else
		{
			status = nc_inq_varid(ncid, "nbe", &nbe_varid); //Navy
			if (status != NC_NOERR) {/*err = -1; goto done;*/}
			else 
			{
				bTopInfoInFile = true;
				status = nc_inq_attlen(ncid, nbe_varid, "order", &t_len);
				topOrder = new char[t_len+1];
				status = nc_get_att_text(ncid, nbe_varid, "order", topOrder);
				if (status != NC_NOERR) {isCCW = false;} // for now to suppport old FVCOM
				topOrder[t_len] = '\0'; 
				if (!strncmpnocase (topOrder, "CW", 2))
					isCCW = false;
				else if (!strncmpnocase (topOrder, "CCW", 3))
					isCCW = true;
				// if order is there let it default to true, that will eventually be default
			}
		}
		if (bTopInfoInFile)
		{
			status = nc_inq_dimid(ncid, "nele", &neleid);	
			if (status != NC_NOERR) {err = -1; goto done;}	
			status = nc_inq_dimlen(ncid, neleid, &neleLength);
			if (status != NC_NOERR) {err = -1; goto done;}	
			fNumEles = neleLength;
			top_verts = new long[neleLength*3]; 
			if (!top_verts ) {err = memFullErr; goto done;}
			top_neighbors = new long[neleLength*3]; 
			if (!top_neighbors ) {err = memFullErr; goto done;}
			top_count[0] = 3;
			top_count[1] = neleLength;
			status = nc_get_vara_long(ncid, nv_varid, topIndex, top_count, top_verts);
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_get_vara_long(ncid, nbe_varid, topIndex, top_count, top_neighbors);
			if (status != NC_NOERR) {err = -1; goto done;}
			
			//determine if velocities are on triangles
			status = nc_inq_varid(ncid, "u", &curr_ucmp_id);
			if (status != NC_NOERR) {err = -1; goto done;}
			status = nc_inq_varndims(ncid, curr_ucmp_id, &uv_ndims);
			if (status != NC_NOERR) {err = -1; goto done;}
			
			status = nc_inq_vardimid (ncid, curr_ucmp_id, uv_dimid);	// see if dimid(1) or (2) == nele or node, depends on uv_ndims
			if (status==NC_NOERR) 
			{
				if (uv_ndims == 3 && uv_dimid[2] == neleid)
				{bVelocitiesOnTriangles = true;}
				else if (uv_ndims == 2 && uv_dimid[1] == neleid)
				{bVelocitiesOnTriangles = true;}
			}
			
		}
	}
	
	status = nc_close(ncid);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	if (!bndry_indices || !bndry_nums || !bndry_type) {err = memFullErr; goto done;}
	
	
	// look for topology in the file
	if (topFilePath[0]) 
	{
		err = (dynamic_cast<TimeGridVelTri*>(this))->ReadTopology(topFilePath); 
		goto depths;
	}
	
	if (bTopInfoInFile/*bVelocitiesOnTriangles*/)
		err = ReorderPoints2(bndry_indices,bndry_nums,bndry_type,nbndLength,top_verts,top_neighbors,neleLength,isCCW);	 
	else
		err = ReorderPoints(bndry_indices,bndry_nums,bndry_type,nbndLength);	 
	
depths:
	if (err) goto done;
	// also translate to fDepthDataInfo and fDepthsH here, using sigma or zgrid info
	
	if (totalDepthsH)
	{
		for (i=0; i<fNumNodes; i++)
		{
			long n;			
			n = i;
			if (n<0 || n>= fNumNodes) {printError("indices messed up"); err=-1; goto done;}
			INDEXH(totalDepthsH,i) = depth_vals[n] * scale_factor;
		}
		//((TTriGridVel*)fGrid)->SetDepths(totalDepthsH);
	}
	
	// CalculateVerticalGrid(sigmaLength,sigmaLevelsH,totalDepthsH);	// maybe multigrid
	{
		long j,index = 0;
		fDepthDataInfo = (DepthDataInfoH)_NewHandle(sizeof(**fDepthDataInfo)*fNumNodes);
		if(!fDepthDataInfo){TechError("TimeGridVelTri_c::TextRead()", "_NewHandle()", 0); err = memFullErr; goto done;}
		//if (fVar.gridType==TWO_D || fVar.gridType==MULTILAYER) 
		if (fVar.gridType==TWO_D) 
		{
			if (totalDepthsH) 
			{
				fDepthsH = (FLOATH)_NewHandleClear(nodeLength*sizeof(float));
				if (!fDepthsH) {TechError("TimeGridVelTri_c::TextRead()", "_NewHandle()", 0); err = memFullErr; goto done;}
				for (i=0; i<fNumNodes; i++)
				{
					(*fDepthsH)[i] = (*totalDepthsH)[i];
				}
			}
			//fDepthsH = totalDepthsH;	// may be null, call it barotropic if depths exist??
		}	
		// assign arrays
		else
		{	//TWO_D grid won't need fDepthsH
			fDepthsH = (FLOATH)_NewHandle(sizeof(float)*fNumNodes*fVar.maxNumDepths);
			if(!fDepthsH){TechError("TimeGridVelTri_c::TextRead()", "_NewHandle()", 0); err = memFullErr; goto done;}
			
		}
		// code goes here, if velocities on triangles need to interpolate total depth I think, or use this differently
		for (i=0;i<fNumNodes;i++)
		{
			// might want to order all surface depths, all sigma1, etc., but then indexToDepthData wouldn't work
			// have 2D case, zgrid case as well
			if (fVar.gridType==TWO_D)
			{
				if (totalDepthsH) (*fDepthDataInfo)[i].totalDepth = (*totalDepthsH)[i];
				else (*fDepthDataInfo)[i].totalDepth = -1;	// no depth data
				(*fDepthDataInfo)[i].indexToDepthData = i;
				(*fDepthDataInfo)[i].numDepths = 1;
			}
			/*else if (fVar.gridType==MULTILAYER)
			 {
			 if (totalDepthsH) (*fDepthDataInfo)[i].totalDepth = (*totalDepthsH)[i];
			 else (*fDepthDataInfo)[i].totalDepth = -1;	// no depth data, this should be an error I think
			 (*fDepthDataInfo)[i].indexToDepthData = 0;
			 (*fDepthDataInfo)[i].numDepths = sigmaLength;
			 }*/
			else
			{
				(*fDepthDataInfo)[i].totalDepth = (*totalDepthsH)[i];
				(*fDepthDataInfo)[i].indexToDepthData = index;
				(*fDepthDataInfo)[i].numDepths = sigmaLength;
				for (j=0;j<sigmaLength;j++)
				{
					//(*fDepthsH)[index+j] = (*totalDepthsH)[i]*(1-(*sigmaLevelsH)[j]);
					//if (fVar.gridType==MULTILAYER) (*fDepthsH)[index+j] = (*totalDepthsH)[i]*(j);	// check this
					if (fVar.gridType==MULTILAYER) /*(*fDepthsH)[index+j] = (sigma_vals[j]);*/	// check this, measured from the bottom
						// since depth is measured from bottom should recalculate the depths for each point
					{
						if (( (*totalDepthsH)[i] - sigma_vals[sigmaLength - j - 1]) >= 0) 
							(*fDepthsH)[index+j] = (*totalDepthsH)[i] - sigma_vals[sigmaLength - j - 1] ; 
						else (*fDepthsH)[index+j] = (*totalDepthsH)[i]+1;
					}
					else (*fDepthsH)[index+j] = (*totalDepthsH)[i]*(1-sigma_vals[j]);
					//(*fDepthsH)[j*fNumNodes+i] = totalDepthsH[i]*(1-sigmaLevelsH[j]);
				}
				index+=sigmaLength;
			}
		}
	}
	if (totalDepthsH)	// why is this here twice?
	{
		for (i=0; i<fNumNodes; i++)
		{
			long n = i;
			
			if (fVerdatToNetCDFH) n = INDEXH(fVerdatToNetCDFH,i);
			if (n<0 || n>= fNumNodes) {printError("indices messed up"); err=-1; goto done;}
			INDEXH(totalDepthsH,i) = depth_vals[n] * scale_factor;
		}
		(dynamic_cast<TTriGridVel*>(fGrid))->SetDepths(totalDepthsH);
	}
	
done:
	if (err)
	{
		if (!errmsg[0]) 
			strcpy(errmsg,"Error opening NetCDF file");
		printNote(errmsg);
		//printNote("Error opening NetCDF file");
		if(fGrid)
		{
			fGrid ->Dispose();
			delete fGrid;
			fGrid = 0;
		}
		if(vertexPtsH) {DisposeHandle((Handle)vertexPtsH); vertexPtsH = 0;	fVertexPtsH	 = 0;}
		if(sigmaLevelsH) {DisposeHandle((Handle)sigmaLevelsH); sigmaLevelsH = 0;}
	}
	//printNote("NetCDF triangular grid model current mover is not yet implemented");
	
	if (timeUnits) delete [] timeUnits;
	if (lat_vals) delete [] lat_vals;
	if (lon_vals) delete [] lon_vals;
	if (depth_vals) delete [] depth_vals;
	if (sigma_vals) delete [] sigma_vals;
	if (bndry_indices) delete [] bndry_indices;
	if (bndry_nums) delete [] bndry_nums;
	if (bndry_type) delete [] bndry_type;
	if (topOrder) delete [] topOrder;
	
	return err;
}


OSErr TimeGridVelTri_c::ReadTimeData(long index,VelocityFH *velocityH, char* errmsg) 
{	// - needs to be updated once triangle grid format is set
	OSErr err = 0;
	long i,j;
	char path[256], outPath[256]; 
	int status, ncid, numdims, uv_ndims;
	int curr_ucmp_id, curr_vcmp_id, uv_dimid[3], nele_id;
	static size_t curr_index[] = {0,0,0,0};
	static size_t curr_count[4];
	float *curr_uvals,*curr_vvals, fill_value, dry_value = 0;
	long totalNumberOfVels = fNumNodes * fVar.maxNumDepths, numVelsAtDepthLevel=0;
	VelocityFH velH = 0;
	long numNodes = fNumNodes;
	long numTris = fNumEles;
	long numDepths = fVar.maxNumDepths;	// assume will always have full set of depths at each point for now
	double scale_factor = 1.;
	
	errmsg[0]=0;
	
	strcpy(path,fVar.pathName);
	if (!path || !path[0]) return -1;
	
	status = nc_open(path, NC_NOWRITE, &ncid);
	if (status != NC_NOERR) {err = -1; goto done;}
	/*if (status != NC_NOERR)
	{
#if TARGET_API_MAC_CARBON
		err = ConvertTraditionalPathToUnixPath((const char *) path, outPath, kMaxNameLen) ;
		status = nc_open(outPath, NC_NOWRITE, &ncid);
#endif
		if (status != NC_NOERR) {err = -1; goto done;}
	}*/
	status = nc_inq_ndims(ncid, &numdims);	// in general it's not the total number of dimensions but the number the variable depends on
	if (status != NC_NOERR) {err = -1; goto done;}
	
	curr_index[0] = index;	// time 
	curr_count[0] = 1;	// take one at a time
	
	// check for sigma or zgrid dimension
	if (numdims>=6)	// should check what the dimensions are
	{
		//curr_count[1] = 1;	// depth
		curr_count[1] = numDepths;	// depth
		//curr_count[1] = depthlength;	// depth
		curr_count[2] = numNodes;
	}
	else
	{
		curr_count[1] = numNodes;	
	}
	status = nc_inq_varid(ncid, "u", &curr_ucmp_id);
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_inq_varid(ncid, "v", &curr_vcmp_id);
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_inq_varndims(ncid, curr_ucmp_id, &uv_ndims);
	if (status==NC_NOERR){if (numdims < 6 && uv_ndims==3) {curr_count[1] = numDepths; curr_count[2] = numNodes;}}	// could have more dimensions than are used in u,v
	if (status==NC_NOERR){if (numdims >= 6 && uv_ndims==2) {curr_count[1] = numNodes;}}	// could have more dimensions than are used in u,v
	
	status = nc_inq_vardimid (ncid, curr_ucmp_id, uv_dimid);	// see if dimid(1) or (2) == nele or node, depends on uv_ndims
	if (status==NC_NOERR) 
	{
		status = nc_inq_dimid (ncid, "nele", &nele_id);
		if (status==NC_NOERR)
		{
			if (uv_ndims == 3 && uv_dimid[2] == nele_id)
			{bVelocitiesOnTriangles = true; curr_count[2] = numTris;}
			else if (uv_ndims == 2 && uv_dimid[1] == nele_id)
			{bVelocitiesOnTriangles = true; curr_count[1] = numTris;}
		}
	}
	if (bVelocitiesOnTriangles) 
	{
		totalNumberOfVels = numTris * fVar.maxNumDepths;
		numVelsAtDepthLevel = numTris;
	}
	else
		numVelsAtDepthLevel = numNodes;

	curr_uvals = new float[totalNumberOfVels]; 
	if(!curr_uvals) {TechError("TimeGridVelTri_c::ReadTimeData()", "new[]", 0); err = memFullErr; goto done;}
	curr_vvals = new float[totalNumberOfVels]; 
	if(!curr_vvals) {TechError("TimeGridVelTri_c::ReadTimeData()", "new[]", 0); err = memFullErr; goto done;}
	
	status = nc_get_vara_float(ncid, curr_ucmp_id, curr_index, curr_count, curr_uvals);
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_get_vara_float(ncid, curr_vcmp_id, curr_index, curr_count, curr_vvals);
	if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_get_att_float(ncid, curr_ucmp_id, "missing_value", &fill_value);// missing_value vs _FillValue
	if (status != NC_NOERR) {/*err = -1; goto done;*/fill_value=-9999.;}
	status = nc_get_att_double(ncid, curr_ucmp_id, "scale_factor", &scale_factor);
	//if (status != NC_NOERR) {err = -1; goto done;}
	status = nc_get_att_float(ncid, curr_ucmp_id, "dry_value", &dry_value);// missing_value vs _FillValue
	if (status != NC_NOERR) {/*err = -1; goto done;*/}  
	status = nc_close(ncid);
	if (status != NC_NOERR) {err = -1; goto done;}
	
	velH = (VelocityFH)_NewHandleClear(totalNumberOfVels * sizeof(VelocityFRec));
	if (!velH) {err = memFullErr; goto done;}
	for (j=0;j<numDepths;j++)
	{
		//for (i=0;i<totalNumberOfVels;i++)
		for (i=0;i<numVelsAtDepthLevel;i++)
			//for (i=0;i<numNodes;i++)
		{
			// really need to store the fill_value data and check for it when moving or drawing
			/*if (curr_uvals[i]==0.||curr_vvals[i]==0.)
			 curr_uvals[i] = curr_vvals[i] = 1e-06;
			 if (curr_uvals[i]==fill_value)
			 curr_uvals[i]=0.;
			 if (curr_vvals[i]==fill_value)
			 curr_vvals[i]=0.;
			 // for now until we decide what to do with the dry value flag
			 if (curr_uvals[i]==dry_value)
			 curr_uvals[i]=0.;
			 if (curr_vvals[i]==dry_value)
			 curr_vvals[i]=0.;
			 INDEXH(velH,i).u = curr_uvals[i];	// need units
			 INDEXH(velH,i).v = curr_vvals[i];*/
			/*if (curr_uvals[j*fNumNodes+i]==0.||curr_vvals[j*fNumNodes+i]==0.)
			 curr_uvals[j*fNumNodes+i] = curr_vvals[j*fNumNodes+i] = 1e-06;
			 if (curr_uvals[j*fNumNodes+i]==fill_value)
			 curr_uvals[j*fNumNodes+i]=0.;
			 if (curr_vvals[j*fNumNodes+i]==fill_value)
			 curr_vvals[j*fNumNodes+i]=0.;*/
			if (curr_uvals[j*numVelsAtDepthLevel+i]==0.||curr_vvals[j*numVelsAtDepthLevel+i]==0.)
				curr_uvals[j*numVelsAtDepthLevel+i] = curr_vvals[j*numVelsAtDepthLevel+i] = 1e-06;
			if (curr_uvals[j*numVelsAtDepthLevel+i]==fill_value)
				curr_uvals[j*numVelsAtDepthLevel+i]=0.;
			if (curr_vvals[j*numVelsAtDepthLevel+i]==fill_value)
				curr_vvals[j*numVelsAtDepthLevel+i]=0.;
			//if (fVar.gridType==MULTILAYER /*sigmaReversed*/)
			/*{
			 INDEXH(velH,(numDepths-j-1)*fNumNodes+i).u = curr_uvals[j*fNumNodes+i];	// need units
			 INDEXH(velH,(numDepths-j-1)*fNumNodes+i).v = curr_vvals[j*fNumNodes+i];	// also need to reverse top to bottom (if sigma is reversed...)
			 }
			 else*/
			{
				//INDEXH(velH,i*numDepths+(numDepths-j-1)).u = curr_uvals[j*fNumNodes+i];	// need units
				//INDEXH(velH,i*numDepths+(numDepths-j-1)).v = curr_vvals[j*fNumNodes+i];	// also need to reverse top to bottom
				INDEXH(velH,i*numDepths+(numDepths-j-1)).u = curr_uvals[j*numVelsAtDepthLevel+i];	// need units
				INDEXH(velH,i*numDepths+(numDepths-j-1)).v = curr_vvals[j*numVelsAtDepthLevel+i];	// also need to reverse top to bottom
			}
		}
	}
	*velocityH = velH;
	fFillValue = fill_value;
	if (scale_factor!=1.) fVar.fileScaleFactor = scale_factor;
	
done:
	if (err)
	{
		strcpy(errmsg,"Error reading current data from NetCDF file");
		// We don't want to put up an error message here because it can lead to an infinite loop of messages.
		if(velH) {DisposeHandle((Handle)velH); velH = 0;}
	}
	if (curr_uvals) delete [] curr_uvals;
	if (curr_vvals) delete [] curr_vvals;
	return err;
}

OSErr TimeGridVelTri_c::ReorderPoints2(long *bndry_indices, long *bndry_nums, long *bndry_type, long numBoundaryPts, long *tri_verts, long *tri_neighbors, long ntri, Boolean isCCW) 
{
	OSErr err = 0;
	char errmsg[256];
	long i, n, nv = fNumNodes;
	long currentBoundary;
	long numVerdatPts = 0, numVerdatBreakPts = 0;
	
	LONGH vertFlagsH = (LONGH)_NewHandleClear(nv * sizeof(**vertFlagsH));
	LONGH verdatPtsH = (LONGH)_NewHandleClear(nv * sizeof(**verdatPtsH));
	LONGH verdatBreakPtsH = (LONGH)_NewHandleClear(nv * sizeof(**verdatBreakPtsH));
	
	TopologyHdl topo=0;
	DAGTreeStruct tree;
	
	tree.treeHdl = 0;
	TDagTree *dagTree = 0;
	
	LongPointHdl pts=0;
	VelocityFH velH = 0;
	WorldRect triBounds;
	//LONGH waterBoundariesH=0;
	//LONGH boundaryPtsH = 0;
	
	TTriGridVel *triGrid = nil;
	
	Boolean addOne = false;	// for debugging
	
	/////////////////////////////////////////////////
	
	
	if (!vertFlagsH || !verdatPtsH || !verdatBreakPtsH) {err = memFullErr; goto done;}
	
	// put boundary points into verdat list
	
	// code goes here, double check that the water boundary info is also reordered
	currentBoundary=1;
	if (bndry_nums[0]==0) addOne = true;	// for debugging
	for (i = 0; i < numBoundaryPts; i++)
	{	
		//short islandNum, index;
		long islandNum, index;
		index = bndry_indices[i];
		islandNum = bndry_nums[i];
		if (addOne) islandNum++;	// for debugging
		INDEXH(vertFlagsH,index-1) = 1;	// note that point has been used
		INDEXH(verdatPtsH,numVerdatPts++) = index-1;	// add to verdat list
		if (islandNum>currentBoundary)
		{
			// for verdat file indices are really point numbers, subtract one for actual index
			INDEXH(verdatBreakPtsH,numVerdatBreakPts++) = i;	// passed a break point
			currentBoundary++;
		}
		//INDEXH(boundaryPtsH,i) = bndry_indices[i]-1;
	}
	INDEXH(verdatBreakPtsH,numVerdatBreakPts++) = numBoundaryPts;
	
	// add the rest of the points to the verdat list (these points are the interior points)
	for(i = 0; i < nv; i++) {
		if(INDEXH(vertFlagsH,i) == 0)	
		{
			INDEXH(verdatPtsH,numVerdatPts++) = i;
			INDEXH(vertFlagsH,i) = 0; // mark as used
		}
	}
	if (numVerdatPts!=nv) 
	{
		printNote("Not all vertex points were used");
		// shrink handle
		_SetHandleSize((Handle)verdatPtsH,numVerdatPts*sizeof(long));
	}
	
	numVerdatPts = nv;	//for now, may reorder later
	pts = (LongPointHdl)_NewHandle(sizeof(LongPoint)*(numVerdatPts));
	if(pts == nil)
	{
		strcpy(errmsg,"Not enough memory to triangulate data.");
		return -1;
	}
	
	/////////////////////////////////////////////////
	for (i=0; i<=numVerdatPts; i++)
	{
		//long index;
		float fLong, fLat/*, fDepth*/;
		LongPoint vertex;
		
		if(i < numVerdatPts) 
		{	
			//index = i+1;
			//n = INDEXH(verdatPtsH,i);
			n = i;	// for now, not sure if need to reorder
			fLat = INDEXH(fVertexPtsH,n).pLat;	// don't need to store fVertexPtsH, just pass in and use here
			fLong = INDEXH(fVertexPtsH,n).pLong;
			vertex.v = (long)(fLat*1e6);
			vertex.h = (long)(fLong*1e6);
			
			//fDepth = 1.;	// this will be set from bathymetry, just a fudge here for outputting a verdat
			INDEXH(pts,i) = vertex;
		}
		else { // the last line should be all zeros
			//index = 0;
			//fLong = fLat = fDepth = 0.0;
		}
		/////////////////////////////////////////////////
	}
	// figure out the bounds
	triBounds = voidWorldRect;
	if(pts) 
	{
		LongPoint	thisLPoint;
		
		if(numVerdatPts > 0)
		{
			WorldPoint  wp;
			for(i=0;i<numVerdatPts;i++)
			{
				thisLPoint = INDEXH(pts,i);
				wp.pLat = thisLPoint.v;
				wp.pLong = thisLPoint.h;
				AddWPointToWRect(wp.pLat, wp.pLong, &triBounds);
			}
		}
	}
	
	/////////////////////////////////////////////////
	
	// shrink handle
	_SetHandleSize((Handle)verdatBreakPtsH,numVerdatBreakPts*sizeof(long));
	for(i = 0; i < numVerdatBreakPts; i++ )
	{
		INDEXH(verdatBreakPtsH,i)--;
	}
	
	DisplayMessage("NEXTMESSAGETEMP");
	DisplayMessage("Making Triangles");
	// use new maketriangles to force algorithm to avoid 3 points in the same row or column
	MySpinCursor(); // JLM 8/4/99
	//if (err = maketriangles(&topo,pts,numVerdatPts,verdatBreakPtsH,numVerdatBreakPts))
	if(!(topo = (TopologyHdl)_NewHandleClear(ntri * sizeof(Topology))))goto done;	
	
	// point and triangle indices should start with zero
	for(i = 0; i < 3*ntri; i ++)
	{
		tri_neighbors[i] = tri_neighbors[i] - 1;
		tri_verts[i] = tri_verts[i] - 1;
	}
	for(i = 0; i < ntri; i ++)
	{	// topology data needs to be CCW
		(*topo)[i].vertex1 = tri_verts[i];
		if (isCCW)
			(*topo)[i].vertex2 = tri_verts[i+ntri];
		else
			(*topo)[i].vertex3 = tri_verts[i+ntri];
		if (isCCW)
			(*topo)[i].vertex3 = tri_verts[i+2*ntri];
		else
			(*topo)[i].vertex2 = tri_verts[i+2*ntri];
		(*topo)[i].adjTri1 = tri_neighbors[i];
		if (isCCW)
			(*topo)[i].adjTri2 = tri_neighbors[i+ntri];
		else
			(*topo)[i].adjTri3 = tri_neighbors[i+ntri];
		if (isCCW)
			(*topo)[i].adjTri3 = tri_neighbors[i+2*ntri];
		else
			(*topo)[i].adjTri2 = tri_neighbors[i+2*ntri];
	}
	
	DisplayMessage("NEXTMESSAGETEMP");
	DisplayMessage("Making Dag Tree");
	MySpinCursor(); // JLM 8/4/99
	tree = MakeDagTree(topo, (LongPoint**)pts, errmsg); 
	MySpinCursor(); // JLM 8/4/99
	if (errmsg[0])	
	{err = -1; goto done;} 
	// sethandle size of the fTreeH to be tree.fNumBranches, the rest are zeros
	_SetHandleSize((Handle)tree.treeHdl,tree.numBranches*sizeof(DAG));
	/////////////////////////////////////////////////
	
	triGrid = new TTriGridVel;
	if (!triGrid)
	{		
		err = true;
		TechError("Error in TimeGridVelTri_c::ReorderPoints()","new TTriGridVel" ,err);
		goto done;
	}
	
	fGrid = (TTriGridVel*)triGrid;
	
	triGrid -> SetBounds(triBounds); 
	this->SetGridBounds(triBounds);
	
	dagTree = new TDagTree(pts,topo,tree.treeHdl,velH,tree.numBranches); 
	if(!dagTree)
	{
		err = -1;
		printError("Unable to create dag tree.");
		goto done;
	}
	
	triGrid -> SetDagTree(dagTree);
	//triGrid -> SetDepths(totalDepthH);	// used by PtCurMap to check vertical movement
	//if (topo) fNumEles = _GetHandleSize((Handle)topo)/sizeof(**topo);	// should be set in TextRead
	
	pts = 0;	// because fGrid is now responsible for it
	topo = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	tree.treeHdl = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	//totalDepthH = 0; // because fGrid is now responsible for it
	
	/////////////////////////////////////////////////
	/*numBoundaryPts = INDEXH(verdatBreakPtsH,numVerdatBreakPts-1)+1;
	waterBoundariesH = (LONGH)_NewHandle(sizeof(long)*numBoundaryPts);
	if (!waterBoundariesH) {err = memFullErr; goto done;}
	boundaryPtsH = (LONGH)_NewHandleClear(numBoundaryPts * sizeof(**boundaryPtsH));
	if (!boundaryPtsH) {err = memFullErr; goto done;}
	
	for (i=0;i<numBoundaryPts;i++)
	{
		INDEXH(waterBoundariesH,i)=1;	// default is land
		if (bndry_type[i]==1)	
			INDEXH(waterBoundariesH,i)=2;	// water boundary, this marks start point rather than end point...
		INDEXH(boundaryPtsH,i) = bndry_indices[i]-1;
	}*/
	
	// code goes here, do we want to store grid boundary and land/water information?
	/*if (waterBoundariesH)	
	{
		PtCurMap *map = CreateAndInitPtCurMap(fVar.pathName,triBounds); // the map bounds are the same as the grid bounds
		if (!map) {err=-1; goto done;}
		// maybe move up and have the map read in the boundary information
		map->SetBoundarySegs(verdatBreakPtsH);	
		map->SetWaterBoundaries(waterBoundariesH);
		map->SetBoundaryPoints(boundaryPtsH);
		
		*newMap = map;
	}
	else*/
	{
		//if (waterBoundariesH) {DisposeHandle((Handle)waterBoundariesH); waterBoundariesH=0;}
		if (verdatBreakPtsH) {DisposeHandle((Handle)verdatBreakPtsH); verdatBreakPtsH=0;}
		//if (boundaryPtsH) {DisposeHandle((Handle)boundaryPtsH); boundaryPtsH = 0;}
	}
	
	/////////////////////////////////////////////////
	//fVerdatToNetCDFH = verdatPtsH;	// this should be resized
	
done:
	if (err) printError("Error reordering gridpoints into verdat format");
	if (vertFlagsH) {DisposeHandle((Handle)vertFlagsH); vertFlagsH = 0;}
	if (verdatPtsH) {DisposeHandle((Handle)verdatPtsH); verdatPtsH = 0;}
	
	if(err)
	{
		if(!errmsg[0])
			strcpy(errmsg,"An error occurred in TimeGridVelTri_c::ReorderPoints");
		printError(errmsg); 
		if(pts) {DisposeHandle((Handle)pts); pts=0;}
		if(topo) {DisposeHandle((Handle)topo); topo=0;}
		if(velH) {DisposeHandle((Handle)velH); velH=0;}
		if(tree.treeHdl) {DisposeHandle((Handle)tree.treeHdl); tree.treeHdl=0;}
		
		if(fGrid)
		{
			fGrid ->Dispose();
			delete fGrid;
			fGrid = 0;
		}
		/*if (*newMap) 
		{
			(*newMap)->Dispose();
			delete *newMap;
			*newMap=0;
		}*/
		//if (waterBoundariesH) {DisposeHandle((Handle)waterBoundariesH); waterBoundariesH=0;}
		if (verdatBreakPtsH) {DisposeHandle((Handle)verdatBreakPtsH); verdatBreakPtsH = 0;}
		if (verdatPtsH) {DisposeHandle((Handle)verdatPtsH); verdatPtsH = 0;}
		//if (boundaryPtsH) {DisposeHandle((Handle)boundaryPtsH); boundaryPtsH = 0;}
	}
	return err;
}

OSErr TimeGridVelTri_c::ReorderPoints(long *bndry_indices, long *bndry_nums, long *bndry_type, long numBoundaryPts) 
{
	OSErr err = 0;
	char errmsg[256];
	long i, n, nv = fNumNodes;
	long currentBoundary;
	long numVerdatPts = 0, numVerdatBreakPts = 0;
	
	LONGH vertFlagsH = (LONGH)_NewHandleClear(nv * sizeof(**vertFlagsH));
	LONGH verdatPtsH = (LONGH)_NewHandleClear(nv * sizeof(**verdatPtsH));
	LONGH verdatBreakPtsH = (LONGH)_NewHandleClear(nv * sizeof(**verdatBreakPtsH));
	
	TopologyHdl topo=0;
	LongPointHdl pts=0;
	VelocityFH velH = 0;
	DAGTreeStruct tree;
	WorldRect triBounds;
	//LONGH waterBoundariesH=0;
	
	TTriGridVel *triGrid = nil;
	tree.treeHdl = 0;
	TDagTree *dagTree = 0;
	
	Boolean addOne = false;	// for debugging
	
	/////////////////////////////////////////////////
	
	
	if (!vertFlagsH || !verdatPtsH || !verdatBreakPtsH) {err = memFullErr; goto done;}
	
	// put boundary points into verdat list
	
	// code goes here, double check that the water boundary info is also reordered
	currentBoundary=1;
	if (bndry_nums[0]==0) addOne = true;	// for debugging
	for (i = 0; i < numBoundaryPts; i++)
	{	
		//short islandNum, index;
		long islandNum, index;
		index = bndry_indices[i];
		islandNum = bndry_nums[i];
		if (addOne) islandNum++;	// for debugging
		INDEXH(vertFlagsH,index-1) = 1;	// note that point has been used
		INDEXH(verdatPtsH,numVerdatPts++) = index-1;	// add to verdat list
		if (islandNum>currentBoundary)
		{
			// for verdat file indices are really point numbers, subtract one for actual index
			INDEXH(verdatBreakPtsH,numVerdatBreakPts++) = i;	// passed a break point
			currentBoundary++;
		}
	}
	INDEXH(verdatBreakPtsH,numVerdatBreakPts++) = numBoundaryPts;

	// add the rest of the points to the verdat list (these points are the interior points)
	for(i = 0; i < nv; i++) {
		if(INDEXH(vertFlagsH,i) == 0)	
		{
			INDEXH(verdatPtsH,numVerdatPts++) = i;
			INDEXH(vertFlagsH,i) = 0; // mark as used
		}
	}
	if (numVerdatPts!=nv) 
	{
		printNote("Not all vertex points were used");
		// shrink handle
		_SetHandleSize((Handle)verdatPtsH,numVerdatPts*sizeof(long));
	}
	pts = (LongPointHdl)_NewHandle(sizeof(LongPoint)*(numVerdatPts));
	if(pts == nil)
	{
		strcpy(errmsg,"Not enough memory to triangulate data.");
		return -1;
	}
	
	/////////////////////////////////////////////////
	
	for (i=0; i<=numVerdatPts; i++)
	{
		//long index;
		float fLong, fLat/*, fDepth*/;
		LongPoint vertex;
		
		if(i < numVerdatPts) 
		{	
			//index = i+1;
			n = INDEXH(verdatPtsH,i);
			fLat = INDEXH(fVertexPtsH,n).pLat;	// don't need to store fVertexPtsH, just pass in and use here
			fLong = INDEXH(fVertexPtsH,n).pLong;
			vertex.v = (long)(fLat*1e6);
			vertex.h = (long)(fLong*1e6);
			
			//fDepth = 1.;	// this will be set from bathymetry, just a fudge here for outputting a verdat
			INDEXH(pts,i) = vertex;
		}
		else { // the last line should be all zeros
			//index = 0;
			//fLong = fLat = fDepth = 0.0;
		}
		/////////////////////////////////////////////////
	}
	// figure out the bounds
	triBounds = voidWorldRect;
	if(pts) 
	{
		LongPoint	thisLPoint;
		
		if(numVerdatPts > 0)
		{
			WorldPoint  wp;
			for(i=0;i<numVerdatPts;i++)
			{
				thisLPoint = INDEXH(pts,i);
				wp.pLat = thisLPoint.v;
				wp.pLong = thisLPoint.h;
				AddWPointToWRect(wp.pLat, wp.pLong, &triBounds);
			}
		}
	}
	
	/////////////////////////////////////////////////
	
	// shrink handle
	_SetHandleSize((Handle)verdatBreakPtsH,numVerdatBreakPts*sizeof(long));
	for(i = 0; i < numVerdatBreakPts; i++ )
	{
		INDEXH(verdatBreakPtsH,i)--;
	}
	
	DisplayMessage("NEXTMESSAGETEMP");
	DisplayMessage("Making Triangles");
	// use new maketriangles to force algorithm to avoid 3 points in the same row or column
	MySpinCursor(); // JLM 8/4/99
	if (err = maketriangles(&topo,pts,numVerdatPts,verdatBreakPtsH,numVerdatBreakPts))
		goto done;
	
	DisplayMessage("NEXTMESSAGETEMP");
	DisplayMessage("Making Dag Tree");
	MySpinCursor(); // JLM 8/4/99
	tree = MakeDagTree(topo, (LongPoint**)pts, errmsg); 
	MySpinCursor(); // JLM 8/4/99
	if (errmsg[0])	
	{err = -1; goto done;} 
	// sethandle size of the fTreeH to be tree.fNumBranches, the rest are zeros
	_SetHandleSize((Handle)tree.treeHdl,tree.numBranches*sizeof(DAG));
	/////////////////////////////////////////////////
	
	triGrid = new TTriGridVel;
	if (!triGrid)
	{		
		err = true;
		TechError("Error in TimeGridVelTri_c::ReorderPoints()","new TTriGridVel" ,err);
		goto done;
	}
	
	fGrid = (TTriGridVel*)triGrid;
	
	triGrid -> SetBounds(triBounds); 
	this->SetGridBounds(triBounds);
	
	dagTree = new TDagTree(pts,topo,tree.treeHdl,velH,tree.numBranches); 
	if(!dagTree)
	{
		err = -1;
		printError("Unable to create dag tree.");
		goto done;
	}
	
	triGrid -> SetDagTree(dagTree);
	//triGrid -> SetDepths(totalDepthH);	// used by PtCurMap to check vertical movement
	if (topo) fNumEles = _GetHandleSize((Handle)topo)/sizeof(**topo);
	
	pts = 0;	// because fGrid is now responsible for it
	topo = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	tree.treeHdl = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	//totalDepthH = 0; // because fGrid is now responsible for it
	
	/////////////////////////////////////////////////
	/*numBoundaryPts = INDEXH(verdatBreakPtsH,numVerdatBreakPts-1)+1;
	waterBoundariesH = (LONGH)_NewHandle(sizeof(long)*numBoundaryPts);
	if (!waterBoundariesH) {err = memFullErr; goto done;}
	
	for (i=0;i<numBoundaryPts;i++)
	{
		INDEXH(waterBoundariesH,i)=1;	// default is land
		if (bndry_type[i]==1)	
			INDEXH(waterBoundariesH,i)=2;	// water boundary, this marks start point rather than end point...
	}*/
	
	// code goes here, do we want to store the grid boundary and land/water information?
	/*if (waterBoundariesH)
	{
		PtCurMap *map = CreateAndInitPtCurMap(fVar.pathName,triBounds); // the map bounds are the same as the grid bounds
		if (!map) {err=-1; goto done;}
		// maybe move up and have the map read in the boundary information
		map->SetBoundarySegs(verdatBreakPtsH);	
		map->SetWaterBoundaries(waterBoundariesH);
		
		*newMap = map;
	}
	else*/
	{
		//if (waterBoundariesH) {DisposeHandle((Handle)waterBoundariesH); waterBoundariesH=0;}
		if (verdatBreakPtsH) {DisposeHandle((Handle)verdatBreakPtsH); verdatBreakPtsH=0;}
	}
	
	/////////////////////////////////////////////////
	fVerdatToNetCDFH = verdatPtsH;	// this should be resized
	
done:
	if (err) printError("Error reordering gridpoints into verdat format");
	if (vertFlagsH) {DisposeHandle((Handle)vertFlagsH); vertFlagsH = 0;}
	
	if(err)
	{
		if(!errmsg[0])
			strcpy(errmsg,"An error occurred in TimeGridVelTri_c::ReorderPoints");
		printError(errmsg); 
		if(pts) {DisposeHandle((Handle)pts); pts=0;}
		if(topo) {DisposeHandle((Handle)topo); topo=0;}
		if(velH) {DisposeHandle((Handle)velH); velH=0;}
		if(tree.treeHdl) {DisposeHandle((Handle)tree.treeHdl); tree.treeHdl=0;}
		
		if(fGrid)
		{
			fGrid ->Dispose();
			delete fGrid;
			fGrid = 0;
		}
		/*if (*newMap) 
		{
			(*newMap)->Dispose();
			delete *newMap;
			*newMap=0;
		}*/
		//if (waterBoundariesH) {DisposeHandle((Handle)waterBoundariesH); waterBoundariesH=0;}
		if (verdatBreakPtsH) {DisposeHandle((Handle)verdatBreakPtsH); verdatBreakPtsH = 0;}
		if (verdatPtsH) {DisposeHandle((Handle)verdatPtsH); verdatPtsH = 0;}
	}
	return err;
}

long TimeGridVelTri_c::GetNumDepthLevels()
{
	// should have only one version of this for all grid types, but will have to redo the regular grid stuff with depth levels
	// and check both sigma grid and multilayer grid (and maybe others)
	long numDepthLevels = 0;
	OSErr err = 0;
	char path[256], outPath[256];
	int status, ncid, sigmaid, sigmavarid;
	size_t sigmaLength=0;
	//if (fDepthLevelsHdl) numDepthLevels = _GetHandleSize((Handle)fDepthLevelsHdl)/sizeof(**fDepthLevelsHdl);
	//status = nc_open(fVar.pathName, NC_NOWRITE, &ncid);
	//if (status != NC_NOERR) {/*err = -1; goto done;*/return -1;}
	strcpy(path,fVar.pathName);
	if (!path || !path[0]) return -1;
	//status = nc_open(fVar.pathName, NC_NOWRITE, &ncid);
	status = nc_open(path, NC_NOWRITE, &ncid);
	if (status != NC_NOERR) /*{err = -1; goto done;}*/
	{
#if TARGET_API_MAC_CARBON
		err = ConvertTraditionalPathToUnixPath((const char *) path, outPath, kMaxNameLen) ;
		status = nc_open(outPath, NC_NOWRITE, &ncid);
#endif
		if (status != NC_NOERR) {err = -1; return -1;}
	}
	status = nc_inq_dimid(ncid, "sigma", &sigmaid); 	
	if (status != NC_NOERR) 
	{
		numDepthLevels = 1;	// check for zgrid option here
	}	
	else
	{
		status = nc_inq_varid(ncid, "sigma", &sigmavarid); //Navy
		if (status != NC_NOERR) {numDepthLevels = 1;}	// require variable to match the dimension
		status = nc_inq_dimlen(ncid, sigmaid, &sigmaLength);
		if (status != NC_NOERR) {numDepthLevels = 1;}	// error in file
		//fVar.gridType = SIGMA;	// in theory we should track this on initial read...
		//fVar.maxNumDepths = sigmaLength;
		numDepthLevels = sigmaLength;
		//status = nc_get_vara_float(ncid, sigmavarid, &ptIndex, &sigma_count, sigma_vals);
		//if (status != NC_NOERR) {err = -1; goto done;}
		// once depth is read in 
	}
	
	//done:
	return numDepthLevels;     
}

OSErr TimeGridVelTri_c::ReadTopology(char* path)
{
	// import NetCDF triangle info so don't have to regenerate
	// this is same as curvilinear mover so may want to combine later
	char s[1024], errmsg[256];
	long i, numPoints, numTopoPoints, line = 0, numPts;
	CHARH f = 0;
	OSErr err = 0;
	
	TopologyHdl topo=0;
	LongPointHdl pts=0;
	FLOATH depths=0;
	VelocityFH velH = 0;
	DAGTreeStruct tree;
	WorldRect bounds = voidWorldRect;
	
	TTriGridVel *triGrid = nil;
	tree.treeHdl = 0;
	TDagTree *dagTree = 0;
	
	long numWaterBoundaries, numBoundaryPts, numBoundarySegs;
	LONGH boundarySegs=0, waterBoundaries=0, boundaryPts=0;
	
	errmsg[0]=0;
	
	if (!path || !path[0]) return 0;
	
	if (err = ReadFileContents(TERMINATED,0, 0, path, 0, 0, &f)) {
		TechError("TimeGridVelTri::ReadTopology()", "ReadFileContents()", err);
		goto done;
	}
	
	_HLock((Handle)f); // JLM 8/4/99
	
	// No header
	// start with transformation array and vertices
	MySpinCursor(); // JLM 8/4/99
	NthLineInTextOptimized(*f, (line)++, s, 1024); 
	if(IsTransposeArrayHeaderLine(s,&numPts)) // 
	{
		if (err = ReadTransposeArray(f,&line,&fVerdatToNetCDFH,numPts,errmsg)) 
		{strcpy(errmsg,"Error in ReadTransposeArray"); goto done;}
	}
	else 
		//{err=-1; strcpy(errmsg,"Error in Transpose header line"); goto done;}
	{
		//if (!bVelocitiesOnTriangles) {err=-1; strcpy(errmsg,"Error in Transpose header line"); goto done;}
		//else line--;
		line--;
	}
	if(err = ReadTVertices(f,&line,&pts,&depths,errmsg)) goto done;
	
	if(pts) 
	{
		LongPoint	thisLPoint;
		
		numPts = _GetHandleSize((Handle)pts)/sizeof(LongPoint);
		if(numPts > 0)
		{
			WorldPoint  wp;
			for(i=0;i<numPts;i++)
			{
				thisLPoint = INDEXH(pts,i);
				wp.pLat = thisLPoint.v;
				wp.pLong = thisLPoint.h;
				AddWPointToWRect(wp.pLat, wp.pLong, &bounds);
			}
		}
	}
	MySpinCursor();
	
	NthLineInTextOptimized(*f, (line)++, s, 1024); 
	if(IsBoundarySegmentHeaderLine(s,&numBoundarySegs)) // Boundary data from CATs
	{
		MySpinCursor();
		if (numBoundarySegs>0)
			err = ReadBoundarySegs(f,&line,&boundarySegs,numBoundarySegs,errmsg);
		if(err) goto done;
		NthLineInTextOptimized(*f, (line)++, s, 1024); 
	}
	else
	{
		//err = -1;
		//strcpy(errmsg,"Error in Boundary segment header line");
		//goto done;
		// not needed for 2D files, but we require for now
	}
	MySpinCursor(); // JLM 8/4/99
	
	if(IsWaterBoundaryHeaderLine(s,&numWaterBoundaries,&numBoundaryPts)) // Boundary types from CATs
	{
		MySpinCursor();
		err = ReadWaterBoundaries(f,&line,&waterBoundaries,numWaterBoundaries,numBoundaryPts,errmsg);
		if(err) goto done;
		NthLineInTextOptimized(*f, (line)++, s, 1024); 
	}
	else
	{
		//err = -1;
		//strcpy(errmsg,"Error in Water boundaries header line");
		//goto done;
		// not needed for 2D files, but we require for now
	}
	MySpinCursor(); // JLM 8/4/99
	//NthLineInTextOptimized(*f, (line)++, s, 1024); 
	
	if(IsBoundaryPointsHeaderLine(s,&numBoundaryPts)) // Boundary data from CATs
	{
		MySpinCursor();
		if (numBoundaryPts>0)
			err = ReadBoundaryPts(f,&line,&boundaryPts,numBoundaryPts,errmsg);
		if(err) goto done;
		NthLineInTextOptimized(*f, (line)++, s, 1024); 
	}
	else
	{
		//err = -1;
		//strcpy(errmsg,"Error in Boundary points header line");
		//goto done;
		// not always needed ? probably always needed for curvilinear
	}
	MySpinCursor(); // JLM 8/4/99
	
	if(IsTTopologyHeaderLine(s,&numTopoPoints)) // Topology from CATs
	{
		MySpinCursor();
		err = ReadTTopologyBody(f,&line,&topo,&velH,errmsg,numTopoPoints,FALSE);
		if(err) goto done;
		NthLineInTextOptimized(*f, (line)++, s, 1024); 
	}
	else
	{
		err = -1; // for now we require TTopology
		strcpy(errmsg,"Error in topology header line");
		if(err) goto done;
	}
	MySpinCursor(); // JLM 8/4/99
	
	
	//NthLineInTextOptimized(*f, (line)++, s, 1024); 
	
	if(IsTIndexedDagTreeHeaderLine(s,&numPoints))  // DagTree from CATs
	{
		MySpinCursor();
		err = ReadTIndexedDagTreeBody(f,&line,&tree,errmsg,numPoints);
		if(err) goto done;
	}
	else
	{
		err = -1; // for now we require TIndexedDagTree
		strcpy(errmsg,"Error in dag tree header line");
		if(err) goto done;
	}
	MySpinCursor(); // JLM 8/4/99
	
	/////////////////////////////////////////////////
	// code goes here, do we want to store grid boundary and land/water information?
	// check if bVelocitiesOnTriangles and boundaryPts
	/*if (waterBoundaries && boundarySegs)
	 {
	 //PtCurMap *map = CreateAndInitPtCurMap(fVar.userName,bounds); // the map bounds are the same as the grid bounds
	 PtCurMap *map = CreateAndInitPtCurMap("Extended Topology",bounds); // the map bounds are the same as the grid bounds
	 if (!map) {strcpy(errmsg,"Error creating ptcur map"); goto done;}
	 // maybe move up and have the map read in the boundary information
	 map->SetBoundarySegs(boundarySegs);	
	 map->SetWaterBoundaries(waterBoundaries);
	 //if (bVelocitiesOnTriangles && boundaryPts) map->SetBoundaryPoints(boundaryPts);	
	 if (boundaryPts) map->SetBoundaryPoints(boundaryPts);	
	 
	 *newMap = map;
	 }	
	 else*/	
	{
		if (waterBoundaries) {DisposeHandle((Handle)waterBoundaries); waterBoundaries=0;}
		if (boundarySegs) {DisposeHandle((Handle)boundarySegs); boundarySegs = 0;}
		if (boundaryPts) {DisposeHandle((Handle)boundaryPts); boundaryPts = 0;}
	}
	
	/////////////////////////////////////////////////
	
	
	triGrid = new TTriGridVel;
	if (!triGrid)
	{		
		err = true;
		TechError("Error in TimeGridVelTri::ReadTopology()","new TTriGridVel" ,err);
		goto done;
	}
	
	fGrid = (TTriGridVel*)triGrid;
	
	triGrid -> SetBounds(bounds); 
	this->SetGridBounds(bounds);
	
	dagTree = new TDagTree(pts,topo,tree.treeHdl,velH,tree.numBranches); 
	if(!dagTree)
	{
		printError("Unable to read Extended Topology file.");
		goto done;
	}
	
	triGrid -> SetDagTree(dagTree);
	//triGrid -> SetDepths(depths);
	
	pts = 0;	// because fGrid is now responsible for it
	topo = 0; // because fGrid is now responsible for it
	tree.treeHdl = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	//depths = 0;
	
done:
	
	if(depths) {DisposeHandle((Handle)depths); depths=0;}
	if(f) 
	{
		_HUnlock((Handle)f); 
		DisposeHandle((Handle)f); 
		f = 0;
	}
	
	if(err)
	{
		if(!errmsg[0])
			strcpy(errmsg,"An error occurred in TimeGridVelTri::ReadTopology");
		printError(errmsg); 
		if(pts) {DisposeHandle((Handle)pts); pts=0;}
		if(topo) {DisposeHandle((Handle)topo); topo=0;}
		if(velH) {DisposeHandle((Handle)velH); velH=0;}
		if(tree.treeHdl) {DisposeHandle((Handle)tree.treeHdl); tree.treeHdl=0;}
		if(depths) {DisposeHandle((Handle)depths); depths=0;}
		if(fGrid)
		{
			fGrid ->Dispose();
			delete fGrid;
			fGrid = 0;
		}
		/*if (*newMap) 
		 {
		 (*newMap)->Dispose();
		 delete *newMap;
		 *newMap=0;
		 }*/
		if (waterBoundaries) {DisposeHandle((Handle)waterBoundaries); waterBoundaries=0;}
		if (boundarySegs) {DisposeHandle((Handle)boundarySegs); boundarySegs = 0;}
		if (boundaryPts) {DisposeHandle((Handle)boundaryPts); boundaryPts = 0;}
	}
	return err;
}

// code to be used for gridcur and ptcur (and probably windcur)
TimeGridCurRect_c::TimeGridCurRect_c () : TimeGridVel_c()
{
	fTimeDataHdl = 0;
	
	fUserUnits = kUndefined;
}

VelocityRec TimeGridCurRect_c::GetScaledPatValue(const Seconds& model_time, WorldPoint3D refPoint)
{
	double timeAlpha;
	long index; 
	Seconds startTime,endTime;
	VelocityRec scaledPatVelocity;
	Boolean useEddyUncertainty = false;	
	OSErr err = 0;
	
	index = GetVelocityIndex(refPoint.p); 
	
	// Check for constant current 
	if(GetNumTimesInFile()==1 && !(GetNumFiles()>1))
	{
		// Calculate the interpolated velocity at the point
		if (index >= 0) 
		{
			scaledPatVelocity.u = INDEXH(fStartData.dataHdl,index).u;
			scaledPatVelocity.v = INDEXH(fStartData.dataHdl,index).v;
		}
		else	// set vel to zero
		{
			scaledPatVelocity.u = 0.;
			scaledPatVelocity.v = 0.;
		}
	}
	else // time varying current 
	{
		// Calculate the time weight factor
		if (GetNumFiles()>1 && fOverLap)
			startTime = fOverLapStartTime;
		else
			startTime = (*fTimeDataHdl)[fStartData.timeIndex].time;
		//startTime = (*fTimeDataHdl)[fStartData.timeIndex].time;
		endTime = (*fTimeDataHdl)[fEndData.timeIndex].time;
		timeAlpha = (endTime - model_time)/(double)(endTime - startTime);
		
		// Calculate the interpolated velocity at the point
		if (index >= 0) 
		{
			scaledPatVelocity.u = timeAlpha*INDEXH(fStartData.dataHdl,index).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index).u;
			scaledPatVelocity.v = timeAlpha*INDEXH(fStartData.dataHdl,index).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,index).v;
		}
		else	// set vel to zero
		{
			scaledPatVelocity.u = 0.;
			scaledPatVelocity.v = 0.;
		}
	}
	
	//scale:
	
	//scaledPatVelocity.u *= fVar.curScale; // may want to allow some sort of scale factor
	//scaledPatVelocity.v *= fVar.curScale; 
	
	
	return scaledPatVelocity;
}

void TimeGridCurRect_c::DisposeTimeHdl()
{	
	if(fTimeDataHdl) {DisposeHandle((Handle)fTimeDataHdl); fTimeDataHdl=0;}
	TimeGridVel_c::DisposeTimeHdl();
}


long TimeGridCurRect_c::GetNumTimesInFile()
{
	long numTimes = 0;
	
	if (fTimeDataHdl) numTimes = _GetHandleSize((Handle)fTimeDataHdl)/sizeof(**fTimeDataHdl);
	return numTimes;     
}

OSErr TimeGridCurRect_c::ReadInputFileNames(CHARH fileBufH, long *line, long numFiles, PtCurFileInfoH *inputFilesH, char *pathOfInputfile)
{
	long i,numScanned;
	DateTimeRec time;
	Seconds timeSeconds;
	OSErr err = 0;
	char s[1024], classicPath[256];
	
	PtCurFileInfoH inputFilesHdl = (PtCurFileInfoH)_NewHandle(sizeof(PtCurFileInfo)*numFiles);
	if(!inputFilesHdl) {TechError("TimeGridCurRect_c::ReadInputFileNames()", "_NewHandle()", 0); err = memFullErr; goto done;}
	for (i=0;i<numFiles;i++)	// should count files as go along, and check that they exist ?
	{
		NthLineInTextNonOptimized(*fileBufH, (*line)++, s, 1024); 	// check it is a [FILE] line
		//strcpy((*inputFilesHdl)[i].pathName,s+strlen("[FILE]\t"));
		RemoveLeadingAndTrailingWhiteSpace(s);
		strcpy((*inputFilesHdl)[i].pathName,s+strlen("[FILE] "));
		RemoveLeadingAndTrailingWhiteSpace((*inputFilesHdl)[i].pathName);
		// allow for a path relative to the GNOME directory
		ResolvePathFromInputFile(pathOfInputfile,(*inputFilesHdl)[i].pathName); // JLM 6/8/10, we need to pass in the input file path so we can use it here
		
		if((*inputFilesHdl)[i].pathName[0] && FileExists(0,0,(*inputFilesHdl)[i].pathName))
		{
			//
		}	
		else 
		{
			char msg[256];
			sprintf(msg,"PATH to data File does not exist.%s%s",NEWLINESTRING,(*inputFilesHdl)[i].pathName);
			printError(msg);
			err = true;
			goto done;
		}
		
		
		NthLineInTextNonOptimized(*fileBufH, (*line)++, s, 1024); // check it is a [STARTTIME] line
		RemoveLeadingAndTrailingWhiteSpace(s);
		
		numScanned=sscanf(s+strlen("[STARTTIME]"), "%hd %hd %hd %hd %hd",
						  &time.day, &time.month, &time.year,
						  &time.hour, &time.minute) ;
		if (numScanned!= 5)
		{ err = -1; TechError("TimeGridCurRect_c::ReadInputFileNames()", "sscanf() == 5", 0); goto done; }
		// not allowing constant current in separate file
		//if (time.day == time.month == time.year == time.hour == time.minute == -1)
		if (time.day == -1 && time.month == -1 && time.year == -1 && time.hour == -1 && time.minute == -1)
		{
			timeSeconds = CONSTANTCURRENT;
		}
		else // time varying current
		{
			CheckYear(&time.year);
			
			time.second = 0;
			DateToSeconds (&time, &timeSeconds);
		}
		(*inputFilesHdl)[i].startTime = timeSeconds;
		
		NthLineInTextNonOptimized(*fileBufH, (*line)++, s, 1024); // check it is an [ENDTIME] line
		RemoveLeadingAndTrailingWhiteSpace(s);
		
		numScanned=sscanf(s+strlen("[ENDTIME]"), "%hd %hd %hd %hd %hd",
						  &time.day, &time.month, &time.year,
						  &time.hour, &time.minute) ;
		if (numScanned!= 5)
		{ err = -1; TechError("TimeGridCurRect_c::ReadInputFileNames()", "sscanf() == 5", 0); goto done; }
		if (time.day == -1 && time.month == -1 && time.year == -1 && time.hour == -1 && time.minute == -1)
			//if (time.day == time.month == time.year == time.hour == time.minute == -1)
		{
			timeSeconds = CONSTANTCURRENT;
		}
		else // time varying current
		{
			CheckYear(&time.year);
			
			time.second = 0;
			DateToSeconds (&time, &timeSeconds);
		}
		(*inputFilesHdl)[i].endTime = timeSeconds;
	}
	*inputFilesH = inputFilesHdl;
	
done:
	if (err)
	{
		if(inputFilesHdl) {DisposeHandle((Handle)inputFilesHdl); inputFilesHdl=0;}
	}
	return err;
}

OSErr TimeGridCurRect_c::GetStartTime(Seconds *startTime)
{
	OSErr err = 0;
	*startTime = 0;
	if (fStartData.timeIndex != UNASSIGNEDINDEX && fTimeDataHdl)
		*startTime = (*fTimeDataHdl)[fStartData.timeIndex].time;
	else return -1;
	return 0;
}

OSErr TimeGridCurRect_c::GetEndTime(Seconds *endTime)
{
	OSErr err = 0;
	*endTime = 0;
	if (fEndData.timeIndex != UNASSIGNEDINDEX && fTimeDataHdl)
		*endTime = (*fTimeDataHdl)[fEndData.timeIndex].time;
	else return -1;
	return 0;
}

//OSErr GridCurMover::ScanFileForTimes(char *path, PtCurTimeDataHdl *timeDataH,Boolean setStartTime)
OSErr ScanFileForTimes(char *path, PtCurTimeDataHdl *timeDataH, Seconds ***timeH/*, Boolean setStartTime*/)
{
	// scan through the file looking for times "[TIME "  (close file if necessary...)
	
	OSErr err = 0;
	CHARH h = 0;
	char *sectionOfFile = 0;
	
	long fileLength,lengthRemainingToScan,offset;
	long lengthToRead,lengthOfPartToScan,numTimeBlocks=0;
	long i, numScanned;
	DateTimeRec time;
	Seconds timeSeconds;	
	
	// allocate an empty handle
	PtCurTimeDataHdl timeDataHdl;
	timeDataHdl = (PtCurTimeDataHdl)_NewHandle(0);
	if(!timeDataHdl) {TechError("GridCurMover::ScanFileForTimes()", "_NewHandle()", 0); err = memFullErr; goto done;}

	// allocate an empty handle
	Seconds **timeHdl;
	timeHdl = (Seconds**)_NewHandle(0);
	if(!timeHdl) {TechError("GridCurMover::ScanFileForTimes()", "_NewHandle()", 0); err = memFullErr; goto done;}
	
	// think in terms of 100K blocks, allocate 101K, read 101K, scan 100K
	
#define kGridCurFileBufferSize  100000 // code goes here, increase to 100K or more
#define kGridCurFileBufferExtraCharSize  256
	
	err = MyGetFileSize(0,0,path,&fileLength);
	if(err) goto done;
	
	offset = 0;
	lengthRemainingToScan = fileLength - 5;
	
	// loop until whole file is read 
	
	h = (CHARH)_NewHandle(2* kGridCurFileBufferSize+1);
	if(!h){TechError("GridCurMover::TextRead()", "_NewHandle()", 0); err = memFullErr; goto done;}
	
	_HLock((Handle)h);
	sectionOfFile = *h;
	
	while (lengthRemainingToScan>0)
	{
		if(lengthRemainingToScan > 2* kGridCurFileBufferSize)
		{
			lengthToRead = kGridCurFileBufferSize + kGridCurFileBufferExtraCharSize; 
			lengthOfPartToScan = kGridCurFileBufferSize; 		
		}
		else
		{
			// deal with it in one piece
			// just read the rest of the file
			lengthToRead = fileLength - offset;
			lengthOfPartToScan = lengthToRead - 5; 
		}
		
		err = ReadSectionOfFile(0,0,path,offset,lengthToRead,sectionOfFile,0);
		if(err || !h) goto done;
		sectionOfFile[lengthToRead] = 0; // make it a C string
		
		lengthRemainingToScan -= lengthOfPartToScan;
		
		
		// scan 100K chars of the buffer for '['
		for(i = 0; i < lengthOfPartToScan; i++)
		{
			if(	sectionOfFile[i] == '[' 
			   && sectionOfFile[i+1] == 'T'
			   && sectionOfFile[i+2] == 'I'
			   && sectionOfFile[i+3] == 'M'
			   && sectionOfFile[i+4] == 'E')
			{
				// read and record the time and filePosition
				PtCurTimeData timeData;
				memset(&timeData,0,sizeof(timeData));
				timeData.fileOffsetToStartOfData = i + offset;
				
				if (numTimeBlocks > 0) 
				{
					(*timeDataHdl)[numTimeBlocks-1].lengthOfData = i+offset - (*timeDataHdl)[numTimeBlocks-1].fileOffsetToStartOfData;					
				}
				// some sort of a scan
				numScanned=sscanf(sectionOfFile+i+6, "%hd %hd %hd %hd %hd",
								  &time.day, &time.month, &time.year,
								  &time.hour, &time.minute) ;
				if (numScanned != 5)
				{ err = -1; TechError("GridCurMover::TextRead()", "sscanf() == 5", 0); goto done; }
				// check for constant current
				if (time.day == -1 && time.month == -1 && time.year == -1 && time.hour == -1 && time.minute == -1)
					//if (time.day == time.month == time.year == time.hour == time.minute == -1)
				{
					timeSeconds = CONSTANTCURRENT;
					//setStartTime = false;
				}
				else // time varying current
				{
					if (time.year < 1900)					// two digit date, so fix it
					{
						if (time.year >= 40 && time.year <= 99)	
							time.year += 1900;
						else
							time.year += 2000;					// correct for year 2000 (00 to 40)
					}
					
					time.second = 0;
					DateToSeconds (&time, &timeSeconds);
				}
				
				timeData.time = timeSeconds;
				
				// if we don't know the number of times ahead of time
				_SetHandleSize((Handle) timeDataHdl, (numTimeBlocks+1)*sizeof(timeData));
				if (_MemError()) { TechError("GridCurMover::TextRead()", "_SetHandleSize()", 0); goto done; }
				_SetHandleSize((Handle) timeHdl, (numTimeBlocks+1)*sizeof(Seconds));
				if (_MemError()) { TechError("GridCurMover::TextRead()", "_SetHandleSize()", 0); goto done; }
				/*if (numTimeBlocks==0 && setStartTime) 
				{	// set the default times to match the file
					model->SetModelTime(timeSeconds);
					model->SetStartTime(timeSeconds);
					model->NewDirtNotification(DIRTY_RUNBAR); // must reset the runbar
				}*/
				//(*timeDataHdl)[numTimeBlocks++] = timeData;				
				(*timeDataHdl)[numTimeBlocks] = timeData;				
				(*timeHdl)[numTimeBlocks] = timeData.time;		
				numTimeBlocks++;
			}
		}
		offset += lengthOfPartToScan;
	}
	if (numTimeBlocks > 0)  // last block goes to end of file
	{
		(*timeDataHdl)[numTimeBlocks-1].lengthOfData = fileLength - (*timeDataHdl)[numTimeBlocks-1].fileOffsetToStartOfData;				
	}
	*timeDataH = timeDataHdl;
	*timeH = timeHdl;
	
	
	
done:
	
	if(h) {
		_HUnlock((Handle)h); 
		DisposeHandle((Handle)h); 
		h = 0;
	}
	if (err)
	{
		if(timeDataHdl) {DisposeHandle((Handle)timeDataHdl); timeDataHdl=0;}
		if(timeHdl) {DisposeHandle((Handle)timeHdl); timeHdl=0;}
	}
	return err;
}
OSErr TimeGridCurRect_c::CheckAndScanFile(char *errmsg, const Seconds& model_time)
{
	Seconds time = model_time, startTime, endTime, lastEndTime, testTime; // AH 07/17/2012
	
	long i,numFiles = GetNumFiles();
	OSErr err = 0;
	
	errmsg[0]=0;
	if (fEndData.timeIndex!=UNASSIGNEDINDEX)
		testTime = (*fTimeDataHdl)[fEndData.timeIndex].time;	// currently loaded end time
	
	for (i=0;i<numFiles;i++)
	{
		startTime = (*fInputFilesHdl)[i].startTime;
		endTime = (*fInputFilesHdl)[i].endTime;
		if (startTime<=time&&time<=endTime && !(startTime==endTime))
		{
			//if(fTimeDataHdl) {DisposeHandle((Handle)fTimeDataHdl); fTimeDataHdl=0;}
			DisposeTimeHdl();
			err = ScanFileForTimes((*fInputFilesHdl)[i].pathName,&fTimeDataHdl,&fTimeHdl);	// AH 07/17/2012
			// code goes here, check that start/end times match
			strcpy(fVar.pathName,(*fInputFilesHdl)[i].pathName);
			fOverLap = false;
			return err;
		}
		if (startTime==endTime && startTime==time)	// one time in file, need to overlap
		{
			long fileNum;
			if (i<numFiles-1)
				fileNum = i+1;
			else
				fileNum = i;
			fOverLapStartTime = (*fInputFilesHdl)[fileNum-1].endTime;	// last entry in previous file
			DisposeLoadedData(&fStartData);
			/*if (fOverLapStartTime==testTime)	// shift end time data to start time data
			 {
			 fStartData = fEndData;
			 ClearLoadedData(&fEndData);
			 }
			 else*/
			{
				//if(fTimeDataHdl) {DisposeHandle((Handle)fTimeDataHdl); fTimeDataHdl=0;}
				DisposeTimeHdl();
				err = ScanFileForTimes((*fInputFilesHdl)[fileNum-1].pathName,&fTimeDataHdl,&fTimeHdl);	// AH 07/17/2012
				DisposeLoadedData(&fEndData);
				strcpy(fVar.pathName,(*fInputFilesHdl)[fileNum-1].pathName);
				if (err = this -> ReadTimeData(GetNumTimesInFile()-1,&fStartData.dataHdl,errmsg)) return err;
			}
			fStartData.timeIndex = UNASSIGNEDINDEX;
			//if(fTimeDataHdl) {DisposeHandle((Handle)fTimeDataHdl); fTimeDataHdl=0;}
			DisposeTimeHdl();
			err = ScanFileForTimes((*fInputFilesHdl)[fileNum].pathName,&fTimeDataHdl,&fTimeHdl);	// AH 07/17/2012
			strcpy(fVar.pathName,(*fInputFilesHdl)[fileNum].pathName);
			err = this -> ReadTimeData(0,&fEndData.dataHdl,errmsg);
			if(err) return err;
			fEndData.timeIndex = 0;
			fOverLap = true;
			return noErr;
		}
		if (i>0 && (lastEndTime<time && time<startTime))
		{
			fOverLapStartTime = (*fInputFilesHdl)[i-1].endTime;	// last entry in previous file
			DisposeLoadedData(&fStartData);
			if (fOverLapStartTime==testTime)	// shift end time data to start time data
			{
				fStartData = fEndData;
				ClearLoadedData(&fEndData);
			}
			else
			{
				//if(fTimeDataHdl) {DisposeHandle((Handle)fTimeDataHdl); fTimeDataHdl=0;}
				DisposeTimeHdl();
				err = ScanFileForTimes((*fInputFilesHdl)[i-1].pathName,&fTimeDataHdl,&fTimeHdl);	// AH 07/17/2012
				DisposeLoadedData(&fEndData);
				strcpy(fVar.pathName,(*fInputFilesHdl)[i-1].pathName);
				if (err = this -> ReadTimeData(GetNumTimesInFile()-1,&fStartData.dataHdl,errmsg)) return err;	
			}
			fStartData.timeIndex = UNASSIGNEDINDEX;
			//if(fTimeDataHdl) {DisposeHandle((Handle)fTimeDataHdl); fTimeDataHdl=0;}
			DisposeTimeHdl();
			err = ScanFileForTimes((*fInputFilesHdl)[i].pathName,&fTimeDataHdl,&fTimeHdl);	// AH 07/17/2012
			if (err) return err;
			strcpy(fVar.pathName,(*fInputFilesHdl)[i].pathName);
			err = this -> ReadTimeData(0,&fEndData.dataHdl,errmsg);
			if(err) return err;
			fEndData.timeIndex = 0;
			fOverLap = true;
			return noErr;
		}
		lastEndTime = endTime;
	}
	strcpy(errmsg,"Time outside of interval being modeled");
	return -1;	
	//return err;
}

OSErr TimeGridCurRect_c::ReadHeaderLines(char *path, WorldRect *bounds)
{
	char s[256], classicPath[256];
	long line = 0;
	CHARH f = 0;
	OSErr err = 0;
	long /*numLines,*/numScanned;
	double dLon,dLat,oLon,oLat;
	double lowLon,lowLat,highLon,highLat;
	Boolean velAtCenter = 0;
	Boolean velAtCorners = 0;
	//long numLinesInText, headerLines = 8;
	
	if (!path) return -1;
	
	if (err = ReadFileContents(TERMINATED,0, 0, path, 0, 0, &f)) goto done;
	//numLinesInText = NumLinesInText(*f);
	////
	// read the header
	///////////////////////
	/////////////////////////////////////////////////
	NthLineInTextOptimized(*f, line++, s, 256); // gridcur header
	if(fUserUnits == kUndefined)
	{	
#ifdef pyGNOME	// get rid of this and require units in file
		fUserUnits = kKnots;
#else
		// we have to ask the user for units...
		Boolean userCancel=false;
		short selectedUnits = kKnots; // knots will be default
		err = AskUserForUnits(&selectedUnits,&userCancel);
		if(err || userCancel) { err = -1; goto done;}
		fUserUnits = selectedUnits;
#endif
	}
	
	//
	NthLineInTextOptimized(*f, line++, s, 256); 
	if(!strstr(s,"NUMROWS")) { err = -2; goto done; }
	numScanned = sscanf(s+strlen("NUMROWS"),"%ld",&fNumRows);
	if(numScanned != 1 || fNumRows <= 0) { err = -2; goto done; }
	//
	NthLineInTextOptimized(*f, line++, s, 256); 
	if(!strstr(s,"NUMCOLS")) { err = -2; goto done; }
	numScanned = sscanf(s+strlen("NUMCOLS"),"%ld",&fNumCols);
	if(numScanned != 1 || fNumCols <= 0) { err = -2; goto done; }
	//
	NthLineInTextOptimized(*f, line++, s, 256); 
	
	if(s[0]=='S') // check if lat/long given as corner point and increment, and read in
	{
		if(!strstr(s,"STARTLAT")) { err = -2; goto done; }
		numScanned = sscanf(s+strlen("STARTLAT"),lfFix("%lf"),&oLat);
		if(numScanned != 1 ) { err = -2; goto done; }
		//
		NthLineInTextOptimized(*f, line++, s, 256); 
		if(!strstr(s,"STARTLONG")) { err = -2; goto done; }
		numScanned = sscanf(s+strlen("STARTLONG"),lfFix("%lf"),&oLon);
		if(numScanned != 1 ) { err = -2; goto done; }
		//
		NthLineInTextOptimized(*f, line++, s, 256); 
		if(!strstr(s,"DLAT")) { err = -2; goto done; }
		numScanned = sscanf(s+strlen("DLAT"),lfFix("%lf"),&dLat);
		if(numScanned != 1 || dLat <= 0) { err = -2; goto done; }
		//
		NthLineInTextOptimized(*f, line++, s, 256); 
		if(!strstr(s,"DLONG")) { err = -2; goto done; }
		numScanned = sscanf(s+strlen("DLONG"),lfFix("%lf"),&dLon);
		if(numScanned != 1 || dLon <= 0) { err = -2; goto done; }
		
		velAtCorners=true;
		//
	}
	else if(s[0]=='L') // check if lat/long bounds given, and read in
	{
		if(!strstr(s,"LOLAT")) { err = -2; goto done; }
		numScanned = sscanf(s+strlen("LOLAT"),lfFix("%lf"),&lowLat);
		if(numScanned != 1 ) { err = -2; goto done; }
		//
		NthLineInTextOptimized(*f, line++, s, 256); 
		if(!strstr(s,"HILAT")) { err = -2; goto done; }
		numScanned = sscanf(s+strlen("HILAT"),lfFix("%lf"),&highLat);
		if(numScanned != 1 ) { err = -2; goto done; }
		//
		NthLineInTextOptimized(*f, line++, s, 256); 
		if(!strstr(s,"LOLONG")) { err = -2; goto done; }
		numScanned = sscanf(s+strlen("LOLONG"),lfFix("%lf"),&lowLon);
		if(numScanned != 1 ) { err = -2; goto done; }
		//
		NthLineInTextOptimized(*f, line++, s, 256); 
		if(!strstr(s,"HILONG")) { err = -2; goto done; }
		numScanned = sscanf(s+strlen("HILONG"),lfFix("%lf"),&highLon);
		if(numScanned != 1 ) { err = -2; goto done; }
		
		velAtCenter=true;
	}
	else {err = -2; goto done; }
	//
	//NthLineInTextOptimized(*f, line++, s, 256); // row col u v header
	//
	
	// check hemisphere stuff here , code goes here
	if(velAtCenter)
	{
		(*bounds).loLat = lowLat*1000000;
		(*bounds).hiLat = highLat*1000000;
		(*bounds).loLong = lowLon*1000000;
		(*bounds).hiLong = highLon*1000000;
	}
	else if(velAtCorners)
	{
		(*bounds).loLat = round((oLat - dLat/2.0)*1000000);
		(*bounds).hiLat = round((oLat + (fNumRows-1)*dLat + dLat/2.0)*1000000);
		(*bounds).loLong = round((oLon - dLon/2.0)*1000000);
		(*bounds).hiLong = round((oLon + (fNumCols-1)*dLon + dLon/2.0)*1000000);
	}
	//numLines = numLinesInText - headerLines;	// allows user to leave out land points within grid (or standing water)
	
	NthLineInTextOptimized(*f, (line)++, s, 256);
	RemoveLeadingAndTrailingWhiteSpace(s);
	while ((s[0]=='[' && s[1]=='U') || s[0]==0)
	{	// [USERDATA] lines, and blank lines, generalize to anything but [FILE] ?
		NthLineInTextOptimized(*f, (line)++, s, 256);
		RemoveLeadingAndTrailingWhiteSpace(s);
	}
	if(!strstr(s,"[FILE]")) 
	{	// single file
		
		err = ScanFileForTimes(path,&fTimeDataHdl,&fTimeHdl);	// AH 07/17/2012
		
		if (err) goto done;
	}
	else
	{	// multiple files
		char errmsg[256];
		long numLinesInText = NumLinesInText(*f);
		long numFiles = (numLinesInText - (line - 1))/3;	// 3 lines for each file - filename, starttime, endtime
		//strcpy(fPathName,s+strlen("[FILE]\t"));
		strcpy(fVar.pathName,s+strlen("[FILE] "));
		sprintf(errmsg,"pathName = %s\n",fVar.pathName);
		printNote(errmsg);
		RemoveLeadingAndTrailingWhiteSpace(fVar.pathName);
		ResolvePathFromInputFile(path,fVar.pathName); // JLM 6/8/10
		sprintf(errmsg,"pathName = %s\n",path);
		printNote(errmsg);
		sprintf(errmsg,"resolved pathName = %s\n",fVar.pathName);
		printNote(errmsg);
		if(fVar.pathName[0] && FileExists(0,0,fVar.pathName))
		{
			err = ScanFileForTimes(fVar.pathName,&fTimeDataHdl,&fTimeHdl);	// AH 07/17/2012
			
			if (err) goto done;
			// code goes here, maybe do something different if constant current
			line--;
			err = /*dynamic_cast<TimeGridCurRect*>(this)->*/ReadInputFileNames(f,&line,numFiles,&fInputFilesHdl,path);
		}	
		else 
		{
			char msg[256];
			sprintf(msg,"PATH to GridCur data File does not exist.%s%s",NEWLINESTRING,fVar.pathName);
			printError(msg);
			err = true;
		}
		
		/*err = ScanFileForTimes(fPathName,&fTimeDataHdl,true);
		 if (err) goto done;
		 line--;
		 err = ReadInputFileNames(f,&line,numFiles,&fInputFilesHdl,path);*/
	}
	
done:
	if(f) { DisposeHandle((Handle)f); f = 0;}
	if(err)
	{
		if(err==memFullErr)
			TechError("TRectGridVel::ReadGridCurFile()", "_NewHandleClear()", 0); 
		else
			printError("Unable to read GridCur file.");
	}
	return err;
}

OSErr TimeGridCurRect_c::TextRead(char *path, char *topFilePath) 
{
	WorldRect bounds;
	OSErr err = 0;
	char pathName[256];
	
	TRectGridVel *rectGrid = nil;
	
	if (!path || !path[0]) return 0;
	
	//strcpy(fPathName,path);
	strcpy(fVar.pathName,path);
	
	//strcpy(pathName,fPathName);
	//SplitPathFile(pathName,fFileName);
	strcpy(pathName,fVar.pathName);
#ifndef pyGNOME
	SplitPathFile(pathName,fVar.userName);	// code goes here, this won't work on unix paths
#else
	SplitPathFileName (pathName, fVar.userName);
#endif
	// code goes here, we need to worry about really big files
	
	// do the readgridcur file stuff, store numrows, numcols, return the bounds
	err = this -> ReadHeaderLines(path,&bounds);
	if(err)
		goto done;
	
	/////////////////////////////////////////////////
	
	rectGrid = new TRectGridVel;
	if (!rectGrid)
	{		
		err = true;
		TechError("Error in GridCurMover::TextRead()","new TRectGridVel" ,err);
		goto done;
	}
	
	fGrid = (TGridVel*)rectGrid;
	
	rectGrid -> SetBounds(bounds); 
	this->SetGridBounds(bounds);
	
	// scan through the file looking for "[TIME ", then read and record the time, filePosition, and length of data
	// consider the possibility of multiple files
	/*NthLineInTextOptimized(*f, (line)++, s, 256); 
	 if(!strstr(s,"[FILE]")) 
	 {	// single file
	 err = ScanFileForTimes(path,&fTimeDataHdl,true);
	 if (err) goto done;
	 }
	 else
	 {	// multiple files
	 long numLinesInText = NumLinesInText(*f);
	 long numFiles = (numLinesInText - (line - 1))/3;	// 3 lines for each file - filename, starttime, endtime
	 strcpy(fPathName,s+strlen("[FILE]\t"));
	 err = ScanFileForTimes(fPathName,&fTimeDataHdl,true);
	 if (err) goto done;
	 line--;
	 err = ReadInputFileNames(f,&line,numFiles,&fInputFilesHdl,path);
	 }*/
	
	//err = ScanFileForTimes(path,&fTimeDataHdl);
	//if (err) goto done;
	
	
done:
	
	if(err)
	{
		printError("An error occurred in GridCurMover::TextRead"); 
		if(fGrid)
		{
			fGrid ->Dispose();
			delete fGrid;
			fGrid = 0;
		}
	}
	return err;
	
	// rest of file (i.e. velocity data) is read as needed
}

OSErr TimeGridCurRect_c::ReadTimeData(long index,VelocityFH *velocityH, char* errmsg) 
{
	char s[256], path[256]; 
	long i,line = 0;
	long offset,lengthToRead;
	CHARH h = 0;
	char *sectionOfFile = 0;
	char *strToMatch = 0;
	long len,numScanned;
	VelocityFH velH = 0;
	long totalNumberOfVels = fNumRows * fNumCols;
	long numLinesInBlock;
	
	OSErr err = 0;
	DateTimeRec time;
	Seconds timeSeconds;
	errmsg[0]=0;
	
	strcpy(path,fVar.pathName);
	if (!path || !path[0]) return -1;
	
	lengthToRead = (*fTimeDataHdl)[index].lengthOfData;
	offset = (*fTimeDataHdl)[index].fileOffsetToStartOfData;
	
	h = (CHARH)_NewHandle(lengthToRead+1);
	if(!h){TechError("TimeGridCurRect_c::ReadTimeData()", "_NewHandle()", 0); err = memFullErr; goto done;}
	
	_HLock((Handle)h);
	sectionOfFile = *h;			
	
	err = ReadSectionOfFile(0,0,path,offset,lengthToRead,sectionOfFile,0);
	if(err || !h) 
	{
		char firstPartOfLine[128];
		sprintf(errmsg,"Unable to open data file:%s",NEWLINESTRING);
		strncpy(firstPartOfLine,path,120);
		strcpy(firstPartOfLine+120,"...");
		strcat(errmsg,firstPartOfLine);
		goto done;
	}
	sectionOfFile[lengthToRead] = 0; // make it a C string
	numLinesInBlock = NumLinesInText(sectionOfFile);
	
	// some other way to calculate
	velH = (VelocityFH)_NewHandleClear(sizeof(**velH)*totalNumberOfVels);
	if(!velH){TechError("TimeGridCurRect_c::ReadTimeData()", "_NewHandle()", 0); err = memFullErr; goto done;}
	
	strToMatch = "[TIME]";
	len = strlen(strToMatch);
	NthLineInTextOptimized (sectionOfFile, line = 0, s, 256);
	if(!strncmp(s,strToMatch,len)) 
	{
		numScanned=sscanf(s+len, "%hd %hd %hd %hd %hd",
						  &time.day, &time.month, &time.year,
						  &time.hour, &time.minute) ;
		if (numScanned!= 5)
		{ err = -1; TechError("TimeGridCurRect_c::ReadTimeData()", "sscanf() == 5", 0); goto done; }
		// check for constant current
		if (time.day == -1 && time.month == -1 && time.year == -1 && time.hour == -1 && time.minute == -1)
			//if (time.year == time.month == time.day == time.hour == time.minute == -1) 
		{
			timeSeconds = CONSTANTCURRENT;
		}
		else // time varying current
		{
			if (time.year < 1900)					// two digit date, so fix it
			{
				if (time.year >= 40 && time.year <= 99)	
					time.year += 1900;
				else
					time.year += 2000;					// correct for year 2000 (00 to 40)
			}
			
			time.second = 0;
			DateToSeconds (&time, &timeSeconds);
		}
		
		// check time is correct
		if (timeSeconds!=(*fTimeDataHdl)[index].time)
		{ err = -1;  strcpy(errmsg,"Can't read data - times in the file have changed."); goto done; }
		line++;
	}
	
	// allow to omit areas of the grid with zero velocity, use length of data info
	//for(i=0;i<totalNumberOfVels;i++) // interior points
	for(i=0;i<numLinesInBlock-1;i++) // lines of data
	{
		VelocityRec vel;
		long rowNum,colNum;
		long index;
		
		NthLineInTextOptimized(sectionOfFile, line++, s, 256); 	// in theory should run out of lines eventually
		RemoveLeadingAndTrailingWhiteSpace(s);
		if(s[0] == 0) continue; // it's a blank line, allow this and skip the line
		numScanned = sscanf(s,lfFix("%ld %ld %lf %lf"),&rowNum,&colNum,&vel.u,&vel.v);
		if(numScanned != 4 
		   || rowNum <= 0 || rowNum > fNumRows
		   || colNum <= 0 || colNum > fNumCols
		   )
		{ 
			err = -1;  
			char firstPartOfLine[128];
			sprintf(errmsg,"Unable to read velocity data from line %ld:%s",line,NEWLINESTRING);
			strncpy(firstPartOfLine,s,120);
			strcpy(firstPartOfLine+120,"...");
			strcat(errmsg,firstPartOfLine);
			goto done; 
		}
		index = (rowNum -1) * fNumCols + colNum-1;
		(*velH)[index].u = vel.u; // units ??? assumed m/s
		(*velH)[index].v = vel.v; 
	}
	*velocityH = velH;
	
done:
	
	if(h) {
		_HUnlock((Handle)h); 
		DisposeHandle((Handle)h); 
		h = 0;
	}
	
	
	if(err)
	{
		if(!errmsg[0])
			strcpy(errmsg,"An error occurred in TimeGridCurRect_c::ReadTimeData");
		//printError(errmsg); // This alert causes a freeze up...
		// We don't want to put up an error message here because it can lead to an infinite loop of messages.
		if(velH) {DisposeHandle((Handle)velH); velH = 0;}
	}
	return err;
	
}

TimeGridCurTri_c::TimeGridCurTri_c () : TimeGridCurRect_c()
{
	memset(&fVar2,0,sizeof(fVar2));
	fVar2.arrowScale = 5;
	fVar2.arrowDepth = 0;
	fVar2.alongCurUncertainty = .5;
	fVar2.crossCurUncertainty = .25;
	//fVar.uncertMinimumInMPS = .05;
	fVar2.uncertMinimumInMPS = 0.0;
	fVar2.curScale = 1.0;
	fVar2.startTimeInHrs = 0.0;
	fVar2.durationInHrs = 24.0;
	fVar2.numLandPts = 0; // default that boundary velocities are given
	fVar2.maxNumDepths = 1; // 2D default
	fVar2.gridType = TWO_D; // 2D default
	fVar2.bLayerThickness = 0.; // FREESLIP default
	//
	// Override TCurrentMover defaults
	/*fDownCurUncertainty = -fVar2.alongCurUncertainty; 
	fUpCurUncertainty = fVar2.alongCurUncertainty; 	
	fRightCurUncertainty = fVar2.crossCurUncertainty;  
	fLeftCurUncertainty = -fVar2.crossCurUncertainty; 
	fDuration=fVar2.durationInHrs*3600.; //24 hrs as seconds 
	fUncertainStartTime = (long) (fVa2r.startTimeInHrs*3600.);*/
	//
/*	fGrid = 0;
	fTimeDataHdl = 0;
	fIsOptimizedForStep = false;
	fOverLap = false;		// for multiple files case
	fOverLapStartTime = 0;
	
	memset(&fStartData,0,sizeof(fStartData));
	fStartData.timeIndex = UNASSIGNEDINDEX; 
	fStartData.dataHdl = 0; 
	memset(&fEndData,0,sizeof(fEndData));
	fEndData.timeIndex = UNASSIGNEDINDEX;
	fEndData.dataHdl = 0;
	*/
	fDepthsH = 0;
	fDepthDataInfo = 0;
	//fInputFilesHdl = 0;	// for multiple files case
	
	//SetClassName (name); // short file name
}
long TimeGridCurTri_c::GetNumDepths(void)
{
	long numDepths = 0;
	if (fDepthsH) numDepths = _GetHandleSize((Handle)fDepthsH)/sizeof(**fDepthsH);
	
	return numDepths;
}
void TimeGridCurTri_c::GetDepthIndices(long ptIndex, float depthAtPoint, long *depthIndex1, long *depthIndex2)
{
	long indexToDepthData = (*fDepthDataInfo)[ptIndex].indexToDepthData;
	long numDepths = (*fDepthDataInfo)[ptIndex].numDepths;
	float totalDepth = (*fDepthDataInfo)[ptIndex].totalDepth;
	
	
	switch(fVar.gridType) 
	{
		case TWO_D:	// no depth data
			*depthIndex1 = indexToDepthData;
			*depthIndex2 = UNASSIGNEDINDEX;
			break;
		case BAROTROPIC:	// values same throughout column, but limit on total depth
			if (depthAtPoint <= totalDepth)
			{
				*depthIndex1 = indexToDepthData;
				*depthIndex2 = UNASSIGNEDINDEX;
			}
			else
			{
				*depthIndex1 = UNASSIGNEDINDEX;
				*depthIndex2 = UNASSIGNEDINDEX;
			}
			break;
		case MULTILAYER: //
			//break;
		case SIGMA: // 
			if (depthAtPoint <= totalDepth) // check data exists at chosen/LE depth for this point
			{
				long j;
				for(j=0;j<numDepths-1;j++)
				{
					if(INDEXH(fDepthsH,indexToDepthData+j)<depthAtPoint &&
					   depthAtPoint<=INDEXH(fDepthsH,indexToDepthData+j+1))
					{
						*depthIndex1 = indexToDepthData+j;
						*depthIndex2 = indexToDepthData+j+1;
					}
					else if(INDEXH(fDepthsH,indexToDepthData+j)==depthAtPoint)
					{
						*depthIndex1 = indexToDepthData+j;
						*depthIndex2 = UNASSIGNEDINDEX;
					}
				}
				if(INDEXH(fDepthsH,indexToDepthData)==depthAtPoint)	// handles single depth case
				{
					*depthIndex1 = indexToDepthData;
					*depthIndex2 = UNASSIGNEDINDEX;
				}
				else if(INDEXH(fDepthsH,indexToDepthData+numDepths-1)<depthAtPoint)
				{
					*depthIndex1 = indexToDepthData+numDepths-1;
					*depthIndex2 = UNASSIGNEDINDEX; //BOTTOM, for now just extrapolate lowest depth value (at bottom case?)
				}
				else if(INDEXH(fDepthsH,indexToDepthData)>depthAtPoint)
				{
					*depthIndex1 = indexToDepthData;
					*depthIndex2 = UNASSIGNEDINDEX; //TOP, for now just extrapolate highest depth value
				}
			}
			else // no data at this point
			{
				*depthIndex1 = UNASSIGNEDINDEX;
				*depthIndex2 = UNASSIGNEDINDEX;
			}
			break;
		default:
			*depthIndex1 = UNASSIGNEDINDEX;
			*depthIndex2 = UNASSIGNEDINDEX;
			break;
	}
}
VelocityRec TimeGridCurTri_c::GetScaledPatValue(const Seconds& model_time, WorldPoint3D refPoint)
{
	double timeAlpha, depth = refPoint.z;
	long ptIndex1,ptIndex2,ptIndex3; 
	Seconds startTime,endTime;
	InterpolationVal interpolationVal;
	VelocityRec scaledPatVelocity;
	Boolean useEddyUncertainty = false;	
	OSErr err = 0;
	
	memset(&interpolationVal,0,sizeof(interpolationVal));
	
	// Get the interpolation coefficients, alpha1,ptIndex1,alpha2,ptIndex2,alpha3,ptIndex3
	interpolationVal = fGrid -> GetInterpolationValues(refPoint.p);
	
	if (interpolationVal.ptIndex1 >= 0)  // if negative corresponds to negative ntri
	{
		ptIndex1 =  (*fDepthDataInfo)[interpolationVal.ptIndex1].indexToDepthData;
		ptIndex2 =  (*fDepthDataInfo)[interpolationVal.ptIndex2].indexToDepthData;
		ptIndex3 =  (*fDepthDataInfo)[interpolationVal.ptIndex3].indexToDepthData;
	}
	
	// code goes here, need interpolation in z if LE is below surface
	// what kind of weird things can triangles do below the surface ??
	if (depth>0 && interpolationVal.ptIndex1 >= 0) 
	{
		scaledPatVelocity = GetScaledPatValue3D(model_time,interpolationVal,depth);
		goto scale;
	}						
	
	// Check for constant current 
	if(GetNumTimesInFile()==1 && !(GetNumFiles()>1))
	{
		// Calculate the interpolated velocity at the point
		if (interpolationVal.ptIndex1 >= 0) 
		{
			scaledPatVelocity.u = interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,ptIndex1).u)
			+interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,ptIndex2).u)
			+interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,ptIndex3).u );
			scaledPatVelocity.v = interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,ptIndex1).v)
			+interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,ptIndex2).v)
			+interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,ptIndex3).v);
		}
		else	// if negative corresponds to negative ntri, set vel to zero
		{
			scaledPatVelocity.u = 0.;
			scaledPatVelocity.v = 0.;
		}
	}
	else // time varying current 
	{
		// Calculate the time weight factor
		if (GetNumFiles()>1 && fOverLap)
			startTime = fOverLapStartTime;
		else
			startTime = (*fTimeDataHdl)[fStartData.timeIndex].time;
		endTime = (*fTimeDataHdl)[fEndData.timeIndex].time;
		timeAlpha = (endTime - model_time)/(double)(endTime - startTime);
		
		// Calculate the interpolated velocity at the point
		if (interpolationVal.ptIndex1 >= 0) 
		{
			scaledPatVelocity.u = interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,ptIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,ptIndex1).u)
			+interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,ptIndex2).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,ptIndex2).u)
			+interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,ptIndex3).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,ptIndex3).u);
			scaledPatVelocity.v = interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,ptIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,ptIndex1).v)
			+interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,ptIndex2).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,ptIndex2).v)
			+interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,ptIndex3).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,ptIndex3).v);
		}
		else	// if negative corresponds to negative ntri, set vel to zero
		{
			scaledPatVelocity.u = 0.;
			scaledPatVelocity.v = 0.;
		}
	}
	
scale:
	
	scaledPatVelocity.u *= fVar.fileScaleFactor; 
	scaledPatVelocity.v *= fVar.fileScaleFactor; 
			
	return scaledPatVelocity;
}

VelocityRec TimeGridCurTri_c::GetScaledPatValue3D(const Seconds& model_time,InterpolationVal interpolationVal,float depth)
{
	// figure out which depth values the LE falls between
	// will have to interpolate in lat/long for both levels first
	// and some sort of check on the returned indices, what to do if one is below bottom?
	// for sigma model might have different depth values at each point
	// for multilayer they should be the same, so only one interpolation would be needed
	// others don't have different velocities at different depths so no interpolation is needed
	// in theory the surface case should be a subset of this case, may eventually combine
	
	long pt1depthIndex1, pt1depthIndex2, pt2depthIndex1, pt2depthIndex2, pt3depthIndex1, pt3depthIndex2;
	double topDepth, bottomDepth, depthAlpha, timeAlpha;
	VelocityRec pt1interp = {0.,0.}, pt2interp = {0.,0.}, pt3interp = {0.,0.};
	VelocityRec scaledPatVelocity = {0.,0.};
	Seconds startTime, endTime;
	
	GetDepthIndices(interpolationVal.ptIndex1,depth,&pt1depthIndex1,&pt1depthIndex2);	
	GetDepthIndices(interpolationVal.ptIndex2,depth,&pt2depthIndex1,&pt2depthIndex2);	
	GetDepthIndices(interpolationVal.ptIndex3,depth,&pt3depthIndex1,&pt3depthIndex2);	
	
 	// the contributions from each point will default to zero if the depth indicies
	// come back negative (ie the LE depth is out of bounds at the grid point)
	if(GetNumTimesInFile()==1 && !(GetNumFiles()>1))
	{
		if (pt1depthIndex1!=-1)
		{
			if (pt1depthIndex2!=-1) 
			{
				topDepth = INDEXH(fDepthsH,pt1depthIndex1);	
				bottomDepth = INDEXH(fDepthsH,pt1depthIndex2);
				depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
				pt1interp.u = depthAlpha*(interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,pt1depthIndex1).u))
				+ (1-depthAlpha)*(interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,pt1depthIndex2).u));
				pt1interp.v = depthAlpha*(interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,pt1depthIndex1).v))
				+ (1-depthAlpha)*(interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,pt1depthIndex2).v));
			}
			else
			{
				pt1interp.u = interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,pt1depthIndex1).u); 
				pt1interp.v = interpolationVal.alpha1*(INDEXH(fStartData.dataHdl,pt1depthIndex1).v); 
			}
		}
		
		if (pt2depthIndex1!=-1)
		{
			if (pt2depthIndex2!=-1) 
			{
				topDepth = INDEXH(fDepthsH,pt2depthIndex1);	
				bottomDepth = INDEXH(fDepthsH,pt2depthIndex2);
				depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
				pt2interp.u = depthAlpha*(interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,pt2depthIndex1).u))
				+ (1-depthAlpha)*(interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,pt2depthIndex2).u));
				pt2interp.v = depthAlpha*(interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,pt2depthIndex1).v))
				+ (1-depthAlpha)*(interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,pt2depthIndex2).v));
			}
			else
			{
				pt2interp.u = interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,pt2depthIndex1).u); 
				pt2interp.v = interpolationVal.alpha2*(INDEXH(fStartData.dataHdl,pt2depthIndex1).v);
			}
		}
		
		if (pt3depthIndex1!=-1) 
		{
			if (pt3depthIndex2!=-1) 
			{
				topDepth = INDEXH(fDepthsH,pt3depthIndex1);	
				bottomDepth = INDEXH(fDepthsH,pt3depthIndex2);
				depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
				pt3interp.u = depthAlpha*(interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,pt3depthIndex1).u))
				+ (1-depthAlpha)*(interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,pt3depthIndex2).u));
				pt3interp.v = depthAlpha*(interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,pt3depthIndex1).v))
				+ (1-depthAlpha)*(interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,pt3depthIndex2).v));
			}
			else
			{
				pt3interp.u = interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,pt3depthIndex1).u); 
				pt3interp.v = interpolationVal.alpha3*(INDEXH(fStartData.dataHdl,pt3depthIndex1).v); 
			}
		}
	}
	
	else // time varying current 
	{
		// Calculate the time weight factor
		if (GetNumFiles()>1 && fOverLap)
			startTime = fOverLapStartTime;
		else
			startTime = (*fTimeDataHdl)[fStartData.timeIndex].time;
		endTime = (*fTimeDataHdl)[fEndData.timeIndex].time;
		timeAlpha = (endTime - model_time)/(double)(endTime - startTime);
		
		if (pt1depthIndex1!=-1)
		{
			if (pt1depthIndex2!=-1) 
			{
				topDepth = INDEXH(fDepthsH,pt1depthIndex1);	
				bottomDepth = INDEXH(fDepthsH,pt1depthIndex2);
				depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
				pt1interp.u = depthAlpha*(interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,pt1depthIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt1depthIndex1).u))
				+ (1-depthAlpha)*(interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,pt1depthIndex2).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt1depthIndex2).u));
				pt1interp.v = depthAlpha*(interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,pt1depthIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt1depthIndex1).v))
				+ (1-depthAlpha)*(interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,pt1depthIndex2).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt1depthIndex2).v));
			}
			else
			{
				pt1interp.u = interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,pt1depthIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt1depthIndex1).u); 
				pt1interp.v = interpolationVal.alpha1*(timeAlpha*INDEXH(fStartData.dataHdl,pt1depthIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt1depthIndex1).v); 
			}
		}
		
		if (pt2depthIndex1!=-1)
		{
			if (pt2depthIndex2!=-1) 
			{
				topDepth = INDEXH(fDepthsH,pt2depthIndex1);	
				bottomDepth = INDEXH(fDepthsH,pt2depthIndex2);
				depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
				pt2interp.u = depthAlpha*(interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,pt2depthIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt2depthIndex1).u))
				+ (1-depthAlpha)*(interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,pt2depthIndex2).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt2depthIndex2).u));
				pt2interp.v = depthAlpha*(interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,pt2depthIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt2depthIndex1).v))
				+ (1-depthAlpha)*(interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,pt2depthIndex2).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt2depthIndex2).v));
			}
			else
			{
				pt2interp.u = interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,pt2depthIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt2depthIndex1).u); 
				pt2interp.v = interpolationVal.alpha2*(timeAlpha*INDEXH(fStartData.dataHdl,pt2depthIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt2depthIndex1).v); 
			}
		}
		
		if (pt3depthIndex1!=-1) 
		{
			if (pt3depthIndex2!=-1)
			{
				topDepth = INDEXH(fDepthsH,pt3depthIndex1);	
				bottomDepth = INDEXH(fDepthsH,pt3depthIndex2);
				depthAlpha = (bottomDepth - depth)/(double)(bottomDepth - topDepth);
				pt3interp.u = depthAlpha*(interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,pt3depthIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt3depthIndex1).u))
				+ (1-depthAlpha)*(interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,pt3depthIndex2).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt3depthIndex2).u));
				pt3interp.v = depthAlpha*(interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,pt3depthIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt3depthIndex1).v))
				+ (1-depthAlpha)*(interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,pt3depthIndex2).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt3depthIndex2).v));
			}
			else
			{
				pt3interp.u = interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,pt3depthIndex1).u + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt3depthIndex1).u); 
				pt3interp.v = interpolationVal.alpha3*(timeAlpha*INDEXH(fStartData.dataHdl,pt3depthIndex1).v + (1-timeAlpha)*INDEXH(fEndData.dataHdl,pt3depthIndex1).v); 
			}
		}
	}
	scaledPatVelocity.u = pt1interp.u + pt2interp.u + pt3interp.u;
	scaledPatVelocity.v = pt1interp.v + pt2interp.v + pt3interp.v;
	
	return scaledPatVelocity;
}

/////////////////////////////////////////////////////////////////
OSErr TimeGridCurTri_c::ReadHeaderLine(char *s)
{
	char msg[512],str[256];
	char gridType[24],boundary[24];
	char *strToMatch = 0;
	long len,numScanned,longVal;
	double val=0.;
	if(s[0] != '[')
		return -1; // programmer error
	
	switch(s[1]) {
		case 'C':
			strToMatch = "[CURSCALE]\t";
			len = strlen(strToMatch);
			if(!strncmp(s,strToMatch,len)) {
				numScanned = sscanf(s+len,lfFix("%lf"),&val);
				if (numScanned != 1 || val <= 0.0)
					goto BadValue; 
				//fVar.curScale = val;
				fVar.fileScaleFactor = val;
				return 0; // no error
			}
			break;
			
		case 'F':
			strToMatch = "[FILETYPE]";
			if(!strncmp(s,strToMatch,strlen(strToMatch))) {
				return 0; // no error, already dealt with this
			}
			break;
			
		case 'G':
			strToMatch = "[GRIDTYPE]\t";
			len = strlen(strToMatch);
			if(!strncmp(s,strToMatch,len)) {
				numScanned = sscanf(s+len,"%s",gridType);
				if (numScanned != 1)
					goto BadValue; 
				if (!strncmp(gridType,"2D",strlen("2D")))
					fVar.gridType = TWO_D;
				else
				{
					// code goes here, deal with bottom boundary condition
					if (!strncmp(gridType,"BAROTROPIC",strlen("BAROTROPIC")))
						fVar.gridType = BAROTROPIC;
					else if (!strncmp(gridType,"SIGMA",strlen("SIGMA")))
						fVar.gridType = SIGMA;
					else if (!strncmp(gridType,"MULTILAYER",strlen("MULTILAYER")))
						fVar.gridType = MULTILAYER;
					numScanned = sscanf(s+len+strlen(gridType),lfFix("%s%lf"),boundary,&val);
					if (numScanned < 1 || val < 0.)
						goto BadValue; 	
					// check on FREESLIP vs NOSLIP
					fVar2.bLayerThickness = val;
				}
				return 0; // no error
			}
			break;
			
		case 'N':
			strToMatch = "[NAME]\t";
			len = strlen(strToMatch);
			if(!strncmp(s,strToMatch,len)) {
				strncpy(fVar.userName,s+len,kPtCurUserNameLen);
				fVar.userName[kPtCurUserNameLen-1] = 0;
				return 0; // no error
			}
			break;
			
		case 'M':
			strToMatch = "[MAXNUMDEPTHS]\t";
			len = strlen(strToMatch);
			if(!strncmp(s,strToMatch,len)) {
				numScanned = sscanf(s+len,"%ld",&longVal);
				//if (numScanned != 1 || longVal <= 0.0)
				if (numScanned != 1 || longVal < 0.0)
					goto BadValue; 
				fVar.maxNumDepths = longVal;
				return 0; // no error
			}
			break;
			
		case 'U':
			///
			strToMatch = "[UNCERTALONG]\t";
			len = strlen(strToMatch);
			if(!strncmp(s,strToMatch,len)) {
				numScanned = sscanf(s+len,lfFix("%lf"),&val);
				if (numScanned != 1 || val <= 0.0)
					goto BadValue; 
				fVar2.alongCurUncertainty = val;
				return 0; // no error
			}
			///
			strToMatch = "[UNCERTCROSS]\t";
			len = strlen(strToMatch);
			if(!strncmp(s,strToMatch,len)) {
				numScanned = sscanf(s+len,lfFix("%lf"),&val);
				if (numScanned != 1 || val <= 0.0)
					goto BadValue; 
				fVar2.crossCurUncertainty = val;
				return 0; // no error
			}
			///
			strToMatch = "[UNCERTMIN]\t";
			len = strlen(strToMatch);
			if(!strncmp(s,strToMatch,len)) {
				numScanned = sscanf(s+len,lfFix("%lf"),&val);
				if (numScanned != 1 || val <= 0.0)
					goto BadValue; 
				//fVar.uncertMinimumInMPS = val;
				return 0; // no error
			}
			///
			strToMatch = "[USERDATA]";
			if(!strncmp(s,strToMatch,strlen(strToMatch))) {
				return 0; // no error, but nothing to do
			}
			break;
			
	}
	// if we get here, we did not recognize the string
	strncpy(str,s,255);
	strcpy(str+250,"..."); // cute trick
	sprintf(msg,"Unrecognized line:%s%s",NEWLINESTRING,str);
	printError(msg);
	
	return -1;
	
BadValue:
	strncpy(str,s,255);
	strcpy(str+250,"..."); // cute trick
	sprintf(msg,"Bad value:%s%s",NEWLINESTRING,str);
	printError(msg);
	return -1;
	
}

OSErr TimeGridCurTri_c::ReadPtCurVertices(CHARH fileBufH,long *line,LongPointHdl *pointsH,FLOATH *bathymetryH,char* errmsg,long numPoints)
// Note: '*line' must contain the line# at which the vertex data begins
{
	LongPointHdl ptsH = nil;
	FLOATH depthsH = 0, bathymetryHdl = 0;
	DepthDataInfoH depthDataInfo = 0;
	OSErr err=-1;
	char *s;
	long i,index = 0;
	
	strcpy(errmsg,""); // clear it
	*pointsH = 0;
	
	ptsH = (LongPointHdl)_NewHandle(sizeof(LongPoint)*(numPoints));
	if(ptsH == nil)
	{
		strcpy(errmsg,"Not enough memory to read PtCur file.");
		return -1;
	}
	
	bathymetryHdl = (FLOATH)_NewHandle(sizeof(float)*(numPoints));
	if(bathymetryHdl == nil)
	{
		strcpy(errmsg,"Not enough memory to read PtCur file.");
		return -1;
	}
	
	if (fVar.gridType != TWO_D) // have depth info
	{	
		depthsH = (FLOATH)_NewHandle(0);
		if(!depthsH) {TechError("PtCurMover::ReadPtCurVertices()", "_NewHandle()", 0); err = memFullErr; goto done;}
		
	}
	
	depthDataInfo = (DepthDataInfoH)_NewHandle(sizeof(**depthDataInfo)*numPoints);
	if(!depthDataInfo){TechError("PtCurMover::ReadPtCurVertices()", "_NewHandle()", 0); err = memFullErr; goto done;}
	
	s = new char[(fVar.maxNumDepths+4)*64]; // large enough to hold ptNum, vertex, total depth, and all depths
	if(!s) {TechError("PtCurMover::ReadPtCurVertices()", "new[]", 0); err = memFullErr; goto done;}
	
	for(i=0;i<numPoints;i++)
	{
		LongPoint vertex;
		NthLineInTextOptimized(*fileBufH, (*line)++, s, (fVar.maxNumDepths+4)*64); 
		
		char* token = strtok(s,PTCUR_DELIM_STR); // points to ptNum	 - skip over (maybe check...)
		token = strtok(NULL,PTCUR_DELIM_STR); // points to x
		
		err = ScanMatrixPt(token,&vertex);
		if(err)
		{
			char firstPartOfLine[128];
			sprintf(errmsg,"Unable to read vertex data from line %ld:%s",*line,NEWLINESTRING);
			strncpy(firstPartOfLine,s,120);
			strcpy(firstPartOfLine+120,"...");
			strcat(errmsg,firstPartOfLine);
			goto done;
		}
		
		// should be (*ptsH)[ptNum-1] or track the original indices 
		(*ptsH)[i].h = vertex.h;
		(*ptsH)[i].v = vertex.v;
		
		if (fVar.gridType != TWO_D) // have depth info
		{
			double depth;
			long numDepths = 0;
			(*depthDataInfo)[i].indexToDepthData = index;
			
			token = strtok(NULL,PTCUR_DELIM_STR); // points to y
			
			while (numDepths!=fVar.maxNumDepths+1)
			{
				token = strtok(NULL,PTCUR_DELIM_STR); // points to a depth
				err = ScanDepth(token,&depth);
				if(err)
				{
					char firstPartOfLine[128];
					sprintf(errmsg,"Unable to read depth data from line %ld:%s",*line,NEWLINESTRING);
					strncpy(firstPartOfLine,s,120);
					strcpy(firstPartOfLine+120,"...");
					strcat(errmsg,firstPartOfLine);
					goto done;
				}
				
				if (depth==-1) break; // no more depths
				if (numDepths==0) // first one is actual depth at the location
				{
					(*depthDataInfo)[i].totalDepth = depth;
					(*bathymetryHdl)[i] = depth;
				}
				else
				{
					// since we don't know the number of depths ahead of time
					_SetHandleSize((Handle) depthsH, (index+numDepths)*sizeof(**depthsH));
					if (_MemError()) { TechError("PtCurMover::ReadPtCurVertices()", "_SetHandleSize()", 0); goto done; }
					(*depthsH)[index+numDepths-1] = depth; 
				}
				numDepths++;
			}
			if (numDepths==1) // first one is actual depth at the location
			{
				(*depthDataInfo)[i].numDepths = numDepths;
				//(*depthDataInfo)[i].indexToDepthData = i;			
				index+=numDepths;
			}
			else
			{
				numDepths--; // don't count the actual depth
				(*depthDataInfo)[i].numDepths = numDepths;
				index+=numDepths;
			}
		}
		else // 2D, no depth info
		{
			(*depthDataInfo)[i].indexToDepthData = i;			
			(*depthDataInfo)[i].numDepths = 1;	// surface velocity only
			(*depthDataInfo)[i].totalDepth = -1;	// unknown
			(*bathymetryHdl)[i] = -1;	// don't we always have bathymetry?
		}
	}
	
	*pointsH = ptsH;
	fDepthsH = depthsH;
	fDepthDataInfo = depthDataInfo;
	*bathymetryH = bathymetryHdl;
	err = noErr;
	
	
done:
	
	if(s) {delete[] s;  s = 0;}
	if(err) 
	{
		if(ptsH) {DisposeHandle((Handle)ptsH); ptsH = 0;}
		if(depthsH) {DisposeHandle((Handle)depthsH); depthsH = 0;}
		if(depthDataInfo) {DisposeHandle((Handle)depthDataInfo); depthDataInfo = 0;}
		if(bathymetryHdl) {DisposeHandle((Handle)bathymetryHdl); bathymetryHdl = 0;}
	}
	return err;		
}

OSErr TimeGridCurTri_c::ReadTimeData(long index,VelocityFH *velocityH, char* errmsg) 
{
	char s[256], path[256]; 
	long i,j,line = 0;
	long offset,lengthToRead;
	CHARH h = 0;
	char *sectionOfFile = 0;
	char *strToMatch = 0;
	long len,numScanned;
	VelocityFH velH = 0;
	long totalNumberOfVels = 0;
	long numDepths = 1;
	
	LongPointHdl ptsHdl = 0;
	TTriGridVel* triGrid = dynamic_cast<TTriGridVel*> (fGrid); // don't think need 3D here
	
	OSErr err = 0;
	DateTimeRec time;
	Seconds timeSeconds;
	long numPoints; 
	errmsg[0]=0;
	
	strcpy(path,fVar.pathName);
	//strnzcpy (path, fVar.pathName, kMaxNameLen - 1);
	if (!path || !path[0]) return -1;
	
	lengthToRead = (*fTimeDataHdl)[index].lengthOfData;
	offset = (*fTimeDataHdl)[index].fileOffsetToStartOfData;
	
	ptsHdl = triGrid -> GetPointsHdl();
	if(ptsHdl)
		numPoints = _GetHandleSize((Handle)ptsHdl)/sizeof(**ptsHdl);
	else 
	{err=-1; goto done;} // no data
	
	
	h = (CHARH)_NewHandle(lengthToRead+1);
	if(!h){TechError("PtCurMover::ReadTimeData()", "_NewHandle()", 0); err = memFullErr; goto done;}
	
	_HLock((Handle)h);
	sectionOfFile = *h;			
	
	err = ReadSectionOfFile(0,0,path,offset,lengthToRead,sectionOfFile,0);
	if(err || !h) 
	{
		char firstPartOfLine[128];
		sprintf(errmsg,"Unable to open data file:%s",NEWLINESTRING);
		strncpy(firstPartOfLine,path,120);
		strcpy(firstPartOfLine+120,"...");
		strcat(errmsg,firstPartOfLine);
		goto done;
	}
	sectionOfFile[lengthToRead] = 0; // make it a C string
	
	totalNumberOfVels = (*fDepthDataInfo)[numPoints-1].indexToDepthData+(*fDepthDataInfo)[numPoints-1].numDepths;
	if(totalNumberOfVels<numPoints) {err=-1; goto done;} // must have at least full set of 2D velocity data
	velH = (VelocityFH)_NewHandle(sizeof(**velH)*totalNumberOfVels);
	if(!velH){TechError("PtCurMover::ReadTimeData()", "_NewHandle()", 0); err = memFullErr; goto done;}
	
	strToMatch = "[TIME]";
	len = strlen(strToMatch);
	NthLineInTextOptimized (sectionOfFile, line = 0, s, 256);
	if(!strncmp(s,strToMatch,len)) 
	{
		numScanned=sscanf(s+len, "%hd %hd %hd %hd %hd",
						  &time.day, &time.month, &time.year,
						  &time.hour, &time.minute) ;
		if (numScanned!= 5)
		{ err = -1; TechError("PtCurMover::ReadTimeData()", "sscanf() == 5", 0); goto done; }
		// check for constant current
		if (time.day == -1 && time.month == -1 && time.year == -1 && time.hour == -1 && time.minute == -1)
			//if (time.year == time.month == time.day == time.hour == time.minute == -1) 
		{
			timeSeconds = CONSTANTCURRENT;
		}
		else // time varying current
		{
			if (time.year < 1900)					// two digit date, so fix it
			{
				if (time.year >= 40 && time.year <= 99)	
					time.year += 1900;
				else
					time.year += 2000;					// correct for year 2000 (00 to 40)
			}
			
			time.second = 0;
			DateToSeconds (&time, &timeSeconds);
		}
		
		// check time is correct
		if (timeSeconds!=(*fTimeDataHdl)[index].time)
		{ err = -1;  strcpy(errmsg,"Can't read data - times in the file have changed."); goto done; }
		line++;
	}
	
	
	for(i=0;i<fVar2.numLandPts;i++)	// zero out boundary velocity
	{
		numDepths = (*fDepthDataInfo)[i].numDepths;
		for(j=0;j<numDepths;j++) 
		{
			(*velH)[(*fDepthDataInfo)[i].indexToDepthData+j].u = 0.0;
			(*velH)[(*fDepthDataInfo)[i].indexToDepthData+j].v = 0.0;
		}
	}
	
	for(i=fVar2.numLandPts;i<numPoints;i++) // interior points
	{
		VelocityRec vel;
		char *startScan;
		long scanLength,stringIndex=0;
		numDepths = (*fDepthDataInfo)[i].numDepths;
		
		char *s1 = new char[numDepths*64];
		if(!s1) {TechError("PtCurMover::ReadTimeData()", "new[]", 0); err = memFullErr; goto done;}
		
		NthLineInTextOptimized (sectionOfFile, line, s1, numDepths*64);
		startScan = &s1[stringIndex];
		
		for(j=0;j<numDepths;j++) 
		{
			err = ScanVelocity(startScan,&vel,&scanLength); 
			// ScanVelocity is faster than scanf, but doesn't handle scientific notation. Try a scanf on error.
			if (err)
			{
				if(sscanf(&s1[stringIndex],lfFix("%lf%lf"),&vel.u,&vel.v) < 2)
				{
					char firstPartOfLine[128];
					sprintf(errmsg,"Unable to read velocity data from line %ld:%s",line,NEWLINESTRING);
					strncpy(firstPartOfLine,s1,120);
					strcpy(firstPartOfLine+120,"...");
					strcat(errmsg,firstPartOfLine);
					delete[] s1; s1=0;
					goto done;
				}
				err = 0;
			}
			(*velH)[(*fDepthDataInfo)[i].indexToDepthData+j].u = vel.u; 
			(*velH)[(*fDepthDataInfo)[i].indexToDepthData+j].v = vel.v; 
			stringIndex += scanLength;
			startScan = &s1[stringIndex];
		}
		line++;
		delete[] s1; s1=0;
	}
	*velocityH = velH;
	
done:
	
	if(h) {
		_HUnlock((Handle)h); 
		DisposeHandle((Handle)h); 
		h = 0;
	}
	
	
	if(err)
	{
		if(!errmsg[0])
			strcpy(errmsg,"An error occurred in PtCurMover::ReadTimeData");
		//printError(errmsg); // This alert causes a freeze up...
		// We don't want to put up an error message here because it can lead to an infinite loop of messages.
		if(velH) {DisposeHandle((Handle)velH); velH = 0;}
	}
	return err;
	
}

OSErr TimeGridCurTri_c::TextRead(char *path, char *topFilePath) 
{
	char s[1024], errmsg[256], classicPath[256];
	long i, numPoints, numTopoPoints, line = 0;
	CHARH f = 0;
	OSErr err = 0;
	
	TopologyHdl topo=0;
	LongPointHdl pts=0;
	FLOATH bathymetryH = 0;
	VelocityFH velH = 0;
	DAGTreeStruct tree;
	WorldRect bounds;
	
	TTriGridVel *triGrid = nil;
	tree.treeHdl = 0;
	TDagTree *dagTree = 0;
	
	long numWaterBoundaries, numBoundaryPts, numBoundarySegs;
	LONGH boundarySegs=0, waterBoundaries=0;
	Boolean haveBoundaryData = false;
	
	errmsg[0]=0;
	
	
	if (!path || !path[0]) return 0;
	
	strcpy(fVar.pathName,path);
	
	// code goes here, we need to worry about really big files
	
	if (err = ReadFileContents(TERMINATED,0, 0, path, 0, 0, &f)) {
		TechError("PtCurMover::TextRead()", "ReadFileContents()", err);
		goto done;
	}
	
	_HLock((Handle)f); // JLM 8/4/99
	
	// code goes here, worry about really long lines in the file
	
	// read header here
	for (i = 0 ; TRUE ; i++) {
		NthLineInTextOptimized(*f, line++, s, 1024); 
		if(s[0] != '[')
			break;
		err = this -> ReadHeaderLine(s);
		if(err)
			goto done;
	}
	
	// option to read in exported topology or just require cut and paste into file	
	// read triangle/topology info if included in file, otherwise calculate
	
	if(IsPtCurVerticesHeaderLine(s,&numPoints,&fVar2.numLandPts))	// Points in Galt format
	{
		MySpinCursor();
		err = ReadPtCurVertices(f,&line,&pts,&bathymetryH,errmsg,numPoints);
		if(err) goto done;
	}
	else
	{
		err = -1; 
		printError("Unable to read PtCur Triangle Velocity file."); 
		goto done;
	}
	
	// figure out the bounds
	bounds = voidWorldRect;
	long numPts;
	if(pts) 
	{
		LongPoint	thisLPoint;
		
		numPts = _GetHandleSize((Handle)pts)/sizeof(LongPoint);
		if(numPts > 0)
		{
			WorldPoint  wp;
			for(i=0;i<numPts;i++)
			{
				thisLPoint = INDEXH(pts,i);
				wp.pLat = thisLPoint.v;
				wp.pLong = thisLPoint.h;
				AddWPointToWRect(wp.pLat, wp.pLong, &bounds);
			}
		}
	}
	
	MySpinCursor();
	NthLineInTextOptimized(*f, (line)++, s, 1024); 
	
	if(IsBoundarySegmentHeaderLine(s,&numBoundarySegs)) // Boundary data from CATs
	{
		MySpinCursor();
		err = ReadBoundarySegs(f,&line,&boundarySegs,numBoundarySegs,errmsg);
		if(err) goto done;
		NthLineInTextOptimized(*f, (line)++, s, 1024); 
		haveBoundaryData = true;
	}
	else
	{
		haveBoundaryData = false;
		// not needed for 2D files, unless there is no topo - store a flag
	}
	MySpinCursor(); // JLM 8/4/99
	
	if(IsWaterBoundaryHeaderLine(s,&numWaterBoundaries,&numBoundaryPts)) // Boundary types from CATs
	{
		MySpinCursor();
		err = ReadWaterBoundaries(f,&line,&waterBoundaries,numWaterBoundaries,numBoundaryPts,errmsg);
		if(err) goto done;
		NthLineInTextOptimized(*f, (line)++, s, 1024); 
	}
	else
	{
		// not needed for 2D files
	}
	MySpinCursor(); // JLM 8/4/99
	//NthLineInTextOptimized(*f, (line)++, s, 1024); 
	
	if(IsTTopologyHeaderLine(s,&numTopoPoints)) // Topology from CATs
	{
		MySpinCursor();
		err = ReadTTopologyBody(f,&line,&topo,&velH,errmsg,numTopoPoints,FALSE);
		if(err) goto done;
		NthLineInTextOptimized(*f, (line)++, s, 1024); 
	}
	else
	{
		if (!haveBoundaryData) {err=-1; strcpy(errmsg,"File must have boundary data to create topology"); goto done;}
		//DisplayMessage("NEXTMESSAGETEMP");
		DisplayMessage("Making Triangles\n");
		
		if (err = maketriangles(&topo,pts,numPoints,boundarySegs,numBoundarySegs))  // use maketriangles.cpp
			err = -1; // for now we require TTopology
		// code goes here, support Galt style ??
		//DisplayMessage(0);
		DisplayMessage("\n");
		if(err) goto done;
	}
	MySpinCursor(); // JLM 8/4/99
	
	//NthLineInTextOptimized(*f, (line)++, s, 1024); 
	
	if(IsTIndexedDagTreeHeaderLine(s,&numPoints))  // DagTree from CATs
	{
		MySpinCursor();
		err = ReadTIndexedDagTreeBody(f,&line,&tree,errmsg,numPoints);
		if(err) goto done;
	}
	else
	{
		//DisplayMessage("NEXTMESSAGETEMP");
		DisplayMessage("Making Dag Tree\n");
		tree = MakeDagTree(topo, (LongPoint**)pts, errmsg); // use CATSDagTree.cpp and my_build_list.h
		//DisplayMessage(0);
		DisplayMessage("\n");
		if (errmsg[0])	
			err = -1; // for now we require TIndexedDagTree
		// code goes here, support Galt style ??
		if(err) goto done;
	}
	MySpinCursor(); // JLM 8/4/99
	
	/////////////////////////////////////////////////
	// if the boundary information is in the file we'll need to create a bathymetry map (required for 3D)
	
	// code goes here, do we want to store the grid boundaries and land/water information?
	/*if (waterBoundaries && (this -> moverMap == model -> uMap || fVar.gridType != TWO_D))
	{
		//PtCurMap *map = CreateAndInitPtCurMap(fVar.userName,bounds); // the map bounds are the same as the grid bounds
		PtCurMap *map = CreateAndInitPtCurMap(fVar.pathName,bounds); // the map bounds are the same as the grid bounds
		if (!map) goto done;
		// maybe move up and have the map read in the boundary information
		map->SetBoundarySegs(boundarySegs);	
		map->SetWaterBoundaries(waterBoundaries);
		
		*newMap = map;
	}
	else*/
	{
		if (boundarySegs){DisposeHandle((Handle)boundarySegs); boundarySegs=0;}
		if (waterBoundaries){DisposeHandle((Handle)waterBoundaries); waterBoundaries=0;}
	}
	
	/////////////////////////////////////////////////	
	
	triGrid = new TTriGridVel;
	if (!triGrid)
	{		
		err = true;
		TechError("Error in TimeGridCurTri_c::TextRead()","new TTriGridVel" ,err);
		goto done;
	}
	
	fGrid = (TGridVel*)triGrid;
	
	triGrid -> SetBounds(bounds); 
	this->SetGridBounds(bounds);
	
	dagTree = new TDagTree(pts,topo,tree.treeHdl,velH,tree.numBranches); 
	if(!dagTree)
	{
		printError("Unable to read Triangle Velocity file.");
		goto done;
	}
	
	triGrid -> SetDagTree(dagTree);
	//if (fDepthsH) triGrid->SetBathymetry(fDepthsH);	// maybe set both?
	if (bathymetryH) triGrid->SetDepths(bathymetryH);	// want just the bottom depths not all levels, so not fDepthsH
	
	pts = 0;	// because fGrid is now responsible for it
	topo = 0; // because fGrid is now responsible for it
	tree.treeHdl = 0; // because fGrid is now responsible for it
	velH = 0; // because fGrid is now responsible for it
	bathymetryH = 0; // because fGrid is now responsible for it
	
	
	// scan through the file looking for "[TIME ", then read and record the time, filePosition, and length of data
	// consider the possibility of multiple files
	NthLineInTextOptimized(*f, (line)++, s, 1024); 
	if(!strstr(s,"[FILE]")) 
	{	// single file
		//err = ScanFileForTimes(path,&fTimeDataHdl,true);	
		err = ScanFileForTimes(path,&fTimeDataHdl,&fTimeHdl);	
		
		if (err) goto done;
	}
	else
	{	// multiple files
		long numLinesInText = NumLinesInText(*f);
		long numFiles = (numLinesInText - (line - 1))/3;	// 3 lines for each file - filename, starttime, endtime
		strcpy(fVar.pathName,s+strlen("[FILE]\t"));
		ResolvePathFromInputFile(path,fVar.pathName); // JLM 6/8/10
		//err = ScanFileForTimes(fVar.pathName,&fTimeDataHdl,true);	
		err = ScanFileForTimes(fVar.pathName,&fTimeDataHdl,&fTimeHdl);	
		
		if (err) goto done;
		// code goes here, maybe do something different if constant current
		line--;
		err = ReadInputFileNames(f,&line,numFiles,&fInputFilesHdl,path);
	}
	//err = ScanFileForTimes(path,&fTimeDataHdl);
	//if (err) goto done;
	
	
	
done:
	
	if(f) 
	{
		_HUnlock((Handle)f); 
		DisposeHandle((Handle)f); 
		f = 0;
	}
	
	if(err)
	{
		if(!errmsg[0])
			strcpy(errmsg,"An error occurred in PtCurMover::TextRead");
		printError(errmsg); 
		if(pts) {DisposeHandle((Handle)pts); pts=0;}
		if(topo) {DisposeHandle((Handle)topo); topo=0;}
		if(velH) {DisposeHandle((Handle)velH); velH=0;}
		if(bathymetryH) {DisposeHandle((Handle)bathymetryH); bathymetryH=0;}
		if(tree.treeHdl) {DisposeHandle((Handle)tree.treeHdl); tree.treeHdl=0;}
		if(fGrid)
		{
			fGrid ->Dispose();
			delete fGrid;
			fGrid = 0;
		}
		/*if (*newMap) 
		{
			(*newMap)->Dispose();
			delete *newMap;
			*newMap=0;
		}*/
		if (boundarySegs){DisposeHandle((Handle)boundarySegs); boundarySegs=0;}
		if (waterBoundaries){DisposeHandle((Handle)waterBoundaries); waterBoundaries=0;}
	}
	return err;
	
	// rest of file (i.e. velocity data) is read as needed
}

//#endif


import time
import calendar
import re

class gwtm:

    """ yyyy-mm-dd hh:mm"""
                    
    reference_time = '1970-01-01 00:00:00'
    pattern = re.compile('^(\d\d\d\d)\-(\d\d)\-(\d\d)\s(\d\d):(\d\d):(\d\d)$')

    def __init__(self, time_string, epoch = reference_time):
        tokens = self.parse_datetime(time_string)
        self.date_time = {'year': tokens.group(1), 'month': tokens.group(2), \
                                    'day': tokens.group(3), 'hour': tokens.group(4), \
                                    'minute': tokens.group(5), 'second': tokens.group(6)  }
        self.__set_epoch(epoch)
        self.__to_seconds()
        
    def __del__(self):
        pass
    
    def __set_epoch(self, epoch):
        self.epoch_date_time = epoch
        if(epoch != self.reference_time):
            self.epoch_seconds = gwtm(epoch).time_seconds
        else:
            self.epoch_seconds = 0
            
    def __to_seconds(self):
        dt = self.date_time
        time_str = dt['year'] + ' ' + dt['month'] + ' ' + dt['day'] + ' ' + dt['hour'] + ' ' + dt['minute'] + ' ' + dt['second']
        self.struct_time = time.strptime(time_str, "%Y %m %d %H %M %S")
        self.time_seconds = calendar.timegm(self.struct_time)
        self.time_seconds += -self.epoch_seconds
        
    def parse_datetime(self, datetime_string):
        mtch = self.pattern.match(datetime_string)
        if not mtch:
            raise Exception
        else:
            return mtch

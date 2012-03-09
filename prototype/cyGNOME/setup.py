from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext
import os, fnmatch
import sysconfig
import numpy as np
 
#for root, dirs, _files in os.walk("codeFiles/"):
#    for file in _files:
#        if fnmatch.fnmatch(file, "*.cpp") or fnmatch.fnmatch(file, "*.CPP"):
#            files += [os.path.join(root, file.lower())]

files = ['MemUtils/MemUtils.cpp', 'Mover/Mover_c.cpp']
files += ['Random/Random_c.cpp', 'WindMover/WindMover_c.cpp']
files += ['CompFunctions.cpp', 'CMyList/CMYLIST.cpp']
files += ['OSSMTimeValue/OSSMTimeValue_c.cpp', 'TimeValue/TimeValue_c.cpp']
files += ['GEOMETRY.cpp']

tempList = ['c_gnome.pyx']
for file in files:
    tempList += ["codeFiles/"+file]
files = tempList

compile_args=["-I.", "-fpascal-strings", "-fasm-blocks"]

if sysconfig.get_config_var('UNIVERSALSDK') != None:
    pass
else:
    compile_args += ['-isysroot /Developer/SDKs/MacOSX10.4u.sdk']

setup(
    cmdclass = {'build_ext': build_ext},
    ext_modules = [Extension("c_gnome",
                             files,
                             language="c++",
                             extra_compile_args = compile_args,
                             include_dirs=["codeFiles",
                                           np.get_include(),
                                           ".",
                                           "/Developer/SDKs/MacOSX10.4u.sdk/Developer/Headers/FlatCarbon",
                                           ],
                             )
                   ]
    )


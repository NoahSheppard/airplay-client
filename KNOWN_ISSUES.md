# Environment Issues
## "cannot find cc1plus"

Remake the build folder with this command
`cmake .. -DTARGET_PLATFORM=desktop -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13`
from the project root directory

(Please note, that when make-ing for the first time, the ALAC library will spit warnings out at you, those can be safely ignored.)
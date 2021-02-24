@echo off
echo HAVE YOU UPDATED version.cpp ???
pause


call newsrc.bat
call clean.bat
call clean.bat
call clean.bat
call clean.bat

rem --- 64bit ---
del mameui.exe
del mameui.sym
call make64 -j4 %1 %2 %3

:end

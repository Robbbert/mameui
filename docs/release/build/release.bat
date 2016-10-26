@echo off
echo HAVE YOU UPDATED version.cpp ???
pause


call newsrc.bat
call clean.bat
call clean.bat
call clean.bat
call clean.bat

rem --- 32bit ---
del mameui.exe
call make32 -j4 %1 %2 %3
if not exist mameui.exe goto end

rem --- 64bit ---
del mameui64.exe
call make64 -j4 %1 %2 %3

:end

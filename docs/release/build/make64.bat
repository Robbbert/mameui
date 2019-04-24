@echo off
set MINGW64=E:\Mingw\8-3-0\mingw64
set minpath=%MINGW64%\bin
set oldpath=%Path%
set Path=%minpath%;%oldpath%
echo.|time
%MINGW64%\bin\make PTR64=1 OSD=messui SYMBOLS=0 NO_SYMBOLS=1 %1 %2 %3 %4
echo.|time
set Path=%oldpath%
set oldpath=
if exist mameui64.exe %minpath%\strip -s mameui64.exe
set minpath=


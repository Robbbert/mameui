@echo off
set MINGW64=E:\Mingw\10-1-0\mingw64
set minpath=%MINGW64%\bin
set oldpath=%Path%
set Path=%minpath%;%oldpath%
touch src\version.cpp
echo.|time
%MINGW64%\bin\make PTR64=1 OSD=messui SYMBOLS=0 NO_SYMBOLS=1 %1 %2 %3 %4
echo.|time
set Path=%oldpath%
set oldpath=
if exist mameui.exe %minpath%\strip -s mameui.exe
set minpath=


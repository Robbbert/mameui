del mame.sym
:start
del mame.exe
if exist mame.exe goto start
@echo off
set MINGW32=E:\Mingw\5-3-0\mingw32
set minpath=%MINGW32%\bin
set oldpath=%Path%
set Path=%minpath%;%oldpath%
%MINGW32%\bin\make PTR64=0 SYMBOLS=0 NO_SYMBOLS=1 -j4 %1 %2 %3 %4
set Path=%oldpath%
set oldpath=
if exist mame.exe %minpath%\strip -s mame.exe
set minpath=


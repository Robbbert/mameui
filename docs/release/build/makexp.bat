@echo off
set MINGW32=E:\Mingw\5-3-0\mingw32
set minpath=%MINGW32%\bin
set oldpath=%Path%
set Path=%minpath%;%oldpath%
%MINGW32%\bin\make PTR64=0 SYMBOLS=0 NO_SYMBOLS=1 %1 %2 %3 %4
set Path=%oldpath%
set oldpath=
if exist mameui.exe %minpath%\strip -s mameui.exe
set minpath=


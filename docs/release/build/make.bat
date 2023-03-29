@if exist scripts\minimaws\minimaws.sqlite3 del scripts\minimaws\minimaws.sqlite3
@echo off
del build\generated\resource\mamevers.rc
del mameui.sym
:start
del mameui.exe
if exist mameui.exe goto start
rem call mk.bat
call make64 -j6 %1 %2 %3
if not exist mameui.exe goto end
rem del mameui.sym
:end



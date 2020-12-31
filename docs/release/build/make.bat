@if exist scripts\minimaws\minimaws.sqlite3 del scripts\minimaws\minimaws.sqlite3
@echo off
del build\generated\resource\mamevers.rc
:start
del mameui64.exe
del mameui64.sym
if exist mameui64.exe goto start
call make64 -j6 %1 %2 %3
copy /Y mameui64.exe mameui.exe


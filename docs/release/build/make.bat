del mameui64.sym
:start
del mameui64.exe
if exist mameui64.exe goto start
make64 -j6 %1 %2 %3
copy /Y mameui64.exe mameui.exe


del mameui.sym
:start
del mameui.exe
if exist mameui.exe goto start
make32 -j4 %1 %2 %3

call newsrc.bat
call clean.bat
call clean.bat
call clean.bat
call clean.bat

rem --- 32bit ---
del mameui.exe
call make32 -j4 %1 %2 %3

:end

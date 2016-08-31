set from=c:\MAMEUI
set to=c:\MAMEUI\docs\release

rd %to%\src /q /s

md %to%\src\emu
copy /Y %from%\src\emu\video.* %to%\src\emu

md %to%\src\frontend\mame
copy /Y %from%\src\frontend\mame\audit.* %to%\src\frontend\mame
copy /Y %from%\src\version.cpp %to%\src

md %to%\src\osd\winui
xcopy /E /Y %from%\src\osd\winui %to%\src\osd\winui

rem now save all our stuff to github
copy %from%\*.bat %to%\build
xcopy /E /Y %from%\scripts %to%\scripts

rem convert all the unix documents to windows format for notepad
type %from%\docs\BSD3Clause.txt    | MORE /P > %to%\docs\BSD3Clause.txt
type %from%\docs\LICENSE           | MORE /P > %to%\docs\license.txt
type %from%\docs\winui_license.txt | MORE /P > %to%\docs\winui_license.txt

pause
echo off
cls
echo.
echo RAR up everything.
echo.

pause

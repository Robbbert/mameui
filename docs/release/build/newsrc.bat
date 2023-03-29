set from=c:\MAMEUI
set to=c:\MAMEUI\docs\release

rd %to%\src /q /s
rd %to%\scripts /q /s
rd %to%\build /q /s

md %to%\src\emu
copy %from%\src\emu\diimage.cpp                     %to%\src\emu
copy %from%\src\emu\emuopts.cpp                     %to%\src\emu
copy %from%\src\emu\gamedrv.h                       %to%\src\emu
copy %from%\src\emu\inpttype.ipp                    %to%\src\emu
copy %from%\src\emu\ioport.*                        %to%\src\emu
copy %from%\src\emu\mconfig.cpp                     %to%\src\emu
copy %from%\src\emu\softlist.cpp                    %to%\src\emu
copy %from%\src\emu\softlist_dev.cpp                %to%\src\emu
copy %from%\src\emu\video.*                         %to%\src\emu
copy %from%\src\version.cpp                         %to%\src
copy    %from%\src\makefile                         %to%

md %to%\src\devices\bus\nes
copy %from%\src\devices\bus\nes\nes_ines.hxx        %to%\src\devices\bus\nes
copy %from%\src\devices\bus\nes\nes_slot.cpp        %to%\src\devices\bus\nes

md %to%\src\devices\bus\snes
copy %from%\src\devices\bus\snes\nes_slot.*         %to%\src\devices\bus\snes

md %to%\src\devices\imagedev
copy %from%\src\devices\imagedev\floppy.cpp         %to%\src\devices\imagedev

md %to%\src\frontend\mame\ui
copy %from%\src\frontend\mame\mameopts.*            %to%\src\frontend\mame
copy %from%\src\frontend\mame\audit.*               %to%\src\frontend\mame
copy %from%\src\frontend\mame\mame.cpp              %to%\src\frontend\mame
copy %from%\src\frontend\mame\ui\about.cpp          %to%\src\frontend\mame\ui
copy %from%\src\frontend\mame\ui\ui.cpp             %to%\src\frontend\mame\ui

md %to%\src\lib\util
copy %from%\src\lib\util\options.*                  %to%\src\lib\util
copy %from%\src\lib\util\chdcd.cpp                  %to%\src\lib\util

md %to%\src\mame\nintendo
copy %from%\src\mame\nintendo\snes.cpp              %to%\src\mame\nintendo

md %to%\src\osd\modules\lib
md %to%\src\osd\modules\render
copy %from%\src\osd\modules\lib\osdobj_common.cpp   %to%\src\osd\modules\lib
copy %from%\src\osd\modules\render\drawd3d.cpp      %to%\src\osd\modules\render

md %to%\src\osd\windows
copy %from%\src\osd\windows\winmain.*               %to%\src\osd\windows
copy %from%\src\osd\windows\winopts.*               %to%\src\osd\windows
copy %from%\src\osd\windows\window.*                %to%\src\osd\windows

md %to%\src\osd\winui
xcopy /E /Y %from%\src\osd\winui                    %to%\src\osd\winui

rem now save all our stuff to github
md %to%\build
copy %from%\*.bat                                   %to%\build

md %to%\scripts
xcopy /T %from%\scripts                             %to%\scripts
copy %from%\scripts\build\verinfo.py                %to%\scripts\build
copy %from%\scripts\src\main.lua                    %to%\scripts\src
copy %from%\scripts\src\osd\winui.lua               %to%\scripts\src\osd
copy %from%\scripts\src\osd\winui_cfg.lua           %to%\scripts\src\osd
copy %from%\scripts\src\osd\messui.lua              %to%\scripts\src\osd
copy %from%\scripts\src\osd\newui.lua               %to%\scripts\src\osd
copy %from%\scripts\src\mame\frontend.lua           %to%\scripts\src\mame
copy %from%\scripts\src\osd\modules.lua             %to%\scripts\src\osd
copy %from%\scripts\src\3rdparty.lua                %to%\scripts\src
copy %from%\scripts\genie.lua                       %to%\scripts

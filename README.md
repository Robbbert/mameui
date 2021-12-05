What is MAMEUI?
===============
MAMEUI has an inbuilt front-end allowing you to run a game from a list. It requires Windows 7 or later.

<<<<<<< HEAD
=======
# **MAME** #

[![Join the chat at https://gitter.im/mamedev/mame](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/mamedev/mame?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Build status:

| OS/Compiler   | Status        | 
| ------------- |:-------------:| 
| Linux/GCC and clang | ![CI (Linux)](https://github.com/mamedev/mame/workflows/CI%20(Linux)/badge.svg) |
| Windows/MinGW GCC | ![CI (Windows)](https://github.com/mamedev/mame/workflows/CI%20(Windows)/badge.svg) |
| macOS/clang | ![CI (macOS)](https://github.com/mamedev/mame/workflows/CI%20(macOS)/badge.svg) |
| UI Translations | ![Compile UI translations](https://github.com/mamedev/mame/workflows/Compile%20UI%20translations/badge.svg) |
| Documentation | ![Build documentation](https://github.com/mamedev/mame/workflows/Build%20documentation/badge.svg) |

Static analysis status for entire build (except for third-party parts of project):

[![Coverity Scan Status](https://scan.coverity.com/projects/5727/badge.svg?flat=1)](https://scan.coverity.com/projects/mame-emulator)

What is MAME?
=============

MAME is a multi-purpose emulation framework.

MAME's purpose is to preserve decades of software history. As electronic technology continues to rush forward, MAME prevents this important "vintage" software from being lost and forgotten. This is achieved by documenting the hardware and how it functions. The source code to MAME serves as this documentation. The fact that the software is usable serves primarily to validate the accuracy of the documentation (how else can you prove that you have recreated the hardware faithfully?). Over time, MAME (originally stood for Multiple Arcade Machine Emulator) absorbed the sister-project MESS (Multi Emulator Super System), so MAME now documents a wide variety of (mostly vintage) computers, video game consoles and calculators, in addition to the arcade video games that were its initial focus.
>>>>>>> upstream/master

How to compile?
===============

<<<<<<< HEAD
You can only build MAMEUI on a Windows computer. It won't compile on Unix.
=======
If you're on a UNIX-like system (including Linux and macOS), it could be as easy as typing
>>>>>>> upstream/master

```
make OSD=messui
```

<<<<<<< HEAD
=======
for a MAME build,

```
make SUBTARGET=arcade
```

for an arcade-only build, or

```
make SUBTARGET=mess
```

for a MESS build.

See the [Compiling MAME](http://docs.mamedev.org/initialsetup/compilingmame.html) page on our documentation site for more information, including prerequisites for macOS and popular Linux distributions.

For recent versions of macOS you need to install [Xcode](https://developer.apple.com/xcode/) including command-line tools and [SDL 2.0](https://www.libsdl.org/download-2.0.php).

For Windows users, we provide a ready-made [build environment](http://mamedev.org/tools/) based on MinGW-w64.

Visual Studio builds are also possible, but you still need [build environment](http://mamedev.org/tools/) based on MinGW-w64.
In order to generate solution and project files just run:

```
make vs2019
```
or use this command to build it directly using msbuild

```
make vs2019 MSBUILD=1
```
>>>>>>> upstream/master


Where can I find out more?
==========================

* [Official MAME Development Team Site](http://mamedev.org/) (includes binary downloads for MAME and MESS, wiki, forums, and more)
* [MAMEUI site] http://www.mameui.info/
* [MAMEUI forum] http://www.mameworld.info/ubbthreads/postlist.php?Cat=&Board=mameui


Licensing Information
=====================

Information about the MAME content can be found at https://github.com/mamedev/mame/blob/master/README.md

Information about the MAME license can be found at https://github.com/mamedev/mame/blob/master/COPYING

Information about the WINUI portion can be found at https://github.com/Robbbert/mameui/blob/master/docs/winui_license.txt


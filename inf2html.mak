#
# Compiler macro. This is used for each .OBJ file to be created.
# "\" is the line-continuation mark.
#

CC      = icc /c /gd- /o- /ol- /q+ /se /ss

# /oi- /w2 /fi+
#/ti+

# Some VisualAge C++ compiler options explained:
# /c:   compile only, no link
# /fi+: precompile header files
# /gd-: link runtime statically
# /ge-: create DLL
# /gi+: fast integer execution
# /gm+: multithread libraries
# /kc+: produce preprocessor warnings
# /o+:  optimization (inlining etc.)
# /oi-: no inlining (?)
# /ol+: use intermediate linker
# /q+:  suppress icc logo
# /se:  all language extensions
# /si+: allow use of precompiled header files
# /ss:  allow double slashes
# /ti+: debug code
# /Wcnd: conditional exprs problems (= / == etc.)
# /Wgen: generic debugging msgs
# /Wcmp: possible unsigned comparison redundancies
# /Wcns: operations involving constants
# /Wpar: list not-referenced parameters (annoying)
# /Wppc: list possible preprocessor problems (.h dependencies)
# /Wpro: warn if funcs have not been prototyped
# /Wrea: mark code that cannot be reached
# /Wret: check consistency of return levels
# /w2:   produce error and warning messages, but no infos

.c.obj:
        @ echo Compiling $*.c:
        $(CC) /fi"prech\$*.pch" /si"prech\$*.pch" -I$(INCLUDE) $*.c

all: ..\inf2html.exe

inf2html.obj: inf2html.c inf2html.h inf.h inf2html.mak

..\inf2html.exe: inf2html.obj inf2html.mak
        ilink /OUT:..\inf2html.exe /PMTYPE:VIO inf2html.obj



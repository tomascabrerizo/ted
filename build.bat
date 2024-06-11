@echo off

if not exist .\build mkdir .\build

echo ----------------------------------------
echo Build game ...
echo ----------------------------------------

set TARGET=ted
set CFLAGS=/std:c11 /W2 /nologo /Od /Zi /EHsc
set LIBS=
set SOURCES=src\main.c
set OUT_DIR=/Fo.\build\ /Fe.\build\%TARGET% /Fm.\build\
set INC_DIR=/I.\src
set LNK_DIR=

cl %CFLAGS% %INC_DIR% %SOURCES% %OUT_DIR% /link %LNK_DIR% %LIBS% /SUBSYSTEM:CONSOLE

@echo off

SET WATCOM=C:\devtools\watcom
SET EDPATH=%WATCOM%\EDDAT
SET INCLUDE=%WATCOM%\H
REM SET LIB=
REM SET WWINHELP=D:\BINW

set OLDPATH=%PATH%
SET PATH=%WATCOM%\BINW;%PATH%
set CFLAGS=-w4 -e23 -za -zq -os -ol -oc -d2 -bt=dos -fo=.obj.ml

wmake IMF2MID.MK

PATH=%OLDPATH%


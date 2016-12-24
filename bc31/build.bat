@echo off
set BCPATHR=C:\devtools\bc31
set BCPATH=C:/devtools/bc31
set INCLUDE=%BCPATH%/include/
set LIB=%BCPATH%/lib/
set OLDPATH=%PATH%
PATH=%BCPATHR%;%PATH%

cd ..
set BCRUN=bcc -I%INCLUDE% -L%LIB% -nbc31 -eimf2mid main.c imf2mid.c

echo %BCRUN%
%BCRUN%

cd bc31
PATH=%OLDPATH%


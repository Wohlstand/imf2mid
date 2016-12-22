.AUTODEPEND

.PATH.obj = .

#		*Translator Definitions*
CC = bcc +IMF2MID.CFG
TASM = TASM
TLIB = tlib
TLINK = tlink
LIBPATH = C:\DEVTOOLS\BC31\LIB;
INCLUDEPATH = C:\DEVTOOLS\BC31\INCLUDE;..


#		*Implicit Rules*
.c.obj:
  $(CC) -c {$< }

.cpp.obj:
  $(CC) -c {$< }

#		*List Macros*


EXE_dependencies =  \
 imf2mid.obj \
 main.obj

#		*Explicit Rules*
: imf2mid.cfg $(EXE_dependencies)
  $(TLINK) /v/x/c/P-/L$(LIBPATH) @&&|
c0l.obj+
imf2mid.obj+
main.obj
imf2mid
		# no map file
fp87.lib+
mathl.lib+
cl.lib
|


#		*Individual File Dependencies*
imf2mid.obj: imf2mid.cfg ..\imf2mid.c 
	$(CC) -c ..\imf2mid.c

main.obj: imf2mid.cfg ..\main.c 
	$(CC) -c ..\main.c

#		*Compiler Configuration File*
imf2mid.cfg: imf2mid.mak
  copy &&|
-ml
-3
-f287
-A
-K
-C
-v
-y
-G
-O
-Og
-Oe
-Om
-Ov
-Ol
-Ob
-Op
-Oi
-Z
-k-
-vi-
-H=IMF2MID.SYM
-wpro
-weas
-wpre
-I$(INCLUDEPATH)
-L$(LIBPATH)
| imf2mid.cfg



# IMF2MID
An utility which converts IMF (Id-Software Music File) format into General MIDI.
Created by Wohlstand in 2016 year.


# What's IMF format?

It's a music format based on a sequence of native OPL2 chip values which are sequentially sending into chip registers.
The format used in various Id-Software games like Commander Keen and Wolfenstein and in a lot of games made and/or published by Apogee Software company.


# How to build

**GCC:**
```bash
gcc main.c imf2mid.c jwHash.c -o imf2mid
```

**CLang:**
```bash
clang main.c imf2mid.c jwHash.c -o imf2mid
```

**MSVC:**
```cmd
cl main.c imf2mid.c jwHash.c /link /out:imf2mid.exe
```

# Usage

```
./imf2mid [option] filename.imf [filename.mid]
```
* `-np` - ignore pitch change events
* `-nl` - disable printing log
* `-li` - write dump of detected instruments into "instlog.txt" file


# License
Licensed under MIT license


# TODO
* Add ability to define time multiplication coefficient to allow right alignment of notes per tact
* Add ability to manually declare patch ID per channel
* Add better generation of the drums
* Make more accurate drum velocity detection based on interpolation of attak parameter and note length


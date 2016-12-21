# imf2midi
A small utility which converts IMF (Id-Software Music File) into MIDI file

# How to build

**GCC:**
```bash
gcc main.c imf2mid.c -o imf2mid
```

**CLang:**
```bash
clang main.c imf2mid.c -o imf2mid
```

**MSVC:**
```cmd
cl main.c imf2mid.c /link /out:imf2mid.exe
```

# License
Licensed under MIT license

# TODO
* Add ability to define time multiplication coefficient to allow right alignment of notes per tact
* Add better pitch change detection
* Add ability to manually declare patch ID per channel
* Add better generation of the drums
* Make more accurate drum velocity detection based on interpolation of attak parameter and note length


/*
 * IMF2MIDI - a small utility to convert IMF music files into General MIDI
 *
 * Copyright (c) 2016 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdint.h>

#include "imf2mid.h"

int isFileExists(char* filePath)
{
    FILE* f = fopen(filePath, "rb");
    if(!f)
        return 0;
    fclose(f);
    return 1;
}

int printUsage()
{
    printf("\x1b[32mIMF2MID\x1b[0m utility converts imf (Id-Software Music File) format into General Midi by Wohlstand\n\n");
    printf("  \x1b[31mUsage:\x1b[0m\n");
    printf("     ./imf2mid \x1b[32mfilename.imf\x1b[0m \x1b[37m[filename.mid]\x1b[0m\n\n");
    return 1;
}

int main(int argc, char **argv)
{
    if(argc <= 1)
        return printUsage();

    if(!isFileExists(argv[1]))
    {
        fprintf(stderr, "\x1b[31mERROR:\x1b[0m Source file is invalid!");
        return printUsage();
    }

    return 0;
}
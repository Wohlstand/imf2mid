/*
 * IMF2MIDI - a small utility to convert IMF music files into General MIDI
 *
 * Copyright (c) 2016-2018 Vitaly Novichkov <admin@wohlnet.ru>
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
#include <string.h>
#include "imf2mid.h"
#include <ctype.h>


static int mystricmp(char const *a, char const *b)
{
    for (;; a++, b++) {
        int d = tolower(*a) - tolower(*b);
        if ((d != 0) || (!*a))
            return d;
    }
}

/**
 * @brief Checks existing of the file
 * @param filePath path to file to check for exists
 * @return 1 if file exist, 0 if not exist
 */
static int isFileExists(char* filePath)
{
    FILE* f = fopen(filePath, "rb");
    if(!f)
        return 0;
    fclose(f);
    return 1;
}

#define VERSION_STRING "\x1b[32mIMF2MID version " IMF2MID_VERSION "\x1b[0m"

/**
 * @brief Prints usage guide for this utility
 * @return always 1
 */
static int printUsage(void)
{
    printf("\n " VERSION_STRING "\n\n"
           "An utility which converts IMF (Id-Software Music File) format into General MIDI.\n"
           "Created by Wohlstand in 2016 year. Licensed under MIT license.\n\n"
           "More detail information and source code here:\n"
           "      https://github.com/Wohlstand/imf2mid\n\n");
    printf("  \x1b[31mUsage:\x1b[0m\n");
    printf("     ./imf2mid \x1b[37m[option]\x1b[0m \x1b[32mfilename.imf\x1b[0m \x1b[37m[filename.mid]\x1b[0m\n\n");
    printf(" -np   - ignore pitch change events\n");
    printf(" -nl   - disable printing log\n");
    printf(" -li   - write dump of detected instruments into \"instlog.txt\" file\n");
    printf("\n\n");

    return 1;
}

int main(int argc, char **argv)
{
    struct Imf2MIDI_CVT cvt;
    int logging = 1, noOptions = 0;

    if(argc <= 1)
        return printUsage();

    argv++;
    argc--;

    Imf2MIDI_init(&cvt);

    while(argc > 0)
    {
        if(!noOptions)
        {
            if(mystricmp(*argv, "--version") == 0)
            {
                printf(VERSION_STRING "\n");
                return 0;
            }
            else
            if(mystricmp(*argv, "-np") == 0)
                cvt.flag_usePitch = 0;
            else
            if(mystricmp(*argv, "-li") == 0)
                cvt.flag_logInstruments = 1;
            else
            if(mystricmp(*argv, "-nl") == 0)
                logging = 0;
            else
            {
                noOptions = 1;
                continue;
            }
        }
        else
        {
            if(!cvt.path_in)
            {
                if(!isFileExists(*argv))
                {
                    fprintf(stderr, "\x1b[31mERROR:\x1b[0m Source file %s is invalid!\n\n", *argv);
                    return printUsage();
                }
                cvt.path_in = *argv;
            }
            else
            if(!cvt.path_out)
            {
                cvt.path_out = *argv;
            }
        }

        argv++;
        argc--;
    }

    return Imf2MIDI_process(&cvt, logging);
}

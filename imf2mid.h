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

#ifndef CONVERTER_H
#define CONVERTER_H

#define IMF2MID_VERSION     "0.9b"

#if defined(MSDOS) || defined(__MSDOS__) || defined(_MSDOS) || defined(__DOS__)
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long  uint32_t;
typedef signed char    int8_t;
typedef signed short   int16_t;
typedef signed long    int32_t;
#else
#include <stdint.h>
#endif

struct AdLibInstrument
{
    uint8_t reg20[2];
    uint8_t reg40[2];
    uint8_t reg60[2];
    uint8_t reg80[2];
    uint8_t regC0;
    uint8_t regE0[2];
    uint8_t patch;
};

struct Imf2MIDI_CVT
{
    struct AdLibInstrument imf_instruments[9];
    struct AdLibInstrument imf_instrumentsPrev[9];

    /* MIDI props */
    double   midi_tempo;
    uint32_t midi_resolution;
    uint8_t  midi_mapchannel[9];
    uint32_t midi_lastpatch[9];
    uint16_t midi_lastpitch[9];
    uint32_t midi_trackBegin;
    uint32_t midi_pos;
    uint32_t midi_fileSize;
    uint32_t midi_tracksNum;
    int      midi_eventCode;
    int      midi_isEndOfTrack;
    uint32_t midi_delta;
    uint32_t midi_time;

    /* File paths */
    char    *path_in;
    char    *path_out;

    /* Flags */
    int      flag_usePitch;
};

extern void Imf2MIDI_init(struct Imf2MIDI_CVT *cvt);
extern int  Imf2MIDI_process(struct Imf2MIDI_CVT *cvt, int log);


#endif /* CONVERTER_H */

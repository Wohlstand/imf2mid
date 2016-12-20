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

#include "imf2mid.h"
#include <memory.h>

#define  MIDI_PITCH_CENTER 0x2000

void Imf2MIDI_init(Imf2MIDI_CVT *cvt)
{
    memset(cvt->imf_instruments, 0, sizeof(cvt->imf_instruments));
    memset(cvt->midi_mapchannel, 0, sizeof(cvt->midi_mapchannel));
    cvt->midi_resolution = 384;
    cvt->midi_tempo = 110.0;
    cvt->midi_lastpitch = MIDI_PITCH_CENTER;
}

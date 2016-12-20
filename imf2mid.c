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
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#define  MIDI_PITCH_CENTER 0x2000
#define  MThd "MThd"
#define  MTrk "MTrk"

#define  MIDI_CONTROLLER_VOLUME 7

void Imf2MIDI_init(struct Imf2MIDI_CVT *cvt)
{
    if(!cvt)
        return;

    memset(cvt->imf_instruments, 0, sizeof(cvt->imf_instruments));
    memset(cvt->midi_mapchannel, 0, sizeof(cvt->midi_mapchannel));
    memset(cvt->midi_lastpatch,  0, sizeof(cvt->midi_lastpatch));

    cvt->midi_resolution    = 384;
    cvt->midi_tempo         = 110.0;
    cvt->midi_lastpitch     = MIDI_PITCH_CENTER;
    cvt->midi_trackBegin    = 0;
    cvt->midi_pos           = 0;
    cvt->midi_fileSize      = 0;
    cvt->midi_tracksNum     = 0;
    cvt->midi_eventCode     = -1;
    cvt->midi_isEndOfTrack  = 1;
    cvt->midi_delta         = 0;
    cvt->midi_time          = 0;

    cvt->path_in    = NULL;
    cvt->path_out   = NULL;
}

static uint16_t readLE16(FILE* f)
{
    uint16_t out = 0;
    uint8_t bytes[2];

    if(fread(bytes, 1, 2, f) != 2)
        return 0;

    out  = bytes[0] & 0x00FF;
    out += bytes[1] & 0xFF00;
    return out;
}

static uint32_t readLE32(FILE* f)
{
    uint32_t out = 0;
    uint8_t bytes[4];

    if(fread(bytes, 1, 4, f) != 4)
        return 0;

    out  = bytes[0] & 0x000000FF;
    out += bytes[1] & 0x0000FF00;
    out += bytes[2] & 0x00FF0000;
    out += bytes[3] & 0xFF000000;
    return out;
}

static int write8(FILE* f, uint8_t in)
{
    return (int)fwrite(&in, 1, 1, f);
}

static int writeLE16(FILE* f, uint32_t in)
{
    uint8_t bytes[2];
    bytes[0] = in & 0xFF;
    bytes[1] = (in>>8) & 0xFF;
    return (int)fwrite(bytes, 1, 2, f);
}

static int writeLE24(FILE* f, uint32_t in)
{
    uint8_t bytes[3];
    bytes[0] = in & 0xFF;
    bytes[1] = (in>>8) & 0xFF;
    bytes[2] = (in>>16) & 0xFF;
    return (int)fwrite(bytes, 1, 3, f);
}

static int writeLE32(FILE* f, uint32_t in)
{
    uint8_t bytes[4];
    bytes[0] = in & 0xFF;
    bytes[1] = (in>>8) & 0xFF;
    bytes[2] = (in>>16) & 0xFF;
    bytes[3] = (in>>24) & 0xFF;
    return (int)fwrite(bytes, 1, 4, f);
}

static int writeVarLen32(FILE* f, uint32_t in)
{
    uint32_t buffer;
    uint32_t value = in;
    uint8_t  bytes[4];
    uint8_t *ptr = bytes;
    uint8_t  len = 0;

    buffer = value & 0x7f;
    while ((value >>= 7) > 0)
    {
        buffer <<= 8;
        buffer |= 0x80;
        buffer += (value & 0x7f);
    }
    while(1)
    {
        *ptr++ = (uint8_t)buffer;
        len++;
        if (buffer & 0x80)
            buffer >>= 8;
        else
            break;
    }
    return (int)fwrite(bytes, 1, len, f);
}

static void MIDI_addDelta(struct Imf2MIDI_CVT *cvt, uint32_t delta)
{
    cvt->midi_delta += delta;
}

static void MIDI_writeHead(FILE* f, struct Imf2MIDI_CVT *cvt)
{
    fseek(f, 0, SEEK_SET);
    fwrite(MThd, 1, 4, f);
    writeLE32(f, 6);//Size of the head
    writeLE32(f, 0);//MIDI format 0
    writeLE32(f, 0);//Zero tracks count
    writeLE32(f, cvt->midi_resolution);
    cvt->midi_fileSize = 20;
}

static void MIDI_writeEventCode(FILE*f,
                                struct Imf2MIDI_CVT *cvt,
                                uint8_t eventCode)
{
    if( (eventCode != cvt->midi_eventCode) || (eventCode > 0x9f) )
        write8(f, eventCode);
    cvt->midi_eventCode = eventCode;
}

static void MIDI_writeMetaEvent(FILE*f,
                                struct Imf2MIDI_CVT *cvt,
                                uint8_t type,
                                int8_t  *bytes,
                                uint32_t size)
{
    writeVarLen32(f, cvt->midi_delta);
    cvt->midi_delta = 0;

    MIDI_writeEventCode(f, cvt, 0xFF);
    write8(f, type);
    writeVarLen32(f, size);
    fwrite(bytes, 1, (size_t)size, f);
    cvt->midi_fileSize = ftell(f);
}

static void MIDI_writeControlEvent(FILE*f,
                                   struct Imf2MIDI_CVT *cvt,
                                   uint8_t channel,
                                   uint8_t controller,
                                   uint8_t value)
{
    writeVarLen32(f, cvt->midi_delta);
    cvt->midi_delta = 0;

    channel = channel % 16;
    MIDI_writeEventCode(f, cvt, 0xB0 + channel);
    write8(f, controller);
    write8(f, value);
    cvt->midi_fileSize = ftell(f);
}

static void MIDI_writePatchChangeEvent(FILE* f,
                                       struct Imf2MIDI_CVT *cvt,
                                       uint8_t channel,
                                       uint8_t patch)
{
    writeVarLen32(f, cvt->midi_delta);
    cvt->midi_delta = 0;

    channel = channel % 16;
    MIDI_writeEventCode(f, cvt, 0xC0 + channel);
    write8(f, patch);
    cvt->midi_fileSize = ftell(f);
}

static void MIDI_writePitchEvent(FILE*f,
                                 struct Imf2MIDI_CVT *cvt,
                                 uint8_t    channel,
                                 uint16_t   value)
{
    writeVarLen32(f, cvt->midi_delta);
    cvt->midi_delta = 0;

    channel = channel % 16;
    MIDI_writeEventCode(f, cvt, 0xE0 + channel);
    write8(f,  value & 0x7F);
    write8(f, (value>>7) & 0x7F);
    cvt->midi_fileSize = ftell(f);
}

static void MIDI_writeNoteOnEvent(FILE*f,
                                 struct Imf2MIDI_CVT *cvt,
                                 uint8_t    channel,
                                 uint8_t    key,
                                 uint8_t    velocity)
{
    writeVarLen32(f, cvt->midi_delta);
    cvt->midi_delta = 0;

    channel = channel % 16;
    MIDI_writeEventCode(f, cvt, 0x90 + channel);
    write8(f,  key);
    write8(f,  velocity);
    cvt->midi_fileSize = ftell(f);
}

static void MIDI_writeNoteOffEvent(FILE*f,
                                   struct Imf2MIDI_CVT *cvt,
                                   uint8_t   channel,
                                   uint8_t   key,
                                   uint8_t   velocity)
{
    writeVarLen32(f, cvt->midi_delta);
    cvt->midi_delta = 0;

    channel = channel % 16;
    uint8_t code = ((velocity != 0) ||
                   (cvt->midi_eventCode < 0) ||
                    ((cvt->midi_eventCode & 0xF0) != 0x90)) ? 0x80 : 0x90;
    MIDI_writeEventCode(f, cvt, code + channel);
    write8(f,  key);
    write8(f,  velocity);
    cvt->midi_fileSize = ftell(f);
}

static void MIDI_writeTempoEvent(FILE*f,
                                struct Imf2MIDI_CVT *cvt,
                                uint32_t ticks)
{
    writeVarLen32(f, cvt->midi_delta);
    cvt->midi_delta = 0;

    MIDI_writeEventCode(f, cvt, 0xFF);
    write8(f, 0x51);
    write8(f, 0x03);
    writeLE24(f, ticks);
    cvt->midi_fileSize = ftell(f);
}

static void MIDI_writeMetricKeyEvent(FILE*f,
                                     struct Imf2MIDI_CVT *cvt,
                                     uint8_t nom,
                                     uint8_t denom,
                                     uint8_t key1,
                                     uint8_t key2)
{
    writeVarLen32(f, cvt->midi_delta);
    cvt->midi_delta = 0;

    uint8_t denomID = (uint8_t)(log((double)denom) / log(2.0));
    MIDI_writeEventCode(f, cvt, 0xFF);
    write8(f, 0x58);
    write8(f, 0x04);
    write8(f, nom);
    write8(f, denomID);
    write8(f, key1);
    write8(f, key2);
    cvt->midi_fileSize = ftell(f);
}


static void MIDI_beginTrack(FILE* f, struct Imf2MIDI_CVT *cvt)
{
    if(!cvt->midi_isEndOfTrack)
        return;

    cvt->midi_time = 0;
    cvt->midi_delta = 0;
    cvt->midi_eventCode = -1;
    cvt->midi_isEndOfTrack = 0;
    fwrite(MTrk, 1, 4, f);
    cvt->midi_trackBegin = cvt->midi_fileSize + 4;
    writeLE32(f, 0);//Track length
    cvt->midi_tracksNum++;
    cvt->midi_fileSize = ftell(f);
}

static void MIDI_endTrack(FILE* f, struct Imf2MIDI_CVT *cvt)
{
    if(cvt->midi_isEndOfTrack)
        return;

    MIDI_writeMetaEvent(f, cvt, 0x2f, 0, 0);
    cvt->midi_isEndOfTrack = 1;
    fseek(f, cvt->midi_trackBegin + 4, SEEK_SET);
    writeLE32( f, cvt->midi_fileSize - cvt->midi_trackBegin - 8);
    fseek(f, cvt->midi_fileSize, SEEK_SET);
    cvt->midi_trackBegin = 0;
}

const uint32_t note_frequencies[] =
{
    0x159, // C      24
    0x16B, // C#     25
    0x181, // D      26
    0x198, // D#     27
    0x1B0, // E      28
    0x1CA, // F      29
    0x1E5, // F#     30
    0x202, // G      31
    0x220, // G#     32
    0x241, // A      33
    0x263, // A#     34
    0x287, // B      35
    0x2AE, // C'     36
    0x2DB, // C#'    37
    0x306, // D'     38
    0x334, // D#'    39
    0x365, // E'     40
    0x399, // F'     41
    0x3CF, // F#'    42
    0x3FE, // G'     43
    0
};

uint8_t hzToKey(uint32_t hz, uint32_t octave)
{
    //TODO: Use frequency multiplexing register value to detect octave accurate
    octave += 2; //Fix missed octaves!
    //--
    octave *= 12;

    if(hz == 0)
        return 0;

    int found = 0;
    int32_t  nearestIndex    = -1;
    uint32_t nearestDistance = 0;

    for(uint32_t i = 0; note_frequencies[i]; i++)
    {
        uint32_t dist = (uint32_t)fabs((double)(hz - note_frequencies[i]));

        if((found == 0) || (dist < nearestDistance))
        {
            found           = 1;
            nearestIndex    = i;
            nearestDistance = dist;
        }
    }

    return found ? (octave + nearestIndex) : 0;
}

int Imf2MIDI_process(struct Imf2MIDI_CVT* cvt, int log)
{
    int     res = 0;
    char    *path_out = NULL;
    FILE    *file_in  = NULL;
    FILE    *file_out = NULL;

    uint32_t imf_length = 0;
    uint32_t imf_delay  = 0;
    uint32_t imf_freq[9];
    uint32_t imf_octave  = 0;
    uint8_t  imf_channel = 0;
    uint8_t  imf_regKey = 0;
    uint8_t  imf_regVal = 0;


    if(!cvt)
        return 0;

    //Calculate target path
    if(!cvt->path_out)
    {
        size_t len = strlen(cvt->path_in);
        path_out = (char *)malloc(len + 5);
        memset(path_out, 0, len + 5);
        strncpy(path_out, cvt->path_in, len);

        if(len >= 4)
        {
            char *ext = (path_out + len - 4);
            memcpy(path_out + len - ((ext[0] == '.') ? 4 : 0), ".mid\0", 5);
        }
        else
            memcpy(path_out + len, ".mid\0", 5);

        cvt->path_out = path_out;
    }

    if(strcmp(cvt->path_in, cvt->path_out) == 0)
    {
        fprintf(stderr, "\x1b[31mERROR:\x1b[0m File names are must not be same!");
        res = 0;
        goto quit;
    }

    if(log)
    {
        printf("=============================\n"
               "Convert into \"%s\"\n"
               "=============================\n\n", cvt->path_out);
    }

    file_in  = fopen(cvt->path_in, "rb");
    file_out = fopen(cvt->path_out, "wb");

    if(!file_in)
    {
        fprintf(stderr, "\x1b[31mERROR:\x1b[0m Can't open file %s for read!", cvt->path_in);
        res = 0;
        goto quit;
    }

    if(!file_out)
    {
        fprintf(stderr, "\x1b[31mERROR:\x1b[0m Can't open file %s for write!", cvt->path_out);
        res = 0;
        goto quit;
    }

    imf_length = readLE32(file_in);
    if(imf_length == 0)
    {
        fprintf(stderr, "\x1b[31mERROR:\x1b[0m Failed to read IMF length!");
        goto quit;
    }

    MIDI_writeHead(file_out, cvt);
    MIDI_beginTrack(file_out, cvt);
    MIDI_writeTempoEvent(file_out, cvt, (uint32_t)(60000000.0 / cvt->midi_tempo));
    MIDI_writeMetricKeyEvent(file_out, cvt, 4, 4, 24, 8);

    uint8_t c;
    for(c = 0; c < 9; c++)
    {
        imf_freq[c] = 0;
        cvt->midi_mapchannel[c] = c;
    }

    for(c = 0; c <= 8; c++)
    {
        MIDI_writeControlEvent(file_out, cvt, c, MIDI_CONTROLLER_VOLUME, 127);
        MIDI_writePatchChangeEvent(file_out, cvt, c, c);
        cvt->midi_lastpatch[c] = c;
    }

    while((imf_length > 0) && (ftell(file_in) != EOF))
    {
        imf_length -= 4;
        imf_delay = readLE16(file_in);
        imf_regKey = fgetc(file_in);
        imf_regVal = fgetc(file_in);
        MIDI_addDelta(cvt, imf_delay);

        if((imf_regKey >= 0xA0) && (imf_regKey <= 0xA8))
        {
            imf_channel = imf_regKey - 0xa0;
            imf_freq[imf_channel] = (imf_freq[imf_channel] & 0x0F00)|(imf_regVal & 0xFF);
            continue;
        }

        if((imf_regKey >= 0xB0) && (imf_regKey <= 0xB8))
        {
            imf_channel = imf_regKey - 0xB0;
            imf_freq[imf_channel] = (imf_freq[imf_channel] & 0x00FF)|((imf_regVal & 0x03) << 0x08);
            imf_octave = (imf_regVal >> 0x02) & 0x07;
            int keyon = (imf_regVal >> 5) & 1;
            uint8_t key = hzToKey(imf_freq[imf_channel], imf_octave);

            if(key > 0)
            {
                if(keyon)
                {
                    MIDI_writeNoteOnEvent(file_out, cvt, cvt->midi_mapchannel[imf_channel], key, 127);
                }
                else
                {
                    MIDI_writeNoteOffEvent(file_out, cvt, cvt->midi_mapchannel[imf_channel], key, 0);
                }
            }

            continue;
        }
    }

    MIDI_endTrack(file_out, cvt);

    if(log)
    {
        printf("=============================\n"
               "   Work has been completed!\n"
               "=============================\n\n");
    }

    res = 1;

quit:
    if(file_in)
        fclose(file_in);

    if(file_out)
        fclose(file_out);

    if(path_out)
    {
        free(path_out);
        path_out = NULL;
        cvt->path_out = NULL;
    }

    return res;
}

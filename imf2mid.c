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

#include "imf2mid.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>


#define  MIDI_PITCH_CENTER      0x2000
#define  MIDI_CONTROLLER_VOLUME 7


/*****************************************************************
 *                    Bufferized output                          *
 *****************************************************************/

#define ENABLE_BUFFERIZED_WRITE
#define BUF_MAX_SIZE  20480

#ifdef ENABLE_BUFFERIZED_WRITE
static char    BUF_output[BUF_MAX_SIZE];
static size_t  BUF_stored   = 0;
static size_t  BUF_lastPos  = 0;
#endif

static void fflushb(FILE* output)
{
    #ifdef ENABLE_BUFFERIZED_WRITE
    if(BUF_stored == 0)
        return;
    fwrite(BUF_output, 1, BUF_stored, output);
    BUF_lastPos += BUF_stored;
    BUF_stored = 0;
    #else
    fflush(output);
    #endif
}

static void fseekb(FILE*f, long b)
{
    #ifdef ENABLE_BUFFERIZED_WRITE
    fflushb(f);
    #endif
    fseek(f, b, SEEK_SET);
    #ifdef ENABLE_BUFFERIZED_WRITE
    BUF_lastPos = (size_t)ftell(f);
    #endif
}

static long ftellb(FILE* file)
{
    #ifdef ENABLE_BUFFERIZED_WRITE
    (void)file;
    return (long)(BUF_lastPos + BUF_stored);
    #else
    return ftell(file);
    #endif
}

static size_t fwriteb(char* buf, size_t elements, size_t size, FILE* output)
{
    #ifdef ENABLE_BUFFERIZED_WRITE
    size_t newSize = elements * size;
    if(BUF_MAX_SIZE < (BUF_stored + newSize))
    {
        fflushb(output);
        newSize = size;
    }

    memcpy(BUF_output + BUF_stored, buf, newSize);

    BUF_stored += newSize;
    return size;
    #else
    return fwrite(buf, elements, size, output);
    #endif
}

/*****************************************************************/


/*****************************************************************
 *             Parsing endian-specific integers                  *
 *****************************************************************/
#if 0
static uint16_t readLE16(FILE* f)
{
    uint16_t out = 0;
    uint8_t bytes[2];

    if(fread(bytes, 1, 2, f) != 2)
        return 0;

    out  = (uint16_t)bytes[0] & 0x00FF;
    out |= ((uint16_t)bytes[1]<<8) & 0xFF00;
    return out;
}
#endif

static uint32_t readLE32(FILE* f)
{
    uint32_t out = 0;
    uint8_t bytes[4];

    if(fread(bytes, 1, 4, f) != 4)
        return 0;

    out  = (uint32_t)bytes[0] & 0x000000FF;
    out |= ((uint32_t)bytes[1]<<8) & 0x0000FF00;
    out |= ((uint32_t)bytes[2]<<16) & 0x00FF0000;
    out |= ((uint32_t)bytes[3]<<24) & 0xFF000000;
    return out;
}
/*****************************************************************/


/*****************************************************************
 *             Writing endian-specific integers                  *
 *****************************************************************/
#define write8(f, in) { uint8_t inX = (uint8_t)in; fwriteb((char*)&inX, 1, 1, (f)); }

static int writeBE16(FILE* f, uint32_t in)
{
    uint8_t bytes[2];
    bytes[1] = in & 0xFF;
    bytes[0] = (in>>8) & 0xFF;
    return (int)fwriteb((char*)bytes, 1, 2, f);
}

#if 0
static int writeLE24(FILE* f, uint32_t in)
{
    uint8_t bytes[3];
    bytes[0] = in & 0xFF;
    bytes[1] = (in>>8) & 0xFF;
    bytes[2] = (in>>16) & 0xFF;
    return (int)fwriteb((char*)bytes, 1, 3, f);
}
#endif

static int writeBE24(FILE* f, uint32_t in)
{
    uint8_t bytes[3];
    bytes[2] = in & 0xFF;
    bytes[1] = (in>>8) & 0xFF;
    bytes[0] = (in>>16) & 0xFF;
    return (int)fwriteb((char*)bytes, 1, 3, f);
}

#if 0
static int writeLE32(FILE* f, uint32_t in)
{
    uint8_t bytes[4];
    bytes[0] = in & 0xFF;
    bytes[1] = (in>>8) & 0xFF;
    bytes[2] = (in>>16) & 0xFF;
    bytes[3] = (in>>24) & 0xFF;
    return (int)fwriteb((char*)bytes, 1, 4, f);
}
#endif

static int writeBE32(FILE* f, uint32_t in)
{
    uint8_t bytes[4];
    bytes[3] = in & 0xFF;
    bytes[2] = (in>>8) & 0xFF;
    bytes[1] = (in>>16) & 0xFF;
    bytes[0] = (in>>24) & 0xFF;
    return (int)fwriteb((char*)bytes, 1, 4, f);
}

/**
 * @brief Write variable-length integer
 * @param f file to write
 * @param in input value
 * @return Count of written bytes
 */
static int writeVarLen32(FILE* f, uint32_t in)
{
    uint8_t  bytes[4];
    size_t   len = 0;
    uint8_t *ptr = bytes;
    int32_t  i;

    uint8_t isFirst = 1;
    for(i = 3; i >= 0; i--)
    {
        uint32_t b = in>>(7*i);
        uint8_t byte = (uint8_t)b & 127;
        if(!isFirst || (byte > 0) || (i == 0))
        {
            isFirst = 0;
            if(i > 0)
            {
                /* set 8th bit */
                byte |= 128;
            }
            *ptr++ = byte;
            len++;
        }
    }
    return (int)fwriteb((char*)bytes, 1, len, f);
}
/*****************************************************************/


/*****************************************************************
 *                        MIDI Writing                           *
 *****************************************************************/
static void MIDI_addDelta(struct Imf2MIDI_CVT *cvt, uint32_t delta)
{
    cvt->midi_delta += delta;
}

static void MIDI_writeHead(FILE* f, struct Imf2MIDI_CVT *cvt)
{
    fseekb(f, 0);
    fwriteb((char*)"MThd", 1, 4, f);        /* 0  */
    writeBE32(f, 6);/* Size of the head */  /* 4  */
    writeBE16(f, 0);/* MIDI format 0    */  /* 8  */
    writeBE16(f, 0);/* Zero tracks count*/  /* 10 */
    writeBE16(f, cvt->midi_resolution);     /* 12 */
    cvt->midi_fileSize = (uint32_t)ftellb(f);
}

static void MIDI_closeHead(FILE* f, struct Imf2MIDI_CVT *cvt)
{
    fseekb(f, 10);
    writeBE16(f, cvt->midi_tracksNum);
    fflushb(f);
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
    fwriteb((char*)bytes, 1, (size_t)size, f);
    cvt->midi_fileSize = (uint32_t)ftellb(f);
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
    cvt->midi_fileSize = (uint32_t)ftellb(f);
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
    cvt->midi_fileSize = (uint32_t)ftellb(f);
}

static void MIDI_writePitchEvent(FILE*f,
                                 struct Imf2MIDI_CVT *cvt,
                                 uint8_t    channel,
                                 uint16_t   value)
{
    channel = channel % 9;

    if(cvt->midi_lastpitch[channel] == value)
        return;/* Don't write pitch if value is same */

    writeVarLen32(f, cvt->midi_delta);
    cvt->midi_delta = 0;


    MIDI_writeEventCode(f, cvt, 0xE0 + channel);
    write8(f,  value & 0x7F);
    write8(f, (value>>7) & 0x7F);
    cvt->midi_fileSize = (uint32_t)ftellb(f);
    /* Remember pitch value to don't repeat */
    cvt->midi_lastpitch[channel] = value;
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
    cvt->midi_fileSize = (uint32_t)ftellb(f);
}

static void MIDI_writeNoteOffEvent(FILE*f,
                                   struct Imf2MIDI_CVT *cvt,
                                   uint8_t   channel,
                                   uint8_t   key,
                                   uint8_t   velocity)
{
    uint8_t code = ((velocity != 0) ||
                   (cvt->midi_eventCode < 0) ||
                  ((cvt->midi_eventCode & 0xF0) != 0x90)) ? 0x80 : 0x90;
    channel = channel % 16;

    writeVarLen32(f, cvt->midi_delta);
    cvt->midi_delta = 0;

    MIDI_writeEventCode(f, cvt, code + channel);
    write8(f,  key);
    write8(f,  velocity);
    cvt->midi_fileSize = (uint32_t)ftellb(f);
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
    writeBE24(f, ticks);
    cvt->midi_fileSize = (uint32_t)ftellb(f);
}

static void MIDI_writeMetricKeyEvent(FILE*f,
                                     struct Imf2MIDI_CVT *cvt,
                                     uint8_t nom,
                                     uint8_t denom,
                                     uint8_t key1,
                                     uint8_t key2)
{
    uint8_t denomID = (uint8_t)(log((double)denom) / log(2.0));

    writeVarLen32(f, cvt->midi_delta);
    cvt->midi_delta = 0;

    MIDI_writeEventCode(f, cvt, 0xFF);
    write8(f, 0x58);
    write8(f, 0x04);
    write8(f, nom);
    write8(f, denomID);
    write8(f, key1);
    write8(f, key2);
    cvt->midi_fileSize = (uint32_t)ftellb(f);
}


static void MIDI_beginTrack(FILE* f, struct Imf2MIDI_CVT *cvt)
{
    if(!cvt->midi_isEndOfTrack)
        return;

    cvt->midi_time = 0;
    cvt->midi_delta = 0;
    cvt->midi_eventCode = -1;
    cvt->midi_isEndOfTrack = 0;
    fwriteb((char*)"MTrk", 1, 4, f);
    cvt->midi_trackBegin = (uint32_t)ftellb(f);
    writeBE32(f, 0); /* Track length */
    cvt->midi_tracksNum++;
    cvt->midi_fileSize = (uint32_t)ftellb(f);
}

static void MIDI_endTrack(FILE* f, struct Imf2MIDI_CVT *cvt)
{
    if(cvt->midi_isEndOfTrack)
        return;

    MIDI_writeMetaEvent(f, cvt, 0x2f, 0, 0);
    cvt->midi_isEndOfTrack = 1;
    fseekb(f, cvt->midi_trackBegin);
    writeBE32( f, cvt->midi_fileSize - cvt->midi_trackBegin - 4);
    fseekb(f, cvt->midi_fileSize);
    cvt->midi_trackBegin = 0;
}

/*****************************************************************/


/*****************************************************************
 *                        Index tables                           *
 *****************************************************************/
static const uint16_t note_frequencies[] =
{
    345, /* C   24 */
    363, /* C#  25 */
    385, /* D   26 */
    408, /* D#  27 */
    432, /* E   28 */
    458, /* F   29 */
    485, /* F#  30 */
    514, /* G   31 */
    544, /* G#  32 */
    577, /* A   33 */
    611, /* A#  34 */
    647, /* B   35 */
    686, /* C'  36 */
    731, /* C#' 37 */
    774, /* D'  38 */
    820, /* D#' 39 */
    869, /* E'  40 */
    921, /* F'  41 */
    975, /* F#' 42 */
    1022, /* G' 43 */
    0
};

static uint8_t opl2_opChannel[] =
{
  /*0  1  2  3  4  5*/
    0, 1, 2, 0, 1, 2,
  /*6, 7,*/
    0, 0,
  /*8, 9, A, B, C, D*/
    3, 4, 5, 3, 4, 5,
  /*E, F,*/
    0, 0,
  /*10,11,12,13,14,15*/
    6, 7, 8, 6, 7, 8,
    0
};

static uint8_t opl2_op[] =
{
    /*0  1  2  3  4  5*/
      0, 0, 0, 1, 1, 1,
    /*6, 7,*/
      0, 0,
    /*8, 9, A, B, C, D*/
      0, 0, 0, 1, 1, 1,
    /*E, F,*/
      0, 0,
    /*10,11,12,13,14,15*/
      0, 0, 0, 1, 1, 1,
      0
};
/*****************************************************************/


/*****************************************************************
 *                     Frequency management                      *
 *****************************************************************/
static int8_t nearestFreq(uint16_t hz)
{
    int8_t      nearestIndex    = -1;
    uint16_t    nearestDistance = 0;
    int8_t      i = 0;

    for(i = 0; i < 21; i++)
    {
        uint16_t dist = (uint16_t)fabs((double)hz - (double)note_frequencies[i]);

        if((i == 0) || (dist < nearestDistance))
        {
            nearestIndex    = i;
            nearestDistance = dist;
        }
    }
    return nearestIndex;
}

static int16_t relativeFreq(int16_t i, int16_t halfNotes)
{
    int16_t direction = (halfNotes > 0) ? 1 : -1;

    if((i < 0) || (!note_frequencies[i]))
        return -1;

    while((i >= 0) && (note_frequencies[i]) && (halfNotes != 0))
    {
        halfNotes -= direction;
        i         += direction;
    }

    if(halfNotes == 0)
        return (int16_t)note_frequencies[i];

    return -1;
}

static uint8_t hzToKey(uint16_t hz,
                       uint8_t  octave,
                       uint8_t  multL,
                       uint8_t  multH,
                       uint8_t  wsL,
                       uint8_t  wsH)
{
    int8_t nearestIndex;

    /*
     * Attempt to find best octave with using frequency multiplication values
     */
#if 1
    /* Variant 1: minimal frequency multiplication */
    uint8_t mult = multL < multH ? multL : multH;
    mult -= wsL;
    mult -= wsH;
#else
    /* Variant 2: middle value between two multiplications */
    wsL = (wsL == 3) ? 1 : 0;
    wsH = (wsH == 3) ? 1 : 0;
    uint8_t mult = ((multL + multH) / 2);
    mult -= wsL;
    mult -= wsH;
#endif

    octave++;

    if(mult == 0)
        octave--; /* 1/2x */
    else if(mult > 1)
        octave += (mult-1); /* 2x, 3x, 4x, 5x,.... */

    if(octave > 9)
        octave = 9;

    octave *= 12;

    if(hz == 0)
        return 0;

    nearestIndex = nearestFreq(hz);

    if(nearestIndex < 0)
        return 0;

    return (uint8_t)(octave + nearestIndex);
}
/*****************************************************************/


/*****************************************************************
 *              Hash table by Jonathan Watmough
 * Reduced version, added only used functions:
 * (heler funcs, create hash, delete hash, add int by str, and get int by str)
 * Full implementation of hash table for C can be retreived here:
 * https://github.com/watmough/jwHash
 *****************************************************************/

/* resuts codes */
typedef enum
{
    HASHOK,
    HASHADDED,
    HASHREPLACEDVALUE,
    HASHALREADYADDED,
    HASHDELETED,
    HASHNOTFOUND
} HASHRESULT;

typedef enum
{
    HASHPTR,
    HASHNUMERIC,
    HASHSTRING
} HASHVALTAG;

typedef struct jwHashEntry jwHashEntry;
struct jwHashEntry
{
    union
    {
        char  *strValue;
        unsigned long intValue;
    } key;
    HASHVALTAG valtag;
    union
    {
        long   intValue;
    } value;
    jwHashEntry *next;
};

typedef struct jwHashTable jwHashTable;
struct jwHashTable
{
    /* pointer to array of buckets */
    jwHashEntry **bucket;
    unsigned long buckets;
    /* if we resize, may need to hash multiple times */
    unsigned long bucketsinitial;
    HASHRESULT lastError;
};

/*================= STATIC HELPER FUNCTIONS=================*/

/* http://www.cse.yorku.ca/~oz/hash.html *
 * hash function for string keys djb2    */
static unsigned long int hashString(char * str)
{
    unsigned long hash = 5381;
    unsigned long c;

    while( (c = (unsigned long)*str++) != 0 )
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

/* helper for copying string keys and values */
static char * copystring(char * value)
{
    char *copy = (char *)malloc(strlen(value) + 1);
    if(!copy)
    {
        fprintf(stderr, "Unable to allocate string value %s\n", value);
        abort();
    }
    strcpy(copy, value);
    return copy;
}

/******** Create hash table ********/
static jwHashTable *create_hash(size_t buckets)
{
    /* allocate space */
    jwHashTable *table = (jwHashTable *)malloc(sizeof(jwHashTable));

    if(!table)
    {
        /* unable to allocate */
        return NULL;
    }

    /* setup */
    table->bucket = (jwHashEntry **)malloc(buckets * sizeof(void*));
    if( !table->bucket )
    {
        free(table);
        return NULL;
    }

    memset(table->bucket, 0, buckets * sizeof(void*));
    table->buckets = table->bucketsinitial = buckets;
    return table;
}

/******** Delete hash table ********/
static void *delete_hash(jwHashTable *table)
{
    size_t i =0 ;
    for(; i < table->buckets; i++)
    {
        jwHashEntry *entry = table->bucket[i];
        if(entry != 0)
        {
            /* delete string value if needed */
            if(entry->key.strValue)
                free(entry->key.strValue);
            free(entry);
        }
    }

    free(table->bucket);
    free(table);
    return NULL;
}

static HASHRESULT add_int_by_str( jwHashTable *table, char *key, long int value )
{
    jwHashEntry *entry;

    /* compute hash on key */
    unsigned long hash = hashString(key);
    hash %= table->buckets;

    /* check entry */
    entry = table->bucket[hash];

    /* already an entry */
    while(entry != 0)
    {
        /* check for already indexed */
        if(0 == strcmp(entry->key.strValue, key) && (value == entry->value.intValue))
            return HASHALREADYADDED;

        /* check for replacing entry */
        if(0 == strcmp(entry->key.strValue, key) && (value != entry->value.intValue))
        {
            entry->value.intValue = (int)value;
            return HASHREPLACEDVALUE;
        }

        /* move to next entry */
        entry = entry->next;
    }

    /* create a new entry and add at head of bucket */
    entry = (jwHashEntry *)malloc(sizeof(jwHashEntry));
    entry->key.strValue = copystring(key);
    entry->valtag = HASHNUMERIC;
    entry->value.intValue = (int)value;
    entry->next = table->bucket[hash];
    table->bucket[hash] = entry;

    return HASHOK;
}

/* Lookup int - keyed by str */
static HASHRESULT get_int_by_str( jwHashTable *table, char *key, int *i )
{
    /* compute hash on key */
    unsigned long hash = hashString(key) % table->buckets;

    /* get entry */
    jwHashEntry *entry = table->bucket[hash];

    /* already an entry */
    while(entry)
    {
        /* check for key */
        if(0 == strcmp(entry->key.strValue, key))
        {
            *i = (int)entry->value.intValue;
            return HASHOK;
        }

        /* move to next entry */
        entry = entry->next;
    }

    /* not found */
    return HASHNOTFOUND;
}
/*****************************************************************/


/*****************************************************************
 *                    Instrument management                      *
 *****************************************************************/

static int instcmp(struct AdLibInstrument *inst1, struct AdLibInstrument* inst2)
{
    int cmp = 0;
    cmp += (memcmp(inst1->reg20, inst2->reg20, 2) != 0);
    cmp += ((inst1->reg40[0] & 0xC0) != (inst2->reg40[0] & 0xC0)); /* Don't compare carrier's volume level! */
    cmp += (memcmp(&inst1->reg40[2], &inst1->reg40[2], 1) != 0);
    cmp += (memcmp(inst1->reg60, inst2->reg60, 2) != 0);
    cmp += (memcmp(inst1->reg80, inst2->reg80, 2) != 0);
    cmp += (memcmp(&inst1->regC0, &inst2->regC0, 1) != 0);
    cmp += (memcmp(inst1->regE0, inst2->regE0, 2) != 0);
    return cmp;
}

static int readInstLine(FILE* file, char* buffer)
{
    int len = 0;
    char* pos = buffer;
    int c = 0, commentI = -1;

    do
    {
        if(len >= 100)
            break;

        c = fgetc(file);

        if(c >= 0)
        {
            (*pos++) = (char)c;
            if(c == '/')
                commentI = len;
            len++;
        }

        if((c == EOF) && (len == 0))
            return -1;
    }
    while( ((char)c != '\n') && (c != EOF));

    buffer[len + 1] = '\0';

    if(commentI >= 0) /* Cut comment text */
        buffer[commentI] = '\0';

    return len;
}

static jwHashTable* loadInstMap(void)
{
    char            instLineBuffer[101];
    char            instBuff[27];
    jwHashTable*    table = NULL;
    int             countOfInsts = 0;
    FILE*           instFile = NULL;

    memset(instBuff, 0, 27);
    memset(instLineBuffer, 0, 101);

    instFile = fopen("regtable.txt", "r");
    if(!instFile)
        return NULL;

    /* Count available instruments first */
    while( readInstLine(instFile, instLineBuffer) != -1 )
        countOfInsts++;

    if(countOfInsts > 0)
    {
        int lineLen;
        fseek(instFile, 0, SEEK_SET);
        table = create_hash( (size_t)countOfInsts + 5 );
        while(countOfInsts > 0)
        {
            lineLen = readInstLine(instFile, instLineBuffer);

            if(lineLen < 0)
                break;

            if(lineLen >= 26)
            {
                int patchID = atoi(instLineBuffer + 23);
                memset(instBuff, 0, 27);
                memcpy(instBuff, instLineBuffer, 22);
                add_int_by_str(table, instBuff, (long)patchID);
            }
            countOfInsts--;
        }
    }
    fclose(instFile);

    return table;
}

static uint8_t detectPatch(jwHashTable*table, struct AdLibInstrument *inst, int log)
{
    char instBuff[27];
    int val = 0;

    sprintf(instBuff,
           "%02X%02X"
           "%02X%02X"
           "%02X%02X"
           "%02X%02X"
           "%02X"
           "%02X%02X"
           ,
            inst->reg20[0], inst->reg20[1],
           (inst->reg40[0] & 0xC0), inst->reg40[1],
            inst->reg60[0], inst->reg60[1],
            inst->reg60[0], inst->reg60[1],
            inst->regC0,
            inst->regE0[0], inst->regE0[1]
           );

    if(get_int_by_str(table, instBuff, &val) == HASHOK)
    {
        if(log)
            printf("Detected instrument %03d\n", val);
        return (uint8_t)(val % 128);
    } else {
        val = rand() % 128;
        if(log)
            printf("INSTRUMENT NOT FOUND, USING RANDOM %03d\n", val);
    }

    return rand() % 128;
}

static void printInst(struct AdLibInstrument *inst, uint8_t channel, int log, FILE* inst_log)
{
    if(inst_log)
    {
        fprintf(inst_log,
               "%02X%02X"
               "%02X%02X"
               "%02X%02X"
               "%02X%02X"
               "%02X"
               "%02X%02X|%03d\n"
               ,
                inst->reg20[0], inst->reg20[1],
               (inst->reg40[0] & 0xC0), inst->reg40[1],
                inst->reg60[0], inst->reg60[1],
                inst->reg60[0], inst->reg60[1],
                inst->regC0,
                inst->regE0[0], inst->regE0[1],
                (int)channel
               );
    }

    if(log)
    {
        printf("%d) "
               "20:[%02X %02X]; "
               "40:[%02X %02X]; "
               "60:[%02X %02X]; "
               "80:[%02X %02X]; "
               "C0:[%02X]; "
               "E0:[%02X %02X]\n"
               ,
                (int)channel,
                inst->reg20[0], inst->reg20[1],
               (inst->reg40[0] & 0xC0), inst->reg40[1],
                inst->reg60[0], inst->reg60[1],
                inst->reg60[0], inst->reg60[1],
                inst->regC0,
                inst->regE0[0], inst->regE0[1]
               );
    }
}
/*****************************************************************/


/*****************************************************************
 *                      Helper functions                         *
 *****************************************************************/
static void makePitch(/*FILE* f*/uint16_t *pitchs, /*struct Imf2MIDI_CVT *cvt,*/ int16_t freq, uint8_t channel)
{
    int16_t nextfreqIndex = nearestFreq((uint16_t)freq);
    int16_t nextfreq = (int16_t)note_frequencies[nextfreqIndex];

    if(nextfreq == freq)
    {
        /* Default state */
        /*MIDI_writePitchEvent(f, cvt, cvt->midi_mapchannel[channel], MIDI_PITCH_CENTER);*/
        pitchs[channel] = MIDI_PITCH_CENTER;
        return;
    }

    if(freq == 0)
        return;

    if(nextfreq > freq)
    {
        /* Pitch Up */
        /* Two half-tones up */
        int16_t freqR = relativeFreq(nextfreqIndex, 2);
        if(freqR >= 0)
        {
            /* pitch relative */
            pitchs[channel] = (uint16_t)(MIDI_PITCH_CENTER
                            + (0x2000 * (((double)freq - (double)nextfreq) / ((double)freqR - (double)nextfreq))) );
        }
    }
    else
    {
        /* Pitch down */
        /* Two half-tones down */
        int16_t freqR = relativeFreq(nextfreqIndex, -2);
        if(freqR >= 0)
        {
            /* pitch relative */
            pitchs[channel] = (uint16_t)(MIDI_PITCH_CENTER
                                         - (0x2000 * (((double)nextfreq - (double)freq) / ((double)nextfreq - (double)freqR))) );
        }
    }
}
/*****************************************************************/



void Imf2MIDI_init(struct Imf2MIDI_CVT *cvt)
{
    size_t i = 0;

    if(!cvt)
        return;

    memset(cvt->imf_instruments,     0, sizeof(cvt->imf_instruments));
    memset(cvt->imf_instrumentsPrev, 0, sizeof(cvt->imf_instrumentsPrev));
    memset(cvt->midi_mapchannel,     0, sizeof(cvt->midi_mapchannel));
    memset(cvt->midi_lastpatch,      0, sizeof(cvt->midi_lastpatch));
    memset(cvt->midi_lastpitch,      0, sizeof(cvt->midi_lastpitch));

    cvt->midi_resolution    = 384;
    cvt->midi_tempo         = 110.0;

    for(i = 0; i < 9; i++)
        cvt->midi_lastpitch[i] = MIDI_PITCH_CENTER;

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

    cvt->flag_usePitch = 1;
    cvt->flag_logInstruments = 0;
}

int Imf2MIDI_process(struct Imf2MIDI_CVT* cvt, int log)
{
    int      res = 1;
    char    *path_out = NULL;

    FILE    *file_in  = NULL;
    FILE    *file_out = NULL;
    const char* inst_log_name = "instlog.txt";
    FILE    *inst_log = NULL;

    uint8_t  c;
    uint8_t  imf_buff[4];
    int      imf_insChange[9];
    uint32_t imf_length = 0;
    uint16_t imf_delay  = 0;
    uint16_t imf_freq[9];
    uint8_t  imf_octs[9];
    uint8_t  imf_key_st[9];
    uint8_t  imf_key_st_prev[9];

    /* Array of pressed keys which allows to mute a pitched or toggled notes without powering off */
    uint8_t  imf_keys[9];
    uint8_t  imf_keys_prev[9];

    uint16_t imf_pitchs[9] =
    {
        MIDI_PITCH_CENTER, MIDI_PITCH_CENTER, MIDI_PITCH_CENTER,
        MIDI_PITCH_CENTER, MIDI_PITCH_CENTER, MIDI_PITCH_CENTER,
        MIDI_PITCH_CENTER, MIDI_PITCH_CENTER, MIDI_PITCH_CENTER,
    };
    uint16_t imf_pitchs_prev[9] =
    {
        MIDI_PITCH_CENTER, MIDI_PITCH_CENTER, MIDI_PITCH_CENTER,
        MIDI_PITCH_CENTER, MIDI_PITCH_CENTER, MIDI_PITCH_CENTER,
        MIDI_PITCH_CENTER, MIDI_PITCH_CENTER, MIDI_PITCH_CENTER,
    };
    uint8_t  imf_channel = 0;
    uint8_t  imf_regKey = 0;
    uint8_t  imf_regVal = 0;

    jwHashTable *inst_table = NULL;

    memset(imf_buff, 0, sizeof(imf_buff));
    memset(imf_insChange, 0, sizeof(imf_insChange));
    memset(imf_freq, 0, sizeof(imf_freq));
    memset(imf_octs, 0, sizeof(imf_octs));

    memset(imf_key_st, 0, sizeof(imf_key_st));
    memset(imf_key_st_prev, 0, sizeof(imf_key_st_prev));
    memset(imf_keys, 0, sizeof(imf_keys));
    memset(imf_keys_prev, 0, sizeof(imf_keys_prev));

    if(!cvt)
        return res;

    /* Calculate target path */
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
        fprintf(stderr, "\x1b[31mERROR:\x1b[0m File names are must not be same!\n\n");
        goto quit;
    }

    inst_table = loadInstMap();

    if(log)
    {
        printf("=============================\n"
               "Convert into \"%s\"\n"
               "=============================\n\n", cvt->path_out);

        if(!cvt->flag_usePitch)
            printf("-- Pitch detection is disabled --\n");

        if(inst_table)
            printf("-- Found an instrument detection table! --\n");
    }

    file_in  = fopen(cvt->path_in, "rb");
    file_out = fopen(cvt->path_out, "wb");
    if(cvt->flag_logInstruments)
        inst_log = fopen(inst_log_name, "a");

    if(!file_in)
    {
        fprintf(stderr, "\x1b[31mERROR:\x1b[0m Can't open file %s for read!\n\n", cvt->path_in);
        goto quit;
    }

    if(!file_out)
    {
        fprintf(stderr, "\x1b[31mERROR:\x1b[0m Can't open file %s for write!\n\n", cvt->path_out);
        goto quit;
    }

    imf_length = readLE32(file_in);
    if(imf_length == 0)
    {
        fprintf(stderr, "\x1b[31mERROR:\x1b[0m Failed to read IMF length!\n\n");
        goto quit;
    }

    imf_length -= 4;

    MIDI_writeHead(file_out, cvt);
    MIDI_beginTrack(file_out, cvt);
    MIDI_writeTempoEvent(file_out, cvt, (uint32_t)(60000000.0 / cvt->midi_tempo));
    MIDI_writeMetricKeyEvent(file_out, cvt, 4, 4, 24, 8);

    for(c = 0; c < 9; c++)
    {
        imf_freq[c] = 0;
        imf_octs[c] = 0;
        cvt->midi_mapchannel[c] = c;
    }

    for(c = 0; c <= 8; c++)
    {
        MIDI_writeControlEvent(file_out, cvt, c, MIDI_CONTROLLER_VOLUME, 127);
        cvt->midi_lastpatch[c] = c;
    }

    while(imf_length > 0)
    {
        imf_length -= 4;
        if(fread(imf_buff, 1, 4, file_in) != 4)
        {
            fprintf(stderr, "\x1b[31mWARNING:\x1b[0m IMF length is longer than file itself!\n\n");
            break; /* File end*/
        }

        imf_delay   = (imf_buff[0] & 0x00FF) | ((imf_buff[1]<<8) & 0xFF00);
        imf_regKey  =  imf_buff[2];
        imf_regVal  =  imf_buff[3];

        if((imf_delay > 0) || (imf_length == 0))
        {
            /*Store note events*/
            for(c = 0; c <= 8; c++)
            {
                uint8_t noteKey = 0, multL, multH, wsL, wsH;

                multL   = cvt->imf_instruments[c].reg20[0] & 0x0F;
                multH   = cvt->imf_instruments[c].reg20[1] & 0x0F;
                wsL     = cvt->imf_instruments[c].regE0[0] & 0x07;
                wsH     = cvt->imf_instruments[c].regE0[1] & 0x07;

                imf_keys[c] = hzToKey(imf_freq[c], imf_octs[c],
                                  multL, multH,
                                  wsL, wsH);

                if(cvt->flag_usePitch && noteKey && imf_keys[imf_channel])
                    makePitch(imf_pitchs, (int16_t)imf_freq[c], imf_channel);

                if( (imf_key_st[c] != imf_key_st_prev[c]) ||
                    (imf_keys[c] != imf_keys_prev[c]))
                {
                    if(imf_key_st[c])
                    {
                        struct AdLibInstrument* inst1 = &cvt->imf_instruments[c];
                        struct AdLibInstrument* inst2 = &cvt->imf_instrumentsPrev[c];
                        uint8_t velLevel = cvt->imf_instruments[c].reg40[0] & 0x3F;

                        if((imf_insChange[c]) && (instcmp(inst1 ,inst2) != 0) )
                        {
                            uint8_t patch;
                            printInst(inst1, imf_channel, log, inst_log);
                            if(inst_table)
                                patch = detectPatch(inst_table, inst1, log);
                            else
                                patch = rand() % 128;
                            MIDI_writePatchChangeEvent(file_out, cvt, cvt->midi_mapchannel[imf_channel], patch);
                            memcpy(inst2, inst1, sizeof(struct AdLibInstrument));
                            imf_insChange[imf_channel] = 0;
                        }

                        if(velLevel > (cvt->imf_instruments[c].reg40[1] & 0x3F))
                            velLevel = cvt->imf_instruments[c].reg40[1] & 0x3F;
                        if(imf_keys_prev[c] != 0)/* Mute note in channel if already pressed! */
                            MIDI_writeNoteOffEvent(file_out, cvt, cvt->midi_mapchannel[c], imf_keys_prev[c], 0);

                        if((cvt->flag_usePitch) && (imf_pitchs[c] != imf_pitchs_prev[c]))
                        {
                            MIDI_writePitchEvent(file_out, cvt, cvt->midi_mapchannel[c], imf_pitchs[c]);
                            imf_pitchs_prev[c] = imf_pitchs[c];
                        }

                        MIDI_writeNoteOnEvent(file_out, cvt, cvt->midi_mapchannel[c], imf_keys[c], ((0x3f - velLevel) << 1) & 0xFF);
                    } else {
                        if( imf_keys_prev[c] != 0)
                            MIDI_writeNoteOffEvent(file_out, cvt, cvt->midi_mapchannel[c], imf_keys_prev[c], 0);
                        imf_keys[c] = 0;
                    }

                    imf_key_st_prev[c] = imf_key_st[c];
                    imf_keys_prev[c] = imf_keys[c];
                }
            }

            /*Store pitch change events*/
            for(c = 0; c <= 8; c++)
            {
                makePitch(imf_pitchs, (int16_t)imf_freq[c], imf_channel);
                if((cvt->flag_usePitch) && (imf_pitchs[c] != imf_pitchs_prev[c]))
                {
                    MIDI_writePitchEvent(file_out, cvt, cvt->midi_mapchannel[c], imf_pitchs[c]);
                    imf_pitchs_prev[c] = imf_pitchs[c];
                }
            }

            /*Drop all captured events of this moment!*/
            MIDI_addDelta(cvt, imf_delay);
        }

        if((imf_regKey >= 0xA0) && (imf_regKey <= 0xA8))
        {
            imf_channel = imf_regKey - 0xA0;
            imf_freq[imf_channel] = (imf_freq[imf_channel] & 0x0F00) | (imf_regVal & 0xFF);
            continue;
        }

        if((imf_regKey >= 0xB0) && (imf_regKey <= 0xB8))
        {
            uint8_t isKeyOn = (imf_regVal >> 5) & 1;
            uint8_t /*noteKey = 0,*/ multL, multH, wsL, wsH /*, velLevel*/;

            multL   = cvt->imf_instruments[c].reg20[0] & 0x0F;
            multH   = cvt->imf_instruments[c].reg20[1] & 0x0F;
            wsL     = cvt->imf_instruments[c].regE0[0] & 0x07;
            wsH     = cvt->imf_instruments[c].regE0[1] & 0x07;

            imf_keys[c] = hzToKey(imf_freq[c], imf_octs[c],
                                  multL, multH,
                                  wsL, wsH);

            imf_channel = imf_regKey - 0xB0;
            imf_freq[imf_channel] = (imf_freq[imf_channel] & 0x00FF) | (uint16_t)((imf_regVal & 0x03) << 0x08);
            imf_octs[imf_channel] = (imf_regVal >> 0x02) & 0x07;

            if(isKeyOn)
            {
                if(imf_key_st_prev[imf_channel] && !imf_key_st[imf_channel])
                    imf_key_st_prev[imf_channel] = 0;
            }
            imf_key_st[imf_channel] = isKeyOn;

            /*
             * TODO: Add calculation of velocity for short notes which making expression
             * based on attack and sustain difference
             */
            continue;
        }

        if((imf_regKey >= 0x20) && (imf_regKey <= 0x35))
        {
            imf_channel = opl2_opChannel[(imf_regKey - 0x20) % 0x15];
            cvt->imf_instruments[imf_channel].reg20[opl2_op[(imf_regKey - 0x20) % 0x15]] = imf_regVal;
            imf_insChange[imf_channel] = 1;
            continue;
        }

        if((imf_regKey >= 0x40) && (imf_regKey <= 0x55))
        {
            uint8_t op = opl2_op[(imf_regKey - 0x40) % 0x15];
            uint8_t oldOp1 = cvt->imf_instruments[imf_channel].reg40[0];
            imf_channel = opl2_opChannel[(imf_regKey - 0x40) % 0x15];
            cvt->imf_instruments[imf_channel].reg40[op] = imf_regVal;
            /* Don't notify about changed instrument on volume change */
            if((0 == op) && ((oldOp1 & 0xC0) != (imf_regVal & 0xC0)))
                imf_insChange[imf_channel] = 1;
            continue;
        }

        if((imf_regKey >= 0x60) && (imf_regKey <= 0x75))
        {
            imf_channel = opl2_opChannel[(imf_regKey - 0x60) % 0x15];
            cvt->imf_instruments[imf_channel].reg60[opl2_op[(imf_regKey - 0x60) % 0x15]] = imf_regVal;
            imf_insChange[imf_channel] = 1;
            continue;
        }

        if((imf_regKey >= 0x80) && (imf_regKey <= 0x95))
        {
            imf_channel = opl2_opChannel[(imf_regKey - 0x80) % 0x15];
            cvt->imf_instruments[imf_channel].reg80[opl2_op[(imf_regKey - 0x80) % 0x15]] = imf_regVal;
            imf_insChange[imf_channel] = 1;
            continue;
        }

        if((imf_regKey >= 0xC0) && (imf_regKey <= 0xC8))
        {
            imf_channel = imf_regKey - 0xC0;
            cvt->imf_instruments[imf_channel].regC0 = imf_regVal;
            imf_insChange[imf_channel] = 1;
            continue;
        }

        if((imf_regKey >= 0xE0) && (imf_regKey <= 0xF5))
        {
            imf_channel = opl2_opChannel[(imf_regKey - 0xE0) % 0x15];
            cvt->imf_instruments[imf_channel].regE0[opl2_op[(imf_regKey - 0xE0) % 0x15]] = imf_regVal;
            imf_insChange[imf_channel] = 1;
            continue;
        }
    }

    /* Shut-up all stay-on notes */
    for(c = 0; c <= 8; c++)
    {
        if(imf_keys[c] != 0)
            MIDI_writeNoteOffEvent(file_out, cvt, cvt->midi_mapchannel[c], imf_keys[c], 0);
    }

    MIDI_endTrack(file_out, cvt);
    MIDI_closeHead(file_out, cvt);

    if(log)
    {
        printf("=============================\n"
               "   Work has been completed!\n"
               "=============================\n\n");
    }

    res = 0;

quit:
    if(file_in)
        fclose(file_in);

    if(file_out)
        fclose(file_out);

    if(inst_log)
        fclose(inst_log);

    if(inst_table)
        delete_hash(inst_table);

    if(path_out)
    {
        free(path_out);
        path_out = NULL;
        cvt->path_out = NULL;
    }

    return res;
}

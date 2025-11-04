#include "basic.h"

#define BYTENUM 1
#define BYTESIZE 8
#define BYTEFLAG 0xFF

#define SHORTNUM 2
#define SHORTSIZE 16
#define SHORTFLAG 0xFFFF

#define INTBNUM 4
#define INTSIZE 32
#define INTFLAG 0xFFFFFFFF

#define LINTBNUM 8
#define LINTSIZE 64
#define LINTFLAG 0xFFFFFFFFFFFFFFFF



void printbit(char byte)
{
printf("%d%d%d%d%d%d%d%d",
  (byte & 0x80 ? 1 : 0), 
  (byte & 0x40 ? 1 : 0), 
  (byte & 0x20 ? 1 : 0), 
  (byte & 0x10 ? 1 : 0), 
  (byte & 0x08 ? 1 : 0), 
  (byte & 0x04 ? 1 : 0), 
  (byte & 0x02 ? 1 : 0), 
  (byte & 0x01 ? 1 : 0) 
);
}

#ifndef inccurbit
#define inccurbit(bs) \
{ \
    bs->curbyte += (((bs->curbit + 1) & (15)) >> 3); \
    bs->curbit = (bs->curbit + 1) & 7; \
}
#endif

void stream_init(bitstream_t *bs,byte *buf, int size)
{
    bs->bufsize = size;
    bs->buf = buf;
    bs->curbyte = 0;
    bs->curbit = 0;
    bs->datalen = 0;
}

static void clearbytes(bitstream_t *bs, int inc)
{
    int start = bs->curbyte;
    if(bs->curbit)
        start += 1;
    for(int i = 0; i < inc; i++)
    {
        bs->buf[start + i] = 0;
    } 
}

void stream_writeBit(bitstream_t *bs, int val)
{
    if(!bs->curbit)
        clearbytes(bs, 1);
    bs->buf[bs->curbyte] |= ((val&0x1) << bs->curbit);
    inccurbit(bs);
}

int stream_readBit(bitstream_t* bs)
{
    int val = (bs->buf[bs->curbyte] & ( 1<<bs->curbit )) >> bs->curbit;
    inccurbit(bs);
    return val;
}

void stream_writeByte(bitstream_t* bs, unsigned char b)
{
    clearbytes(bs, 1);
    bs->buf[bs->curbyte] |= (b << bs->curbit);
    bs->curbyte++;
    if(bs->curbit) {
        bs->buf[bs->curbyte] = (b & BYTEFLAG) >> (BYTESIZE - bs->curbit);
    }
}

unsigned char stream_readByte(bitstream_t* bs)
{
    unsigned char val = 0;
    val = (bs->buf[bs->curbyte] & BYTEFLAG) >> bs->curbit;
    bs->curbyte++;
    if(bs->curbit) {
        val |= bs->buf[bs->curbyte] << ( BYTESIZE - bs->curbit );
    }
    return val;
}

void stream_writeInt(bitstream_t* bs, unsigned int b)
{
    clearbytes(bs, 4);
    *(unsigned int*)&bs->buf[bs->curbyte] |= b << bs->curbit;
    bs->curbyte += 4;
    if(bs->curbit) {
        bs->buf[bs->curbyte] |= (b & INTFLAG) >> (INTSIZE-bs->curbit);
    }
}

unsigned int stream_readInt(bitstream_t* bs)
{
    unsigned int val = ( (*( (unsigned int*) &bs->buf[bs->curbyte] )) & INTFLAG) >> bs->curbit;
    // inccurbyte(bs, 4);
    bs->curbyte += 4;
    if(bs->curbit) {
        val |= (bs->buf[bs->curbyte] << (INTSIZE - bs->curbit));
    }
    return val;
}

// void stream_writeintv(bitstream_t *bs, unsigned int b, int bits)
// {
//     int i = 0;
//     while (i < bits)
//     {
//         bs->buf[bs->curbyte] = (b>>i) << bs->curbit;
//         bs->curbit++;
//         inccurbit(
//         i++;
//     }
// }

void stream_writeLong(bitstream_t* bs, unsigned long int b)
{
    clearbytes(bs, 8);
    *(unsigned long int*)&bs->buf[bs->curbyte] |= b << bs->curbit;
    bs->curbyte += 8;
    if(bs->curbit) {
        bs->buf[bs->curbyte] |= (b & LINTFLAG) >> (LINTSIZE-bs->curbit);
    }
}

void stream_writeLongBits(bitstream_t* bs, unsigned long int b, int bits)
{
    float inc;
    inc = (int)CEIL(((float)bits)/8.0);
    clearbytes(bs, inc);

    for(int i = 0; i < bits; i++)
    {
        bs->buf[bs->curbyte] |= ((((b & (1ULL << i)) >> i) & 1) << bs->curbit);
        inccurbit(bs);
    }
}

unsigned long int stream_readLongBits(bitstream_t* bs, int bits)
{
    unsigned long int b = 0;
    
    for(int i = 0; i < bits; i++)
    {
        b |= ((((bs->buf[bs->curbyte] & (1 << bs->curbit)) >> bs->curbit) & 1) << i);
        inccurbit(bs);
    }
    return b;
}


void stream_writeVarLong(bitstream_t *bs, unsigned long int b)
{
    int i;
    unsigned long int sflag = BYTEFLAG;
    
    for(i = 1; i < 8; i++)
    {
        if(b > sflag) {
            stream_writeBit(bs, 1);
        }
        else {
            stream_writeBit(bs, 0);
            break;
        }
        sflag = (sflag << 8) | (BYTEFLAG);
    }
    stream_writeLongBits(bs, b, i*8);
}

unsigned long int stream_readVarLong(bitstream_t *bs)
{
    int numbytes = 1;
    byte bit = 0;
    unsigned long int val;

    while(numbytes < 8)
    {
        bit = stream_readBit(bs);
        if(bit == 0)
            break;
        numbytes++;
    }

    val = stream_readLongBits(bs, numbytes*8);
    return val;
}

unsigned int stream_readLong(bitstream_t* bs)
{
    unsigned long int val = ( *( (unsigned long int*) &bs->buf[bs->curbyte] ) & LINTFLAG) >> bs->curbit;
    bs->curbyte += 8;
    if(bs->curbit) {
        val |= bs->buf[bs->curbyte] << (LINTSIZE - bs->curbit);
    }
    return val;
}

void stream_writeDouble(bitstream_t* bs, long double f)
{
    unsigned bits=32;
    unsigned expbits=8;
    long double fnorm;
    int shift;
    long long sign, exp, significand;
    unsigned significandbits = bits - expbits - 1; // -1 for sign bit

    if (f == 0.0) stream_writeLong(bs,0); // get this special case out of the way

    // check sign and begin normalization
    if (f < 0) { sign = 1; fnorm = -f; }
    else { sign = 0; fnorm = f; }

    // get the normalized form of f and track the exponent
    shift = 0;
    while(fnorm >= 2.0) { fnorm /= 2.0; shift++; }
    while(fnorm < 1.0) { fnorm *= 2.0; shift--; }
    fnorm = fnorm - 1.0;

    // calculate the binary form (non-float) of the significand data
    significand = fnorm * ((1LL<<significandbits) + 0.5f);

    // get the biased exponent
    exp = shift + ((1<<(expbits-1)) - 1); // shift + bias

    // return the final answer
    // return (sign<<(bits-1)) | (exp<<(bits-expbits-1)) | significand;
    stream_writeLong(bs, (sign<<(bits-1)) | (exp<<(bits-expbits-1)) | significand);
}

long double stream_readDouble(bitstream_t* bs)
{
    unsigned long int i = stream_readLong(bs);
    unsigned bits=32;
    unsigned expbits=8;
    long double result;
    long long shift;
    unsigned bias;
    unsigned significandbits = bits - expbits - 1; // -1 for sign bit

    if (i == 0) return 0.0;

    // pull the significand
    result = (i&((1LL<<significandbits)-1)); // mask
    result /= (1LL<<significandbits); // convert back to float
    result += 1.0f; // add the one back on

    // deal with the exponent
    bias = (1<<(expbits-1)) - 1;
    shift = ((i>>significandbits)&((1LL<<expbits)-1)) - bias;
    while(shift > 0) { result *= 2.0; shift--; }
    while(shift < 0) { result /= 2.0; shift++; }

    // sign it
    result *= (i>>(bits-1))&1? -1.0: 1.0;

    return result;
}

void stream_writeString(bitstream_t *bs, byte *str, int len)
{
    stream_writeInt(bs, len);
    for(int i = 0; i < len - 1; i++)
        stream_writeByte(bs, str[i]);
    stream_writeByte(bs, '\0');
}

void stream_readString(bitstream_t *bs, byte *str, int *len)
{
    int i = 0;
    int slen = stream_readInt(bs);
    if((slen + bs->curbyte) > bs->bufsize)
    {
        com_error(ERR_FATAL, "ERROR: reading string exceeds the size of stream buffer\n");
    }
    for(int i = 0; i < slen; i++)
    {
        str[i] = stream_readByte(bs);
    }
    *len = i;
}

void stream_writeData(bitstream_t *bs, byte *data, int len)
{
    for(int i = 0; i < len; i++)
    {
        stream_writeByte(bs, data[i]);
    }
}

qbool stream_isWritten(bitstream_t *bs)
{
    if(bs->curbyte || bs->curbit)
        return qtrue;
    return qfalse;
}

void stream_writeBitsData(bitstream_t *bs, byte *data, int bitLen)
{
    int bigLen = bitLen / 8;
    int smallLen = bitLen % 8;
    int i;
    for(i = 0; i < bigLen; i++)
    {
        stream_writeLongBits(bs, data[i], 8);
    }
    if(smallLen)
        stream_writeLongBits(bs, data[i], smallLen);
}

void stream_copyBitsData(bitstream_t *bs, byte *data, int bitLen)
{
    int bigLen = bitLen / 8;
    int smallLen = bitLen % 8;
    bitstream_t databs;

    int ceilLen = bigLen;
    if(smallLen)
        ceilLen += 1;

    stream_init(&databs, data, ceilLen);

    for(int i = 0; i < bigLen; i++)
    {
        stream_writeByte(&databs, stream_readByte(bs));
    }
    if(smallLen)
        stream_writeLongBits(&databs, stream_readLongBits(bs, smallLen), smallLen);
}

void stream_skipBits(bitstream_t *bs, int bitLen)
{
    bs->curbyte += bitLen/8;
    bs->curbit += bitLen%8;
}

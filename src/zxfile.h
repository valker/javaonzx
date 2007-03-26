/* project : javaonzx
   file    : zxfile.h
   purpose : file operation support on ZX Spectrum 128
   author  : valker
*/


#ifndef ZXFILE_H_INCLUDED
#define ZXFILE_H_INCLUDED

#include <stdio.h>

#ifdef ZX

#define FILE_BUFFER_SIZE 256

typedef struct {
    BYTES_FAR   buffer_;        // internal buffer 
    PSTR_FAR    filename_;
    u4          fileSize_;      // total size of the file
    u4          filePos_;       // current position in the file
} FILE;

#define SEEK_CUR    1
#define SEEK_END    2
#define SEEK_SET    0

i2 getc(FILE * File);
i2 fseek(FILE * File, i2 Offset, i1 Origin);
i2 fread(void * DstBuf, u1 ElementSize, u2 Count, FILE * File);
u2 ftell(FILE * _File);

#endif

non_banked i2 hfread(far_ptr DstBuf, u1 ElementSize, u2 Count, FILE * File);


#endif
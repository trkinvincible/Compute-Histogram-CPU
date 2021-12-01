
#include "../hdr/gzio.h"
#include <memory>
#include <cstring>

/* default memLevel */
#if MAX_MEM_LEVEL >= 8
#  define _NRRD_DEF_MEM_LEVEL 8
#else
#  define _NRRD_DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif

/* stream buffer size */
#define _NRRD_Z_BUFSIZE 16 * 1024

/* gzip flag byte */
#define _NRRD_ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define _NRRD_HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define _NRRD_EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define _NRRD_ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define _NRRD_COMMENT      0x10 /* bit 4 set: file comment present */
#define _NRRD_RESERVED     0xE0 /* bits 5..7: reserved */

typedef struct _NrrdGzStream {
  z_stream stream;
  int      z_err;   /* error code for last stream operation */
  int      z_eof;   /* set if end of input file */
  FILE     *file;   /* .gz file */
  Byte     *inbuf;  /* input buffer */
  Byte     *outbuf; /* output buffer */
  uLong    crc;     /* crc32 of uncompressed data */
  char     *msg;    /* error message */
  int      transparent; /* 1 if input file is not a .gz file */
  char     mode;    /* 'w' or 'r' */
  long     startpos; /* start of compressed data in file (header skipped) */
} _NrrdGzStream;

static int _nrrdGzMagic[2] = {0x1f, 0x8b}; /* gzip magic header */

/* zlib error messages */
static const char *_nrrdGzErrMsg[10] = {
  "need dictionary",     /* Z_NEED_DICT       2  */
  "stream end",          /* Z_STREAM_END      1  */
  "",                    /* Z_OK              0  */
  "file error",          /* Z_ERRNO         (-1) */
  "stream error",        /* Z_STREAM_ERROR  (-2) */
  "data error",          /* Z_DATA_ERROR    (-3) */
  "insufficient memory", /* Z_MEM_ERROR     (-4) */
  "buffer error",        /* Z_BUF_ERROR     (-5) */
  "incompatible version",/* Z_VERSION_ERROR (-6) */
  ""};

#define _NRRD_GZ_ERR_MSG(err) _nrrdGzErrMsg[Z_NEED_DICT-(err)]

/* some forward declarations for things in this file */
static void _nrrdGzCheckHeader(_NrrdGzStream *s);
static int _nrrdGzDestroy(_NrrdGzStream *s);
static int _nrrdGzDoFlush(gzFile file, int flush);
static void _nrrdGzPutLong(FILE *file, uLong x);
static uLong _nrrdGzGetLong(_NrrdGzStream *s);

void* SafeFree(void *ptr) {

  if (ptr) {
    free(ptr);
  }
  return NULL;
}

gzFile _nrrdGzOpen(FILE* fd, const char* mode) {
  static const char me[]="_nrrdGzOpen";
  int error;
  int level = Z_DEFAULT_COMPRESSION; /* compression level */
  int strategy = Z_DEFAULT_STRATEGY; /* compression strategy */
  const char *p = mode;
  _NrrdGzStream *s;
  char fmode[257]; /* copy of mode, without the compression level */
  char *m = fmode;

  if (!mode) {
    return Z_NULL;
  }
  /* allocate stream struct */
  s = (_NrrdGzStream *)calloc(1, sizeof(_NrrdGzStream));
  if (!s) {
    return Z_NULL;
  }
  /* initialize stream struct */
  s->stream.zalloc = (alloc_func)0;
  s->stream.zfree = (free_func)0;
  s->stream.opaque = (voidpf)0;
  s->stream.next_in = s->inbuf = Z_NULL;
  s->stream.next_out = s->outbuf = Z_NULL;
  s->stream.avail_in = s->stream.avail_out = 0;
  s->file = NULL;
  s->z_err = Z_OK;
  s->z_eof = 0;
  s->crc = crc32(0L, Z_NULL, 0);
  s->msg = NULL;
  s->transparent = 0;
  /* parse mode flag */
  s->mode = '\0';
  do {
    if (*p == 'r') s->mode = 'r';
    if (*p == 'w' || *p == 'a') s->mode = 'w';
    if (*p >= '0' && *p <= '9') {
      level = *p - '0';
    } else if (*p == 'f') {
      strategy = Z_FILTERED;
    } else if (*p == 'h') {
      strategy = Z_HUFFMAN_ONLY;
    } else {
      *m++ = *p; /* copy the mode */
    }
  } while (*p++ && m != fmode + sizeof(fmode));
  if (s->mode == '\0') {
    return _nrrdGzDestroy(s), (gzFile)Z_NULL;
  }

  if (s->mode == 'w') {
    error = deflateInit2(&(s->stream), level,
                         Z_DEFLATED, -MAX_WBITS, _NRRD_DEF_MEM_LEVEL,
                         strategy);
    /* windowBits is passed < 0 to suppress zlib header */

    s->stream.next_out = s->outbuf = (Byte*)calloc(1, _NRRD_Z_BUFSIZE);
    if (error != Z_OK || s->outbuf == Z_NULL) {
      return _nrrdGzDestroy(s), (gzFile)Z_NULL;
    }
  } else {
    s->stream.next_in  = s->inbuf = (Byte*)calloc(1, _NRRD_Z_BUFSIZE);

    error = inflateInit2(&(s->stream), -MAX_WBITS);
    /* windowBits is passed < 0 to tell that there is no zlib header.
     * Note that in this case inflate *requires* an extra "dummy" byte
     * after the compressed stream in order to complete decompression and
     * return Z_STREAM_END. Here the gzip CRC32 ensures that 4 bytes are
     * present after the compressed stream.
     */
    if (error != Z_OK || s->inbuf == Z_NULL) {
      return _nrrdGzDestroy(s), (gzFile)Z_NULL;
    }
  }
  s->stream.avail_out = _NRRD_Z_BUFSIZE;
  errno = 0;
  s->file = fd;
  if (s->file == NULL) {
    return _nrrdGzDestroy(s), (gzFile)Z_NULL;
  }
  _nrrdGzCheckHeader(s); /* skip the .gz header */
  s->startpos = (ftell(s->file) - s->stream.avail_in);

  return (gzFile)s;
}

/*
** _nrrdGzClose()
**
** Flushes all pending output if necessary, closes the compressed file
** and deallocates the (de)compression state.
*/
int
_nrrdGzClose (gzFile file) {
  static const char me[]="_nrrdGzClose";
  int error;
  _NrrdGzStream *s = (_NrrdGzStream*)file;

  if (s == NULL) {
    return 1;
  }
  if (s->mode == 'w') {
    error = _nrrdGzDoFlush(file, Z_FINISH);
    if (error != Z_OK) {
      return _nrrdGzDestroy((_NrrdGzStream*)file);
    }
    _nrrdGzPutLong(s->file, s->crc);
    _nrrdGzPutLong(s->file, s->stream.total_in);
  }
  return _nrrdGzDestroy((_NrrdGzStream*)file);
}

/*
** _nrrdGzRead()
**
** Reads the given number of uncompressed bytes from the compressed file.
** Returns the number of bytes actually read (0 for end of file).
*/
int
_nrrdGzRead(gzFile file, void* buf, unsigned int len, unsigned int* didread) {
  static const char me[]="_nrrdGzRead";
  _NrrdGzStream *s = (_NrrdGzStream*)file;
  Bytef *start = (Bytef*)buf; /* starting point for crc computation */
  Byte  *next_out; /* == stream.next_out but not forced far (for MSDOS) */

  if (s == NULL || s->mode != 'r') {
    *didread = 0;
    return 1;
  }

  if (s->z_err == Z_DATA_ERROR || s->z_err == Z_ERRNO) {
    *didread = 0;
    return 1;
  }

  if (s->z_err == Z_STREAM_END) {
    *didread = 0;
    return 0;  /* EOF */
  }

  next_out = (Byte*)buf;
  s->stream.next_out = (Bytef*)buf;
  s->stream.avail_out = len;

  while (s->stream.avail_out != 0) {

    if (s->transparent) {
      /* Copy first the lookahead bytes: */
      uInt n = s->stream.avail_in;
      if (n > s->stream.avail_out) n = s->stream.avail_out;
      if (n > 0) {
        memcpy(s->stream.next_out, s->stream.next_in, n);
        next_out += n;
        s->stream.next_out = next_out;
        s->stream.next_in   += n;
        s->stream.avail_out -= n;
        s->stream.avail_in  -= n;
      }
      if (s->stream.avail_out > 0) {
        s->stream.avail_out -= (uInt)fread(next_out, 1, s->stream.avail_out,
                                           s->file);
      }
      len -= s->stream.avail_out;
      s->stream.total_in  += len;
      s->stream.total_out += len;
      if (len == 0) s->z_eof = 1;
      *didread = len;
      return 0;
    }
    if (s->stream.avail_in == 0 && !s->z_eof) {

      s->stream.avail_in = (uInt)fread(s->inbuf, 1, _NRRD_Z_BUFSIZE, s->file);
      if (s->stream.avail_in == 0) {
        s->z_eof = 1;
        if (ferror(s->file)) {
          s->z_err = Z_ERRNO;
          break;
        }
      }
      s->stream.next_in = s->inbuf;
    }
    s->z_err = inflate(&(s->stream), Z_NO_FLUSH);

    if (s->z_err == Z_STREAM_END) {
      /* Check CRC and original size */
      s->crc = crc32(s->crc, start, (uInt)(s->stream.next_out - start));
      start = s->stream.next_out;

      if (_nrrdGzGetLong(s) != s->crc) {
        s->z_err = Z_DATA_ERROR;
      } else {
        (void)_nrrdGzGetLong(s);
        /* The uncompressed length returned by above getlong() may
         * be different from s->stream.total_out) in case of
         * concatenated .gz files. Check for such files:
         */
        _nrrdGzCheckHeader(s);
        if (s->z_err == Z_OK) {
          uLong total_in = s->stream.total_in;
          uLong total_out = s->stream.total_out;

          inflateReset(&(s->stream));
          s->stream.total_in = total_in;
          s->stream.total_out = total_out;
          s->crc = crc32(0L, Z_NULL, 0);
        }
      }
    }
    if (s->z_err != Z_OK || s->z_eof) break;
  }
  s->crc = crc32(s->crc, start, (uInt)(s->stream.next_out - start));

  *didread = len - s->stream.avail_out;
  return 0;
}

/*
** _nrrdGzGetByte()
**
** Reads a byte from a _NrrdGzStream. Updates next_in and avail_in.
** Returns EOF for end of file.
** IN assertion: the stream s has been sucessfully opened for reading.
*/
static int
_nrrdGzGetByte(_NrrdGzStream *s) {
  static const char me[]="_nrrdGzGetByte";

  if (s->z_eof) return EOF;
  if (s->stream.avail_in == 0) {
    s->stream.avail_in = (uInt)fread(s->inbuf, 1, _NRRD_Z_BUFSIZE, s->file);
    if (s->stream.avail_in == 0) {
      s->z_eof = 1;
      if (ferror(s->file)) {
        s->z_err = Z_ERRNO;
      }
      return EOF;
    }
    s->stream.next_in = s->inbuf;
  }
  s->stream.avail_in--;
  return *(s->stream.next_in)++;
}

/*
******** _nrrdGzCheckHeader()
**
** Checks the gzip header of a _NrrdGzStream opened for reading. Sets
** the stream mode to transparent if the gzip magic header is not
** present; sets s->err to Z_DATA_ERROR if the magic header is present
** but the rest of the header is incorrect.
** IN assertion: the stream s has already been created sucessfully;
** s->stream.avail_in is zero for the first time, but may be non-zero
** for concatenated .gz files.
*/
static void
_nrrdGzCheckHeader(_NrrdGzStream *s) {
  static const char me[]="_nrrdGzCheckHeader";
  int method; /* method byte */
  int flags;  /* flags byte */
  uInt len;
  int c;

  /* Check the gzip magic header */
  for (len = 0; len < 2; len++) {
    c = _nrrdGzGetByte(s);
    if (c != _nrrdGzMagic[len]) {
      if (len != 0) s->stream.avail_in++, s->stream.next_in--;
      if (c != EOF) {
        s->stream.avail_in++, s->stream.next_in--;
        s->transparent = 1;
      }
      s->z_err = s->stream.avail_in != 0 ? Z_OK : Z_STREAM_END;
      return;
    }
  }
  method = _nrrdGzGetByte(s);
  flags = _nrrdGzGetByte(s);
  if (method != Z_DEFLATED || (flags & _NRRD_RESERVED) != 0) {
    s->z_err = Z_DATA_ERROR;
    return;
  }

  /* Discard time, xflags and OS code: */
  for (len = 0; len < 6; len++) (void)_nrrdGzGetByte(s);

  if ((flags & _NRRD_EXTRA_FIELD) != 0) { /* skip the extra field */
    len  =  (uInt)_nrrdGzGetByte(s);
    len += ((uInt)_nrrdGzGetByte(s))<<8;
    /* len is garbage if EOF but the loop below will quit anyway */
    while (len-- != 0 && _nrrdGzGetByte(s) != EOF) ;
  }
  if ((flags & _NRRD_ORIG_NAME) != 0) { /* skip the original file name */
    while ((c = _nrrdGzGetByte(s)) != 0 && c != EOF) ;
  }
  if ((flags & _NRRD_COMMENT) != 0) {   /* skip the .gz file comment */
    while ((c = _nrrdGzGetByte(s)) != 0 && c != EOF) ;
  }
  if ((flags & _NRRD_HEAD_CRC) != 0) {  /* skip the header crc */
    for (len = 0; len < 2; len++) (void)_nrrdGzGetByte(s);
  }
  s->z_err = s->z_eof ? Z_DATA_ERROR : Z_OK;
}

/*
** _nrrdGzDestroy()
**
** Cleans up then free the given _NrrdGzStream. Returns a zlib error code.
** Try freeing in the reverse order of allocations.  FILE* s->file is not
** closed.  Because we didn't allocate it, we shouldn't delete it.
*/
static int
_nrrdGzDestroy(_NrrdGzStream *s) {
  static const char me[]="_nrrdGzDestroy";
  int error = Z_OK;

  if (s == NULL) {
    return 1;
  }
  s->msg = (char*)SafeFree(s->msg);
  if (s->stream.state != NULL) {
    if (s->mode == 'w') {
      error = deflateEnd(&(s->stream));
    } else if (s->mode == 'r') {
      error = inflateEnd(&(s->stream));
    }
  }
  if (error != Z_OK) {
  }
  if (s->z_err < 0) error = s->z_err;
  if (error != Z_OK) {
  }
  s->inbuf = (Byte *)SafeFree(s->inbuf);
  s->outbuf = (Byte *)SafeFree(s->outbuf);
  SafeFree(s);   /* avoiding unused value warnings, no NULL set */
  return error != Z_OK;
}

/*
** _nrrdGzDoFlush()
**
** Flushes all pending output into the compressed file. The parameter
** flush is the same as in the deflate() function.
*/
static int
_nrrdGzDoFlush(gzFile file, int flush) {
  static const char me[]="_nrrdGzDoFlush";
  uInt len;
  int done = 0;
  _NrrdGzStream *s = (_NrrdGzStream*)file;

  if (s == NULL || s->mode != 'w') {
    return Z_STREAM_ERROR;
  }

  s->stream.avail_in = 0; /* should be zero already anyway */

  for (;;) {
    len = _NRRD_Z_BUFSIZE - s->stream.avail_out;

    if (len != 0) {
      if ((uInt)fwrite(s->outbuf, 1, len, s->file) != len) {
        s->z_err = Z_ERRNO;
        return Z_ERRNO;
      }
      s->stream.next_out = s->outbuf;
      s->stream.avail_out = _NRRD_Z_BUFSIZE;
    }
    if (done) break;
    s->z_err = deflate(&(s->stream), flush);

    /* Ignore the second of two consecutive flushes: */
    if (len == 0 && s->z_err == Z_BUF_ERROR) s->z_err = Z_OK;

    /* deflate has finished flushing only when it hasn't used up
     * all the available space in the output buffer:
     */
    done = (s->stream.avail_out != 0 || s->z_err == Z_STREAM_END);

    if (s->z_err != Z_OK && s->z_err != Z_STREAM_END) break;
  }
  return  s->z_err == Z_STREAM_END ? Z_OK : s->z_err;
}

/*
** _nrrdGzPutLong()
**
** Outputs a long in LSB order to the given file.
*/
static void
_nrrdGzPutLong(FILE* file, uLong x) {
  int n;
  for (n = 0; n < 4; n++) {
    fputc((int)(x & 0xff), file);
    x >>= 8;
  }
}

/*
** _nrrdGzGetLong()
**
** Reads a long in LSB order from the given _NrrdGzStream.
** Sets z_err in case of error.
*/
static uLong
_nrrdGzGetLong(_NrrdGzStream *s) {
  uLong x = (uLong)_nrrdGzGetByte(s);
  int c;

  x += ((uLong)_nrrdGzGetByte(s))<<8;
  x += ((uLong)_nrrdGzGetByte(s))<<16;
  c = _nrrdGzGetByte(s);
  if (c == EOF) s->z_err = Z_DATA_ERROR;
  x += ((uLong)c)<<24;
  return x;
}

/*
** random symbol to have in object file, even when Zlib not enabled
*/
int
_nrrdGzDummySymbol(void) {
  return 42;
}


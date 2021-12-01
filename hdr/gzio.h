#pragma once

#include <zlib.h> /* NrrdIO-hack-004 */
#include <stdio.h>

gzFile _nrrdGzOpen(FILE* fd, const char *mode);
int _nrrdGzClose(gzFile file);
int _nrrdGzRead(gzFile file, void* buf, unsigned int len,
                       unsigned int* read);

#pragma once

#include <zlib.h> /* NrrdIO-hack-004 */
#include <stdio.h>

gzFile GzOpen(FILE* fd, const char *mode);
int GzClose(gzFile file);
int GzRead(gzFile file, void* buf, unsigned int len,
                       unsigned int* read);

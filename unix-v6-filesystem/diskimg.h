/**
 * diskimg.h  --  raw block-device abstraction over a disk-image file.
 * The rest of the filesystem is layered strictly on top of these two calls
 * (plus open/close), exactly as in the original CS110 assignment.
 */
#ifndef _DISKIMG_H
#define _DISKIMG_H

#include "unixfilesystem.h"

/* Open a disk image; returns a file descriptor >=0 or -1 on error. */
int diskimg_open(const char *pathname, int readOnly);

/* Read one 512-byte sector `sectorNum` into `buf`. Returns bytes read or -1. */
int diskimg_readsector(int fd, int sectorNum, void *buf);

/* Write one 512-byte sector. Returns bytes written or -1. */
int diskimg_writesector(int fd, int sectorNum, const void *buf);

/* Close the image. Returns 0 on success, -1 on error. */
int diskimg_close(int fd);

#endif

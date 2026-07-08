#ifndef _FILE_H
#define _FILE_H

#include "unixfilesystem.h"

/**
 * file_getblock: read the `blockNum`-th (0-based) block of the file identified
 * by `inumber` into `buf` (must hold DISKIMG_SECTOR_SIZE bytes).
 * Returns the number of valid bytes in the block (the full 512 for every block
 * but the last, which returns size % 512, or 512 if the size is a multiple),
 * or -1 on error.
 */
int file_getblock(struct unixfilesystem *fs, int inumber, int blockNum, void *buf);

#endif

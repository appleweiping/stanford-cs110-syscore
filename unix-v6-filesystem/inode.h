#ifndef _INODE_H
#define _INODE_H

#include "unixfilesystem.h"

/**
 * inode_iget: fetch the inode with number `inumber` from `fs` into `*inp`.
 * Inodes are numbered from 1; inode 1 lives at the very start of the inode
 * region (block INODE_START_SECTOR). Returns 0 on success, -1 on error.
 */
int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp);

/**
 * inode_indexlookup: map the file-relative block number `blockNum` (0-based)
 * to its absolute sector number on disk, following the v6 small/large-file
 * addressing scheme. Returns the sector number, or -1 on error.
 */
int inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int blockNum);

/** Reassemble the 24-bit file size from i_size0 (high 8 bits) and i_size1. */
int inode_getsize(struct inode *inp);

#endif

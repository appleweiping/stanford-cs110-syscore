#include "file.h"
#include "inode.h"
#include "diskimg.h"
#include <stdio.h>

int file_getblock(struct unixfilesystem *fs, int inumber, int blockNum, void *buf) {
  struct inode in;
  if (inode_iget(fs, inumber, &in) < 0) return -1;

  int sector = inode_indexlookup(fs, &in, blockNum);
  if (sector <= 0) return -1;

  if (diskimg_readsector(fs->fd, sector, buf) != DISKIMG_SECTOR_SIZE)
    return -1;

  /* Compute how many bytes of this block are actually part of the file. */
  int size = inode_getsize(&in);
  int fullBlocks = size / DISKIMG_SECTOR_SIZE;
  if (blockNum < fullBlocks) return DISKIMG_SECTOR_SIZE;
  int tail = size % DISKIMG_SECTOR_SIZE;
  return tail == 0 ? DISKIMG_SECTOR_SIZE : tail;
}

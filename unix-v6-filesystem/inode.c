#include "inode.h"
#include "diskimg.h"
#include <stdio.h>
#include <string.h>

int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp) {
  if (inumber < 1) return -1;
  /* Inode `inumber` sits at a 32-byte offset within the inode region, which
   * starts at block INODE_START_SECTOR. There are 16 inodes per 512B block. */
  int index = inumber - 1;                       /* zero-based slot          */
  int sector = INODE_START_SECTOR + index / INODES_PER_BLOCK;
  int within = index % INODES_PER_BLOCK;

  struct inode block[INODES_PER_BLOCK];
  if (diskimg_readsector(fs->fd, sector, block) != DISKIMG_SECTOR_SIZE)
    return -1;
  memcpy(inp, &block[within], sizeof(struct inode));
  return 0;
}

int inode_getsize(struct inode *inp) {
  return ((int)inp->i_size0 << 16) | inp->i_size1;
}

/* Number of 16-bit block pointers that fit in one 512B indirect block. */
#define PTRS_PER_BLOCK (DISKIMG_SECTOR_SIZE / (int)sizeof(uint16_t)) /* 256 */

int inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int blockNum) {
  if (blockNum < 0) return -1;

  /* Small file: i_addr[0..7] point directly at up to 8 data blocks. */
  if (!(inp->i_mode & ILARG)) {
    if (blockNum >= 8) return -1;
    return inp->i_addr[blockNum];
  }

  /* Large file: i_addr[0..6] are singly-indirect blocks (256 pointers each);
   * i_addr[7] is doubly-indirect. */
  uint16_t indirect[PTRS_PER_BLOCK];

  if (blockNum < 7 * PTRS_PER_BLOCK) {
    int idx = blockNum / PTRS_PER_BLOCK;         /* which i_addr slot        */
    int off = blockNum % PTRS_PER_BLOCK;         /* offset inside that block */
    if (inp->i_addr[idx] == 0) return -1;
    if (diskimg_readsector(fs->fd, inp->i_addr[idx], indirect) != DISKIMG_SECTOR_SIZE)
      return -1;
    return indirect[off];
  }

  /* Doubly-indirect region via i_addr[7]. */
  int d = blockNum - 7 * PTRS_PER_BLOCK;
  int firstIdx = d / PTRS_PER_BLOCK;             /* pointer within L1 block  */
  int secondIdx = d % PTRS_PER_BLOCK;            /* pointer within L2 block  */
  if (firstIdx >= PTRS_PER_BLOCK) return -1;     /* beyond max file size     */

  if (inp->i_addr[7] == 0) return -1;
  if (diskimg_readsector(fs->fd, inp->i_addr[7], indirect) != DISKIMG_SECTOR_SIZE)
    return -1;
  uint16_t l1 = indirect[firstIdx];
  if (l1 == 0) return -1;
  if (diskimg_readsector(fs->fd, l1, indirect) != DISKIMG_SECTOR_SIZE)
    return -1;
  return indirect[secondIdx];
}

/**
 * diskimageaccess.c  --  walk a v6 disk image and dump the tree, in the spirit
 * of the CS110 assign2 `diskimageaccess` verification tool. It exercises every
 * layer: opens the image, reads the superblock, resolves paths, and for each
 * file streams its bytes through file_getblock, printing a checksum so output
 * is deterministic and comparable.
 *
 * Usage: diskimageaccess <image>
 */
#include "unixfilesystem.h"
#include "diskimg.h"
#include "inode.h"
#include "file.h"
#include "directory.h"
#include "pathname.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long checksum(const unsigned char *p, int n) {
  unsigned long h = 5381;                 /* djb2 */
  for (int i = 0; i < n; i++) h = ((h << 5) + h) + p[i];
  return h;
}

/* Recursively print a directory subtree rooted at `inumber` with `path`. */
static void walk(struct unixfilesystem *fs, int inumber, const char *path) {
  struct inode in;
  if (inode_iget(fs, inumber, &in) < 0) { printf("  <bad inode %d>\n", inumber); return; }

  if ((in.i_mode & IFMT) == IFDIR) {
    printf("DIR   %-24s (inode %d, %d bytes)\n", path, inumber, inode_getsize(&in));
    int size = inode_getsize(&in);
    int nblocks = (size + DISKIMG_SECTOR_SIZE - 1) / DISKIMG_SECTOR_SIZE;
    char buf[DISKIMG_SECTOR_SIZE];
    for (int b = 0; b < nblocks; b++) {
      int valid = file_getblock(fs, inumber, b, buf);
      if (valid < 0) continue;
      struct direntv6 *e = (struct direntv6 *)buf;
      int n = valid / (int)sizeof(struct direntv6);
      for (int i = 0; i < n; i++) {
        if (e[i].d_inumber == 0) continue;
        char name[MAX_COMPONENT_LENGTH + 1];
        strncpy(name, e[i].d_name, MAX_COMPONENT_LENGTH);
        name[MAX_COMPONENT_LENGTH] = '\0';
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        char child[512];
        snprintf(child, sizeof child, "%s%s%s",
                 path, (strcmp(path, "/") == 0 ? "" : "/"), name);
        walk(fs, e[i].d_inumber, child);
      }
    }
  } else {
    int size = inode_getsize(&in);
    int nblocks = (size + DISKIMG_SECTOR_SIZE - 1) / DISKIMG_SECTOR_SIZE;
    unsigned long h = 5381;
    char buf[DISKIMG_SECTOR_SIZE];
    int totalRead = 0;
    for (int b = 0; b < nblocks; b++) {
      int valid = file_getblock(fs, inumber, b, buf);
      if (valid < 0) { printf("  <read error blk %d>\n", b); break; }
      /* fold block checksum into running hash */
      for (int i = 0; i < valid; i++) h = ((h << 5) + h) + (unsigned char)buf[i];
      totalRead += valid;
    }
    printf("FILE  %-24s (inode %d, %d bytes, read %d, cksum %lu)\n",
           path, inumber, size, totalRead, h);
  }
}

int main(int argc, char **argv) {
  if (argc != 2) { fprintf(stderr, "usage: %s <image>\n", argv[0]); return 1; }
  struct unixfilesystem fs;
  fs.fd = diskimg_open(argv[1], 1);
  if (fs.fd < 0) { perror("open image"); return 1; }
  if (diskimg_readsector(fs.fd, 1, &fs.superblock) != DISKIMG_SECTOR_SIZE) {
    fprintf(stderr, "cannot read superblock\n"); return 1;
  }
  printf("superblock: s_isize=%d s_fsize=%d\n",
         fs.superblock.s_isize, fs.superblock.s_fsize);
  walk(&fs, ROOTINO, "/");
  (void)checksum;
  diskimg_close(fs.fd);
  return 0;
}

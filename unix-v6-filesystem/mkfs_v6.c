/**
 * mkfs_v6.c  --  build a genuine Unix Version 6 disk image from a manifest.
 *
 * The original CS110 assignment ships proprietary disk images. To make this
 * repo self-contained and reproducible, this tool writes out a real v6-format
 * image (same on-disk layout the reader consumes) that packs a small directory
 * tree, including a "large" file that exercises singly-indirect addressing.
 *
 * Layout produced:
 *   block 0        : boot block (zeroed)
 *   block 1        : superblock (struct filsys)
 *   blocks 2..N-1  : inode region (16 inodes/block)
 *   blocks N..     : data blocks
 *
 * Usage: mkfs_v6 <out.img>
 */
#include "unixfilesystem.h"
#include "diskimg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define NUM_INODE_BLOCKS 4                 /* 64 inodes is plenty for the demo */
#define DATA_START (INODE_START_SECTOR + NUM_INODE_BLOCKS)  /* first data blk  */
#define PTRS_PER_BLOCK (DISKIMG_SECTOR_SIZE / (int)sizeof(uint16_t))

static int g_fd;
static int g_nextData = DATA_START;        /* bump allocator for data blocks   */
static int g_nextInode = 1;                /* inode 1 reserved for root        */
static int g_maxBlock = 0;

static void wsector(int sec, const void *buf) {
  if (diskimg_writesector(g_fd, sec, buf) != DISKIMG_SECTOR_SIZE) {
    perror("write"); exit(1);
  }
  if (sec > g_maxBlock) g_maxBlock = sec;
}

static int alloc_data(void) { return g_nextData++; }
static int alloc_inode(void) { return ++g_nextInode; } /* returns 2,3,... */

/* Persist one inode into the inode region. */
static void put_inode(int inumber, struct inode *in) {
  int index = inumber - 1;
  int sector = INODE_START_SECTOR + index / INODES_PER_BLOCK;
  int within = index % INODES_PER_BLOCK;
  struct inode block[INODES_PER_BLOCK];
  char raw[DISKIMG_SECTOR_SIZE];
  if (diskimg_readsector(g_fd, sector, raw) != DISKIMG_SECTOR_SIZE) {
    memset(raw, 0, sizeof raw);
  }
  memcpy(block, raw, sizeof block);
  memcpy(&block[within], in, sizeof(struct inode));
  wsector(sector, block);
}

static void set_size(struct inode *in, int size) {
  in->i_size0 = (size >> 16) & 0xff;
  in->i_size1 = size & 0xffff;
}

/* Write file contents, filling i_addr (small or large). Returns inumber. */
static int make_file(int inumber, const char *data, int size) {
  struct inode in;
  memset(&in, 0, sizeof in);
  in.i_mode = IALLOC;             /* regular file */
  in.i_nlink = 1;
  set_size(&in, size);

  int nblocks = (size + DISKIMG_SECTOR_SIZE - 1) / DISKIMG_SECTOR_SIZE;
  if (nblocks == 0) nblocks = 0;

  if (nblocks <= 8) {
    for (int b = 0; b < nblocks; b++) {
      char buf[DISKIMG_SECTOR_SIZE];
      memset(buf, 0, sizeof buf);
      int chunk = size - b * DISKIMG_SECTOR_SIZE;
      if (chunk > DISKIMG_SECTOR_SIZE) chunk = DISKIMG_SECTOR_SIZE;
      memcpy(buf, data + b * DISKIMG_SECTOR_SIZE, chunk);
      int d = alloc_data();
      wsector(d, buf);
      in.i_addr[b] = d;
    }
  } else {
    /* Large file: singly-indirect via i_addr[0..6]. */
    in.i_mode |= ILARG;
    int remaining = nblocks;
    int written = 0;
    for (int idx = 0; idx < 7 && remaining > 0; idx++) {
      uint16_t ptrs[PTRS_PER_BLOCK];
      memset(ptrs, 0, sizeof ptrs);
      int here = remaining < PTRS_PER_BLOCK ? remaining : PTRS_PER_BLOCK;
      for (int j = 0; j < here; j++) {
        char buf[DISKIMG_SECTOR_SIZE];
        memset(buf, 0, sizeof buf);
        int chunk = size - written * DISKIMG_SECTOR_SIZE;
        if (chunk > DISKIMG_SECTOR_SIZE) chunk = DISKIMG_SECTOR_SIZE;
        if (chunk < 0) chunk = 0;
        memcpy(buf, data + written * DISKIMG_SECTOR_SIZE, chunk);
        int d = alloc_data();
        wsector(d, buf);
        ptrs[j] = d;
        written++;
      }
      int indblk = alloc_data();
      wsector(indblk, ptrs);
      in.i_addr[idx] = indblk;
      remaining -= here;
    }
  }
  put_inode(inumber, &in);
  return inumber;
}

/* Build a directory inode from a list of entries. Returns inumber. */
static int make_dir(int inumber, int parentInumber,
                    struct direntv6 *entries, int nentries) {
  /* Every directory starts with "." and "..". */
  int total = nentries + 2;
  int size = total * (int)sizeof(struct direntv6);
  char buf[DISKIMG_SECTOR_SIZE];
  memset(buf, 0, sizeof buf);
  struct direntv6 *out = (struct direntv6 *)buf;
  out[0].d_inumber = inumber;      strncpy(out[0].d_name, ".", MAX_COMPONENT_LENGTH);
  out[1].d_inumber = parentInumber; strncpy(out[1].d_name, "..", MAX_COMPONENT_LENGTH);
  for (int i = 0; i < nentries; i++) out[2 + i] = entries[i];

  int d = alloc_data();
  wsector(d, buf);

  struct inode in;
  memset(&in, 0, sizeof in);
  in.i_mode = IALLOC | IFDIR;
  in.i_nlink = 2;
  set_size(&in, size);
  in.i_addr[0] = d;
  put_inode(inumber, &in);
  return inumber;
}

static struct direntv6 mkent(int inumber, const char *name) {
  struct direntv6 e;
  memset(&e, 0, sizeof e);
  e.d_inumber = inumber;
  strncpy(e.d_name, name, MAX_COMPONENT_LENGTH);
  return e;
}

int main(int argc, char **argv) {
  if (argc != 2) { fprintf(stderr, "usage: %s out.img\n", argv[0]); return 1; }
  g_fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (g_fd < 0) { perror("open"); return 1; }

  /* Zero out boot + super + inode region so unused inodes read as 0. */
  char zero[DISKIMG_SECTOR_SIZE];
  memset(zero, 0, sizeof zero);
  for (int b = 0; b < DATA_START; b++) wsector(b, zero);

  /* --- Build files --- */
  const char *hello = "Hello from the Unix v6 filesystem!\nThis file lives at /hello.txt\n";
  int helloInode = make_file(alloc_inode(), hello, (int)strlen(hello));

  const char *readme = "CS110 assign2: layered v6 filesystem reader.\n"
                       "Layers: diskimg -> inode -> file -> directory -> pathname.\n";
  int readmeInode = make_file(alloc_inode(), readme, (int)strlen(readme));

  /* A LARGE file: > 8 blocks forces ILARG singly-indirect addressing. */
  int bigSize = 20 * DISKIMG_SECTOR_SIZE + 137;   /* 20.x blocks */
  char *big = malloc(bigSize);
  for (int i = 0; i < bigSize; i++) big[i] = (char)('A' + (i % 26));
  int bigInode = make_file(alloc_inode(), big, bigSize);
  free(big);

  const char *deep = "reached the bottom of /a/b/c/deep.txt\n";
  int deepInode = make_file(alloc_inode(), deep, (int)strlen(deep));

  /* --- Build directory tree ---
     /
     |- hello.txt
     |- README
     |- big.dat  (large file)
     |- a/
        |- b/
           |- c/
              |- deep.txt
  */
  int cInode = alloc_inode();
  int bInode = alloc_inode();
  int aInode = alloc_inode();

  struct direntv6 cEntries[] = { mkent(deepInode, "deep.txt") };
  make_dir(cInode, bInode, cEntries, 1);

  struct direntv6 bEntries[] = { mkent(cInode, "c") };
  make_dir(bInode, aInode, bEntries, 1);

  struct direntv6 aEntries[] = { mkent(bInode, "b") };
  make_dir(aInode, ROOTINO, aEntries, 1);

  struct direntv6 rootEntries[] = {
    mkent(helloInode, "hello.txt"),
    mkent(readmeInode, "README"),
    mkent(bigInode,   "big.dat"),
    mkent(aInode,     "a"),
  };
  make_dir(ROOTINO, ROOTINO, rootEntries, 4);

  /* --- Superblock --- */
  struct filsys sb;
  memset(&sb, 0, sizeof sb);
  sb.s_isize = NUM_INODE_BLOCKS;
  sb.s_fsize = g_maxBlock + 1;
  wsector(1, &sb);

  /* Ensure the image file is exactly (g_maxBlock+1) blocks long so the last
   * data block is fully backed. All real blocks are already written above; a
   * plain ftruncate only extends length, it never clobbers block contents. */
  if (ftruncate(g_fd, (off_t)(g_maxBlock + 1) * DISKIMG_SECTOR_SIZE) != 0) {
    perror("ftruncate");
  }

  close(g_fd);
  printf("wrote %s: %d blocks, %d inodes used\n",
         argv[1], g_maxBlock + 1, g_nextInode);
  return 0;
}

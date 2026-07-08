/**
 * unixfilesystem.h
 * On-disk structures and constants for the classic Unix Version 6 (1975) filesystem,
 * plus the top-level filesystem handle used by the layered reader.
 *
 * All multi-byte integers on a v6 disk are stored little-endian (PDP-11), which
 * matches x86/x86-64, so the packed structs below map directly onto raw disk bytes.
 */
#ifndef _UNIXFILESYSTEM_H
#define _UNIXFILESYSTEM_H

#include <stdint.h>

#define DISKIMG_SECTOR_SIZE 512   /* bytes per block/sector                    */
#define ROOTINO             1     /* inumber of the root directory ("/")       */
#define INODE_SIZE          32    /* bytes per on-disk inode                    */
#define INODES_PER_BLOCK    (DISKIMG_SECTOR_SIZE / INODE_SIZE) /* = 16          */
#define INODE_START_SECTOR  2     /* inodes begin in block 2 (0=boot, 1=super)  */

/* i_mode bit flags (octal, exactly as in the v6 kernel source) */
#define IALLOC  0100000  /* inode is allocated / in use            */
#define IFMT    0060000  /* two-bit file-type mask                 */
#define IFDIR   0040000  /* directory                              */
#define IFCHR   0020000  /* character special                      */
#define IFBLK   0060000  /* block special                          */
#define ILARG   0010000  /* large-file addressing (indirect blocks)*/

/* An on-disk inode is exactly 32 bytes. Attribute-packed so sizeof == 32. */
struct inode {
  uint16_t i_mode;
  uint8_t  i_nlink;
  uint8_t  i_uid;
  uint8_t  i_gid;
  uint8_t  i_size0;      /* most-significant 8 bits of the 24-bit size  */
  uint16_t i_size1;      /* least-significant 16 bits of the size       */
  uint16_t i_addr[8];    /* block map: direct, or (if ILARG) indirect   */
  uint16_t i_atime[2];
  uint16_t i_mtime[2];
} __attribute__((packed));

/* The superblock (block 1) describes the geometry of the filesystem. */
struct filsys {
  uint16_t s_isize;      /* number of blocks devoted to the inode list  */
  uint16_t s_fsize;      /* total number of blocks in the filesystem    */
  uint16_t s_nfree;      /* number of in-core free blocks (0..100)      */
  uint16_t s_free[100];  /* in-core cache of free block numbers         */
  uint16_t s_ninode;     /* number of in-core free inodes (0..100)      */
  uint16_t s_inode[100]; /* in-core cache of free inode numbers         */
  uint8_t  s_flock;
  uint8_t  s_ilock;
  uint8_t  s_fmod;
  uint8_t  s_ronly;
  uint16_t s_time[2];
  uint16_t pad[50];
} __attribute__((packed));

/* A directory is just a file whose data blocks are arrays of these. */
#define MAX_COMPONENT_LENGTH 14
struct direntv6 {
  uint16_t d_inumber;
  char     d_name[MAX_COMPONENT_LENGTH];
} __attribute__((packed));

/* In-memory handle to an opened disk image. */
struct unixfilesystem {
  int fd;                  /* open file descriptor for the raw image */
  struct filsys superblock;
};

#endif

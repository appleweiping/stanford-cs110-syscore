#include "directory.h"
#include "inode.h"
#include "file.h"
#include "diskimg.h"
#include <stdio.h>
#include <string.h>

int directory_findname(struct unixfilesystem *fs, const char *name,
                       int dirinumber, struct direntv6 *dirEnt) {
  struct inode in;
  if (inode_iget(fs, dirinumber, &in) < 0) return -1;
  if ((in.i_mode & IFMT) != IFDIR) return -1;   /* not a directory */

  int size = inode_getsize(&in);
  int numBlocks = (size + DISKIMG_SECTOR_SIZE - 1) / DISKIMG_SECTOR_SIZE;

  char buf[DISKIMG_SECTOR_SIZE];
  for (int b = 0; b < numBlocks; b++) {
    int valid = file_getblock(fs, dirinumber, b, buf);
    if (valid < 0) return -1;
    int nents = valid / (int)sizeof(struct direntv6);
    struct direntv6 *entries = (struct direntv6 *)buf;
    for (int e = 0; e < nents; e++) {
      if (entries[e].d_inumber == 0) continue;   /* free slot */
      /* d_name is at most 14 bytes and may be unterminated; compare bounded. */
      if (strncmp(entries[e].d_name, name, MAX_COMPONENT_LENGTH) == 0) {
        memcpy(dirEnt, &entries[e], sizeof(struct direntv6));
        return 0;
      }
    }
  }
  return -1;
}

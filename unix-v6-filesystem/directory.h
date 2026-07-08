#ifndef _DIRECTORY_H
#define _DIRECTORY_H

#include "unixfilesystem.h"

/**
 * directory_findname: search the directory whose inode is `dirinumber` for an
 * entry named `name` (a single path component, no slashes). On success copies
 * the matching directory entry into `*dirEnt` and returns 0; returns -1 if the
 * inode is not a directory or the name is not found.
 */
int directory_findname(struct unixfilesystem *fs, const char *name,
                       int dirinumber, struct direntv6 *dirEnt);

#endif

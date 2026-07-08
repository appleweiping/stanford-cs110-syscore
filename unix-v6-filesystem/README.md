# assign2 — Unix Version 6 Filesystem Reader

A layered, read-only reimplementation of the classic Unix V6 (1975) filesystem,
built strictly bottom-up: `diskimg → inode → file → directory → pathname`.

| Layer | File | Responsibility |
|---|---|---|
| Block device | `diskimg.c` | 512-byte sector read/write over an image file |
| Inode | `inode.c` | `inode_iget`, `inode_indexlookup` (small + large/indirect), `inode_getsize` |
| File | `file.c` | `file_getblock` — Nth block of a file, with correct tail byte count |
| Directory | `directory.c` | `directory_findname` — resolve one path component |
| Pathname | `pathname.c` | `pathname_lookup` — walk an absolute path to an inumber |

## Self-contained disk image

The original assignment ships proprietary disk images. `mkfs_v6.c` writes a real
V6-format image so the reader is fully reproducible. It deliberately includes a
**large file** (`/big.dat`, 20+ blocks) to exercise the `ILARG` singly-indirect
addressing path, and a nested tree `/a/b/c/deep.txt`.

## Run

```bash
make test        # builds everything, generates sample.img, runs 20 assertions
./mkfs_v6 sample.img
./diskimageaccess sample.img   # dumps the tree with per-file checksums
./runtests sample.img          # assertion suite (exit 0 on success)
```

Verified: **20/20 assertions pass**, clean build under `-Wall -Wextra -Werror`.
See `../results/` for captured output.

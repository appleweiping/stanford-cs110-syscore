# assign1 — Amazon Reviews Search

A keyword-search engine over Amazon reviews stored in two **memory-mapped binary
files**, driven entirely by C pointer arithmetic and STL binary search — the
CS110 C/C++/STL refresher assignment.

- **Database file** (`<prefix>.db`): a 4-byte review count, an offset table
  (one `uint32` per review), then variable-length review records
  (`title\0 category\0 rating headline\0 body\0 [pad] year(u16) month day`).
- **Keyword index** (`<prefix>.kwi`): a 4-byte keyword count, an offset table,
  then alphabetically sorted entries (`keyword\0 [pad] count(u32)` followed by
  `count` × `(reviewIndex:u32, location:u32)`, where `location`'s top byte is
  the field id and the low 3 bytes are the word offset).

The `amazon` class `mmap`s both files once and never copies them:

- `getReview(i, r)` — follow offset table entry `i`, parse the record in place.
- `searchKeywordIndex(query, out)` — `convertQuery` splits the query into terms
  (unquoted word = a term; `"quoted phrase"` = one multi-word term), each term is
  `std::lower_bound`-searched in the sorted keyword table, phrases require
  consecutive word offsets in one field, and terms are combined with
  `std::set_intersection` (AND).
- `getSortedReviewsFromIndexes(...)` — materialize + `std::sort` with a caller
  comparator.

## Self-contained dataset

The real assignment ships a proprietary multi-GB dataset. `mkamazondb` writes a
real, **spec-conformant** `.db` + `.kwi` from a plain TSV, so the engine is fully
reproducible (exactly as assign2 ships its own `mkfs_v6`).

```bash
make
./mkamazondb tests/reviews.tsv tests sample     # build sample.{db,kwi}
./dbase_test -d tests -f sample 0               # print review #0
./amazon_search -d tests -f sample -k stars -r headphones
make test                                       # full test suite
```

## Verification (WSL2, Ubuntu 24.04, g++ 13.3), `-Wall -Wextra -Werror` clean

Captured in `../results/assign1_amazon.txt` — **10/10 pass**:

| Check | Result |
|---|---|
| `dbase_test` review count | 10 |
| `getReview` via mmap+offset table (review #6) | body parsed correctly |
| single-word `headphones` | 3 reviews |
| phrase `"works great"` (consecutive) | 3 reviews |
| multi-term AND `works great` | 4 reviews (incl. the review where the words are *not* adjacent) |
| rare word | exactly 1 review |
| unknown word | 0 reviews |
| sort by stars (`-r` / ascending, `-n 1`) | 5-star / 4-star top hit |
| **valgrind** `--leak-check=full` | no definite leaks or errors |

The phrase-vs-AND gap (3 vs 4) demonstrates the consecutive-word-offset phrase
logic is genuinely distinct from plain intersection.

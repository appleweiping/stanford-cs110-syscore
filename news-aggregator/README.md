# assign5 — RSS News Feed Aggregator

`stnewsaggregator` — a multithreaded news indexer (CS110 assign5). Given the URL
of an RSS *feed-list* document it concurrently downloads every feed, then every
article, tokenizes the articles' visible text, and folds them into a shared
**inverted index** that answers word queries ranked by frequency.

```bash
make
./aggregate <feed-list-url>
# then type query words on stdin:
#   systems
#   3 article(s) contain "systems":
#     4   Operating Systems Intro [http://.../a1.html]
#     1   Threads and Concurrency [http://.../a2.html]
#     1   Networking Basics [http://.../b1.html]
```

## Pipeline & concurrency

1. Download the feed list, parse its `<item>` links → feed URLs.
2. A **feed ThreadPool** downloads each feed; each feed task parses its articles
   and schedules **article tasks** onto a second **article ThreadPool**.
3. Each article task downloads the HTML, extracts visible-text tokens
   (`<script>`/`<style>` skipped), and merges per-token counts into the index.
4. `feedPool.wait()` then `articlePool.wait()` join the two stages.

- **HTTP** via a from-scratch client (`fetch.*`, reusing the proxy's HTTP model).
- **Parsing** via **libxml2** (`xml-utils.*`): RSS `<item>`/Atom `<entry>` for
  titles+links, `htmlReadMemory` + DOM walk for article text.
- **De-duplication:** each article URL is indexed at most once (a URL shared by
  two feeds is fetched once).
- **Per-server throttle:** a counting `semaphore` per host caps simultaneous
  connections (`kMaxConnectionsPerServer = 8`).
- Shared state (index, article table, seen-set, server map) is mutex-protected;
  `xmlInitParser()` is called before any worker thread (libxml2 requirement).

## Verification (WSL2, Ubuntu 24.04, g++ 13.3, libxml2 2.9.14), `-Werror` clean

`make test` serves a small local site (`tests/site/`: one feed list, two feeds,
three articles — `a2.html` deliberately listed in *both* feeds) and runs the
aggregator against it. Captured in `../results/assign5_aggregator.txt`:

| Check | Result |
|---|---|
| de-dup: 3 distinct articles from 2 feeds | ✓ (access log shows `a2.html` fetched once) |
| query `systems` → all 3 articles | ✓ |
| ranking: `Operating Systems Intro` is top hit (count 4) | ✓ |
| `<script>` "systems" excluded (count 4, not 8) | ✓ |
| query `threads` → exactly 1 article | ✓ |
| query `networking` → exactly 1 article | ✓ |
| unknown word → "No matches" | ✓ |

**7/7 pass.** The origin access log proves de-dup and the article/feed fan-out
are causal (6 GETs total: 1 feed list + 2 feeds + 3 unique articles).

## Notes / scope

Exercised against a local server so no external network (or the course's private
feed set) is required; the same binary works against real RSS feeds. The
tokenizer lowercases alphanumeric runs of length 2–32 and skips script/style.

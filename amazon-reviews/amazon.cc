/**
 * File: amazon.cc
 * ---------------
 * Implementation of the amazon search engine. The two files are mmap'd once in
 * the constructor; every accessor walks the raw bytes with pointer arithmetic
 * (memcpy for the multi-byte integers so unaligned reads are well-defined).
 */

#include "amazon.h"

#include <cstdint>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <set>
#include <tuple>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

// ---------------------------------------------------------------- byte helpers
static uint32_t readU32(const uint8_t *p) {
  uint32_t v;
  std::memcpy(&v, p, sizeof(v));
  return v;
}
static uint16_t readU16(const uint8_t *p) {
  uint16_t v;
  std::memcpy(&v, p, sizeof(v));
  return v;
}
// Read a NUL-terminated string and advance p past the terminator.
static std::string readCString(const uint8_t *&p) {
  const char *s = reinterpret_cast<const char *>(p);
  size_t len = std::strlen(s);
  p += len + 1;
  return std::string(s, len);
}

// ---------------------------------------------------------------- query parsing
static std::vector<std::string> splitWords(const std::string &s) {
  std::vector<std::string> words;
  std::string cur;
  for (char ch : s) {
    unsigned char c = static_cast<unsigned char>(ch);
    if (std::isalnum(c)) cur += static_cast<char>(std::tolower(c));
    else if (!cur.empty()) { words.push_back(cur); cur.clear(); }
  }
  if (!cur.empty()) words.push_back(cur);
  return words;
}

std::vector<std::vector<std::string>> convertQuery(const std::string &query) {
  std::vector<std::vector<std::string>> terms;
  size_t i = 0;
  while (i < query.size()) {
    if (std::isspace(static_cast<unsigned char>(query[i]))) { i++; continue; }
    if (query[i] == '"') {
      size_t close = query.find('"', i + 1);
      std::string phrase = (close == std::string::npos) ? query.substr(i + 1)
                                                        : query.substr(i + 1, close - i - 1);
      i = (close == std::string::npos) ? query.size() : close + 1;
      std::vector<std::string> words = splitWords(phrase);
      if (!words.empty()) terms.push_back(words);          // one multi-word term
    } else {
      size_t j = i;
      while (j < query.size() && !std::isspace(static_cast<unsigned char>(query[j])) && query[j] != '"') j++;
      for (const std::string &w : splitWords(query.substr(i, j - i)))
        terms.push_back({w});                              // each word its own term
      i = j;
    }
  }
  return terms;
}

// ---------------------------------------------------------------- ctor / dtor
amazon::amazon(const std::string &directory, const std::string &filesPrefix) {
  auto mapFile = [](const std::string &path, size_t &size) -> const void * {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return nullptr;
    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size == 0) { close(fd); return nullptr; }
    size = static_cast<size_t>(st.st_size);
    void *addr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return (addr == MAP_FAILED) ? nullptr : addr;
  };

  std::string base = directory.empty() ? filesPrefix : directory + "/" + filesPrefix;
  databaseFile = mapFile(base + ".db", databaseSize);
  keywordIndexFile = mapFile(base + ".kwi", keywordIndexSize);

  if (databaseFile != nullptr)
    numReviews = readU32(reinterpret_cast<const uint8_t *>(databaseFile));
  if (keywordIndexFile != nullptr)
    numKeywords = readU32(reinterpret_cast<const uint8_t *>(keywordIndexFile));
}

amazon::~amazon() {
  if (databaseFile != nullptr) munmap(const_cast<void *>(databaseFile), databaseSize);
  if (keywordIndexFile != nullptr) munmap(const_cast<void *>(keywordIndexFile), keywordIndexSize);
}

// ---------------------------------------------------------------- getReview
bool amazon::getReview(unsigned int index, Review &review) const {
  if (databaseFile == nullptr || index >= numReviews) return false;
  const uint8_t *base = reinterpret_cast<const uint8_t *>(databaseFile);

  uint32_t offset = readU32(base + 4 + 4 * index);   // offset table entry
  const uint8_t *p = base + offset;

  review.index = index;
  review.product_title = readCString(p);
  review.product_category = readCString(p);
  review.star_rating = *p++;                          // raw byte 1..5
  review.review_headline = readCString(p);
  review.review_body = readCString(p);
  if (((p - base) & 1) != 0) p++;                     // pad to even boundary
  review.review_year = readU16(p); p += 2;
  review.review_month = *p++;
  review.review_day = *p++;
  return true;
}

void amazon::getSortedReviewsFromIndexes(
    const std::vector<int> &reviewIndexes, std::vector<Review> &reviews,
    std::function<bool(const Review &, const Review &)> cmp) const {
  reviews.clear();
  reviews.reserve(reviewIndexes.size());
  for (int idx : reviewIndexes) {
    Review r;
    if (getReview(static_cast<unsigned int>(idx), r)) reviews.push_back(std::move(r));
  }
  std::sort(reviews.begin(), reviews.end(), cmp);
}

// ---------------------------------------------------------------- keyword search
bool amazon::findKeyword(const std::string &word, std::vector<Occurrence> &occ) const {
  if (keywordIndexFile == nullptr) return false;
  const uint8_t *base = reinterpret_cast<const uint8_t *>(keywordIndexFile);
  const uint32_t *offs = reinterpret_cast<const uint32_t *>(base + 4);
  const uint32_t *end = offs + numKeywords;

  // Binary search the alphabetically sorted keyword offset table.
  const uint32_t *it = std::lower_bound(offs, end, word,
      [&](uint32_t off, const std::string &w) {
        return std::strcmp(reinterpret_cast<const char *>(base + off), w.c_str()) < 0;
      });
  if (it == end) return false;

  const uint8_t *p = base + *it;
  if (std::strcmp(reinterpret_cast<const char *>(p), word.c_str()) != 0) return false;

  p += std::strlen(reinterpret_cast<const char *>(p)) + 1;  // past keyword + NUL
  if (((p - base) & 1) != 0) p++;                           // pad to even boundary
  uint32_t count = readU32(p); p += 4;
  occ.reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    uint32_t reviewIndex = readU32(p);
    uint32_t location = readU32(p + 4);
    p += 8;
    occ.push_back({reviewIndex, static_cast<unsigned char>(location >> 24), location & 0x00FFFFFFu});
  }
  return true;
}

bool amazon::searchKeywordIndex(const std::string &query, std::vector<int> &reviewIndexes) const {
  reviewIndexes.clear();
  std::vector<std::vector<std::string>> terms = convertQuery(query);
  if (terms.empty()) return false;

  std::set<int> result;
  bool first = true;

  for (const std::vector<std::string> &term : terms) {
    std::set<int> matches;
    if (term.size() == 1) {
      std::vector<Occurrence> occ;
      if (findKeyword(term[0], occ))
        for (const Occurrence &o : occ) matches.insert(static_cast<int>(o.reviewIndex));
    } else {
      // Phrase: each word must appear at consecutive offsets in the same field
      // of the same review. Anchor on word 0, verify the rest.
      using Pos = std::tuple<unsigned int, unsigned char, unsigned int>;  // review, field, offset
      std::vector<std::set<Pos>> positions(term.size());
      bool anyEmpty = false;
      for (size_t k = 0; k < term.size(); k++) {
        std::vector<Occurrence> occ;
        if (!findKeyword(term[k], occ)) { anyEmpty = true; break; }
        for (const Occurrence &o : occ) positions[k].insert({o.reviewIndex, o.field, o.wordOffset});
      }
      if (!anyEmpty) {
        for (const Pos &anchor : positions[0]) {
          bool ok = true;
          for (size_t k = 1; k < term.size() && ok; k++) {
            Pos need{std::get<0>(anchor), std::get<1>(anchor), std::get<2>(anchor) + static_cast<unsigned int>(k)};
            ok = positions[k].count(need) > 0;
          }
          if (ok) matches.insert(static_cast<int>(std::get<0>(anchor)));
        }
      }
    }

    if (first) { result = std::move(matches); first = false; }
    else {
      std::set<int> inter;
      std::set_intersection(result.begin(), result.end(), matches.begin(), matches.end(),
                            std::inserter(inter, inter.begin()));
      result = std::move(inter);
    }
    if (result.empty()) break;
  }

  reviewIndexes.assign(result.begin(), result.end());
  return !reviewIndexes.empty();
}

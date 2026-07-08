/**
 * File: amazon.h
 * --------------
 * The `amazon` class (CS110 assign1): a keyword-search engine over Amazon
 * reviews stored in two memory-mapped binary files — a *database* file
 * (offset-indexed review records) and a *keyword index* file (alphabetically
 * sorted keyword -> occurrence lists). All access is via raw pointer arithmetic
 * over the mmap'd images plus STL binary search.
 */

#pragma once
#include <string>
#include <vector>
#include <functional>

struct Review {
  unsigned int index;
  std::string product_title;
  std::string product_category;
  int star_rating;
  std::string review_headline;
  std::string review_body;
  int review_year;
  int review_month;
  int review_day;
};

// Parse a query into terms (AND across terms). An unquoted word is a one-word
// term; a "quoted phrase" is a multi-word term (consecutive words, one field).
std::vector<std::vector<std::string>> convertQuery(const std::string &query);

class amazon {
 public:
  amazon(const std::string &directory, const std::string &filesPrefix);
  ~amazon();

  bool good() const { return databaseFile != nullptr && keywordIndexFile != nullptr; }
  unsigned int totalReviews() const { return numReviews; }
  unsigned int totalKeywords() const { return numKeywords; }

  bool getReview(unsigned int index, Review &review) const;
  void getSortedReviewsFromIndexes(const std::vector<int> &reviewIndexes,
                                   std::vector<Review> &reviews,
                                   std::function<bool(const Review &, const Review &)> cmp) const;
  bool searchKeywordIndex(const std::string &query, std::vector<int> &reviewIndexes) const;

 private:
  struct Occurrence {
    unsigned int reviewIndex;
    unsigned char field;   // 0=title, 1=category, 2=headline, 3=body
    unsigned int wordOffset;
  };

  const void *databaseFile = nullptr;
  const void *keywordIndexFile = nullptr;
  size_t databaseSize = 0;
  size_t keywordIndexSize = 0;
  unsigned int numReviews = 0;
  unsigned int numKeywords = 0;

  bool findKeyword(const std::string &word, std::vector<Occurrence> &occ) const;

  amazon(const amazon &) = delete;
  amazon &operator=(const amazon &) = delete;
};

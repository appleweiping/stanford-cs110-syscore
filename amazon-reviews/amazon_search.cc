/**
 * File: amazon_search.cc
 * ----------------------
 * Searches the review database for a query and prints the matching reviews in a
 * chosen sort order.
 *
 *   ./amazon_search -d <dir> -f <prefix> [-k date|stars|bodysize|title]
 *                   [-r] [-n N] <query...>
 */

#include "amazon.h"
#include <getopt.h>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

static void printReview(const Review &r) {
  std::cout << "Review #" << r.index << ":\n"
            << "  Title:    " << r.product_title << "\n"
            << "  Category: " << r.product_category << "\n"
            << "  Rating:   " << r.star_rating << " star(s)\n"
            << "  Headline: " << r.review_headline << "\n"
            << "  Date:     " << r.review_year << "-" << r.review_month << "-" << r.review_day << "\n"
            << "  Body:     " << r.review_body << "\n";
}

int main(int argc, char *argv[]) {
  std::string directory = ".";
  std::string prefix = "amazon_reviews_us_Electronics_v1_00";
  std::string key = "date";
  bool reversed = false;
  long limit = -1;  // all

  static struct option longopts[] = {
      {"directory", required_argument, nullptr, 'd'},
      {"files-prefix", required_argument, nullptr, 'f'},
      {"primary-key", required_argument, nullptr, 'k'},
      {"reversed", no_argument, nullptr, 'r'},
      {"number-of-reviews", required_argument, nullptr, 'n'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};
  int ch;
  while ((ch = getopt_long(argc, argv, "d:f:k:rn:h", longopts, nullptr)) != -1) {
    switch (ch) {
      case 'd': directory = optarg; break;
      case 'f': prefix = optarg; break;
      case 'k': key = optarg; break;
      case 'r': reversed = true; break;
      case 'n': limit = std::stol(optarg); break;
      case 'h':
        std::cout << "usage: " << argv[0]
                  << " -d <dir> -f <prefix> [-k date|stars|bodysize|title] [-r] [-n N] <query>\n";
        return 0;
      default: return 1;
    }
  }

  std::string query;
  for (int i = optind; i < argc; i++) { if (!query.empty()) query += " "; query += argv[i]; }
  if (query.empty()) { std::cerr << "error: empty query\n"; return 1; }

  amazon db(directory, prefix);
  if (!db.good()) {
    std::cerr << "error: could not open database '" << prefix << "' in '" << directory << "'\n";
    return 1;
  }

  std::function<bool(const Review &, const Review &)> cmp;
  if (key == "stars") cmp = [](const Review &a, const Review &b) { return a.star_rating < b.star_rating; };
  else if (key == "bodysize") cmp = [](const Review &a, const Review &b) { return a.review_body.size() < b.review_body.size(); };
  else if (key == "title") cmp = [](const Review &a, const Review &b) { return a.product_title < b.product_title; };
  else cmp = [](const Review &a, const Review &b) {  // date
    if (a.review_year != b.review_year) return a.review_year < b.review_year;
    if (a.review_month != b.review_month) return a.review_month < b.review_month;
    return a.review_day < b.review_day;
  };
  if (reversed) {
    auto base = cmp;
    cmp = [base](const Review &a, const Review &b) { return base(b, a); };
  }

  std::vector<int> indexes;
  db.searchKeywordIndex(query, indexes);
  std::cout << "Found " << indexes.size() << " matching reviews out of "
            << db.totalReviews() << " reviews in the database.\n";

  std::vector<Review> reviews;
  db.getSortedReviewsFromIndexes(indexes, reviews, cmp);

  size_t shown = 0;
  for (const Review &r : reviews) {
    if (limit >= 0 && shown >= static_cast<size_t>(limit)) break;
    printReview(r);
    shown++;
  }
  return 0;
}

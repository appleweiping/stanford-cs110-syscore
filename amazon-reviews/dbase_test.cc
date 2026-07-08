/**
 * File: dbase_test.cc
 * -------------------
 * Prints the review stored at a given index. With an index on the command line
 * it prints that one review; otherwise it loops, reading indexes from stdin.
 *
 *   ./dbase_test -d <dir> -f <prefix> [index]
 */

#include "amazon.h"
#include <getopt.h>
#include <iostream>
#include <string>

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

  static struct option longopts[] = {
      {"directory", required_argument, nullptr, 'd'},
      {"files-prefix", required_argument, nullptr, 'f'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};
  int ch;
  while ((ch = getopt_long(argc, argv, "d:f:h", longopts, nullptr)) != -1) {
    switch (ch) {
      case 'd': directory = optarg; break;
      case 'f': prefix = optarg; break;
      case 'h':
        std::cout << "usage: " << argv[0] << " -d <dir> -f <prefix> [index]\n";
        return 0;
      default: return 1;
    }
  }

  amazon db(directory, prefix);
  if (!db.good()) {
    std::cerr << "error: could not open database '" << prefix << "' in '" << directory << "'\n";
    return 1;
  }

  if (optind < argc) {  // index supplied on the command line
    unsigned int index = static_cast<unsigned int>(std::stoul(argv[optind]));
    Review r;
    if (!db.getReview(index, r)) { std::cerr << "invalid index " << index << "\n"; return 1; }
    printReview(r);
    return 0;
  }

  std::cout << "Total number of reviews: " << db.totalReviews() << "\n";
  std::string line;
  while (true) {
    std::cout << "Please enter an index (<enter> to end): " << std::flush;
    if (!std::getline(std::cin, line) || line.empty()) break;
    Review r;
    if (db.getReview(static_cast<unsigned int>(std::stoul(line)), r)) printReview(r);
    else std::cout << "invalid index\n";
  }
  return 0;
}

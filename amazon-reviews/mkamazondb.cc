/**
 * File: mkamazondb.cc
 * -------------------
 * Builds the two binary files the `amazon` class reads, in exactly the byte
 * layout assign1 specifies. The real assignment ships a proprietary ~multi-GB
 * dataset; this tool writes a real, spec-conformant database + keyword index
 * from a plain TSV so the search engine is fully reproducible (mirrors how the
 * assign2 filesystem reader ships its own mkfs_v6).
 *
 *   ./mkamazondb <reviews.tsv> <out-dir> <prefix>
 *
 * TSV columns (tab-separated, one review per line):
 *   title  category  rating  headline  body  year  month  day
 *
 * Emits <out-dir>/<prefix>.db and <out-dir>/<prefix>.kwi.
 */

#include <cstdint>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

using namespace std;

struct Rec {
  string title, category, headline, body;
  int rating, year, month, day;
};

static vector<string> splitWords(const string &s) {
  vector<string> words;
  string cur;
  for (char ch : s) {
    unsigned char c = static_cast<unsigned char>(ch);
    if (isalnum(c)) cur += static_cast<char>(tolower(c));
    else if (!cur.empty()) { words.push_back(cur); cur.clear(); }
  }
  if (!cur.empty()) words.push_back(cur);
  return words;
}

static void appendU32(string &out, uint32_t v) { out.append(reinterpret_cast<char *>(&v), 4); }
static void appendU16(string &out, uint16_t v) { out.append(reinterpret_cast<char *>(&v), 2); }

int main(int argc, char *argv[]) {
  if (argc != 4) {
    cerr << "usage: " << argv[0] << " <reviews.tsv> <out-dir> <prefix>\n";
    return 1;
  }
  ifstream in(argv[1]);
  if (!in) { cerr << "cannot open " << argv[1] << "\n"; return 1; }
  string outDir = argv[2], prefix = argv[3];

  vector<Rec> recs;
  string line;
  while (getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    vector<string> f;
    string cell;
    istringstream ss(line);
    while (getline(ss, cell, '\t')) f.push_back(cell);
    if (f.size() < 8) { cerr << "skipping malformed line\n"; continue; }
    Rec r;
    r.title = f[0]; r.category = f[1]; r.rating = stoi(f[2]);
    r.headline = f[3]; r.body = f[4];
    r.year = stoi(f[5]); r.month = stoi(f[6]); r.day = stoi(f[7]);
    recs.push_back(r);
  }

  const uint32_t n = static_cast<uint32_t>(recs.size());

  // ---- database file ----
  {
    const size_t headerSize = 4 + 4ull * n;
    vector<uint32_t> offsets(n);
    string blob;
    size_t cur = headerSize;
    for (uint32_t i = 0; i < n; i++) {
      offsets[i] = static_cast<uint32_t>(cur);
      const Rec &r = recs[i];
      string rec;
      rec += r.title; rec += '\0';
      rec += r.category; rec += '\0';
      rec += static_cast<char>(r.rating);
      rec += r.headline; rec += '\0';
      rec += r.body; rec += '\0';
      if ((cur + rec.size()) % 2 != 0) rec += '\0';   // align year to even offset
      appendU16(rec, static_cast<uint16_t>(r.year));
      rec += static_cast<char>(r.month);
      rec += static_cast<char>(r.day);
      blob += rec;
      cur += rec.size();
    }
    ofstream out(outDir + "/" + prefix + ".db", ios::binary);
    string header;
    appendU32(header, n);
    for (uint32_t o : offsets) appendU32(header, o);
    out.write(header.data(), header.size());
    out.write(blob.data(), blob.size());
  }

  // ---- keyword index file ----
  {
    // keyword -> occurrences (reviewIndex, location=(field<<24)|wordOffset)
    map<string, vector<pair<uint32_t, uint32_t>>> kw;
    for (uint32_t i = 0; i < n; i++) {
      const Rec &r = recs[i];
      const string *fields[4] = {&r.title, &r.category, &r.headline, &r.body};
      for (uint32_t field = 0; field < 4; field++) {
        vector<string> words = splitWords(*fields[field]);
        for (uint32_t off = 0; off < words.size(); off++) {
          uint32_t location = (field << 24) | (off & 0x00FFFFFFu);
          kw[words[off]].push_back({i, location});
        }
      }
    }
    const uint32_t m = static_cast<uint32_t>(kw.size());
    const size_t headerSize = 4 + 4ull * m;
    vector<uint32_t> offsets;
    offsets.reserve(m);
    string blob;
    size_t cur = headerSize;
    for (auto &entry : kw) {                 // std::map iterates alphabetically
      offsets.push_back(static_cast<uint32_t>(cur));
      string e;
      e += entry.first; e += '\0';
      if ((cur + e.size()) % 2 != 0) e += '\0';        // align count to even offset
      appendU32(e, static_cast<uint32_t>(entry.second.size()));
      auto &occ = entry.second;
      sort(occ.begin(), occ.end());
      for (auto &o : occ) { appendU32(e, o.first); appendU32(e, o.second); }
      blob += e;
      cur += e.size();
    }
    ofstream out(outDir + "/" + prefix + ".kwi", ios::binary);
    string header;
    appendU32(header, m);
    for (uint32_t o : offsets) appendU32(header, o);
    out.write(header.data(), header.size());
    out.write(blob.data(), blob.size());
    cerr << "built " << n << " reviews, " << m << " keywords\n";
  }
  return 0;
}

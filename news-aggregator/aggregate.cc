/**
 * File: aggregate.cc
 * ------------------
 * stnewsaggregator (CS110 assign5). Given the URL of an RSS feed-list document,
 * it concurrently:
 *   1. downloads the feed list and parses out the individual feed URLs,
 *   2. downloads each feed and parses out its article <item>s,
 *   3. downloads each article's HTML and tokenizes the visible text,
 *   4. folds the tokens into a shared inverted index (token -> article -> count).
 * Then it answers word queries from stdin, ranking matching articles by count.
 *
 * Concurrency: a feed ThreadPool whose tasks schedule article tasks onto a
 * second ThreadPool. A per-server counting semaphore caps simultaneous
 * connections to any one host. Articles are de-duplicated by URL. All shared
 * state (index, article table, seen-set, server map) is mutex-protected.
 */

#include "fetch.h"
#include "xml-utils.h"
#include "thread-pool.h"
#include "semaphore.h"

#include <libxml/parser.h>

#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <algorithm>
#include <iostream>

using namespace std;

namespace {

struct Article { string title; string url; };

vector<Article> articles;                              // guarded by articlesMutex
map<string, map<size_t, int>> invertedIndex;           // token -> {articleIdx: count}
set<string> seenURLs;
map<string, unique_ptr<semaphore>> serverLimits;

mutex articlesMutex, indexMutex, seenMutex, serversMutex;
const int kMaxConnectionsPerServer = 8;

semaphore &serverSemaphore(const string &server) {
  lock_guard<mutex> lg(serversMutex);
  auto &slot = serverLimits[server];
  if (!slot) slot = make_unique<semaphore>(kMaxConnectionsPerServer);
  return *slot;
}

string normalize(const string &word) {
  string q;
  for (char ch : word) {
    unsigned char c = static_cast<unsigned char>(ch);
    if (isalnum(c)) q += static_cast<char>(tolower(c));
  }
  return q;
}

void processArticle(const RSSItem &item) {
  {
    lock_guard<mutex> lg(seenMutex);
    if (!seenURLs.insert(item.link).second) return;  // already indexed
  }

  semaphore &sem = serverSemaphore(parseHttpURL(item.link).server());
  sem.wait();
  string html;
  bool ok = fetchURL(item.link, html);
  sem.signal();
  if (!ok) return;

  map<string, int> counts;
  for (const string &tok : tokenizeHTML(html)) counts[tok]++;
  if (counts.empty()) return;

  size_t idx;
  {
    lock_guard<mutex> lg(articlesMutex);
    idx = articles.size();
    articles.push_back({item.title, item.link});
  }
  {
    lock_guard<mutex> lg(indexMutex);
    for (const auto &kv : counts) invertedIndex[kv.first][idx] += kv.second;
  }
}

void processFeed(const RSSItem &feed, ThreadPool &articlePool) {
  semaphore &sem = serverSemaphore(parseHttpURL(feed.link).server());
  sem.wait();
  string xml;
  bool ok = fetchURL(feed.link, xml);
  sem.signal();
  if (!ok) return;

  for (const RSSItem &item : parseRSSItems(xml, feed.link))
    articlePool.schedule([item, &articlePool] { processArticle(item); });
}

void runQueries(istream &in, ostream &out) {
  string word;
  while (in >> word) {
    string q = normalize(word);
    auto it = invertedIndex.find(q);
    if (q.empty() || it == invertedIndex.end() || it->second.empty()) {
      out << "No matches for \"" << word << "\".\n";
      continue;
    }
    vector<pair<size_t, int>> hits(it->second.begin(), it->second.end());
    sort(hits.begin(), hits.end(), [](const pair<size_t, int> &a, const pair<size_t, int> &b) {
      if (a.second != b.second) return a.second > b.second;
      return articles[a.first].title < articles[b.first].title;
    });
    out << hits.size() << " article(s) contain \"" << q << "\":\n";
    size_t shown = 0;
    for (const auto &h : hits) {
      out << "  " << h.second << "\t" << articles[h.first].title
          << " [" << articles[h.first].url << "]\n";
      if (++shown >= 10) break;
    }
  }
}

}  // namespace

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cerr << "usage: " << argv[0] << " <feed-list-url>\n";
    return 1;
  }
  const string feedListURL = argv[1];

  xmlInitParser();  // required before libxml2 is used from multiple threads

  string indexXML;
  if (!fetchURL(feedListURL, indexXML)) {
    cerr << "fatal: could not fetch feed list " << feedListURL << "\n";
    return 1;
  }
  vector<RSSItem> feeds = parseRSSItems(indexXML, feedListURL);

  {
    ThreadPool feedPool(8), articlePool(24);
    for (const RSSItem &feed : feeds)
      feedPool.schedule([feed, &articlePool] { processFeed(feed, articlePool); });
    feedPool.wait();      // all feeds parsed + all article tasks scheduled
    articlePool.wait();   // all articles downloaded + indexed
  }
  xmlCleanupParser();

  cerr << "indexed " << articles.size() << " articles from " << feeds.size()
       << " feeds; " << invertedIndex.size() << " distinct tokens\n";

  runQueries(cin, cout);
  return 0;
}

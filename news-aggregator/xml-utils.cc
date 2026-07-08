/**
 * File: xml-utils.cc
 * ------------------
 * Implementation of the RSS/HTML parsing helpers using libxml2. Parsers are set
 * to recover from malformed markup and stay quiet. (main() calls xmlInitParser()
 * once before any worker threads run, as libxml2 requires.)
 */

#include "xml-utils.h"
#include "fetch.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/HTMLparser.h>

#include <cctype>
#include <cstring>

namespace {

std::string nodeText(xmlNode *node) {
  std::string out;
  xmlChar *content = xmlNodeGetContent(node);
  if (content != nullptr) { out = reinterpret_cast<const char *>(content); xmlFree(content); }
  return out;
}

// First direct child element named `name` (case-insensitive), or nullptr.
xmlNode *childNamed(xmlNode *parent, const char *name) {
  for (xmlNode *c = parent->children; c != nullptr; c = c->next)
    if (c->type == XML_ELEMENT_NODE && c->name != nullptr &&
        xmlStrcasecmp(c->name, reinterpret_cast<const xmlChar *>(name)) == 0)
      return c;
  return nullptr;
}

// Resolve a possibly-relative link against the feed's URL.
std::string resolve(const std::string &base, const std::string &link) {
  if (link.find("://") != std::string::npos) return link;
  URL b = parseHttpURL(base);
  std::string origin = "http://" + b.server();
  if (!link.empty() && link[0] == '/') return origin + link;
  std::string dir = b.path.substr(0, b.path.find_last_of('/') + 1);
  return origin + dir + link;
}

void collectItems(xmlNode *node, const std::string &baseURL, std::vector<RSSItem> &items) {
  for (xmlNode *n = node; n != nullptr; n = n->next) {
    if (n->type == XML_ELEMENT_NODE && n->name != nullptr &&
        (xmlStrcasecmp(n->name, reinterpret_cast<const xmlChar *>("item")) == 0 ||
         xmlStrcasecmp(n->name, reinterpret_cast<const xmlChar *>("entry")) == 0)) {
      RSSItem item;
      if (xmlNode *t = childNamed(n, "title")) item.title = nodeText(t);
      if (xmlNode *l = childNamed(n, "link")) {
        std::string text = nodeText(l);
        if (text.empty()) {
          xmlChar *href = xmlGetProp(l, reinterpret_cast<const xmlChar *>("href"));  // Atom
          if (href != nullptr) { text = reinterpret_cast<const char *>(href); xmlFree(href); }
        }
        item.link = resolve(baseURL, text);
      }
      if (!item.link.empty()) items.push_back(item);
    }
    if (n->children != nullptr) collectItems(n->children, baseURL, items);
  }
}

void collectVisibleText(xmlNode *node, std::string &out) {
  for (xmlNode *n = node; n != nullptr; n = n->next) {
    if (n->type == XML_ELEMENT_NODE && n->name != nullptr) {
      if (xmlStrcasecmp(n->name, reinterpret_cast<const xmlChar *>("script")) == 0 ||
          xmlStrcasecmp(n->name, reinterpret_cast<const xmlChar *>("style")) == 0)
        continue;
      collectVisibleText(n->children, out);
    } else if (n->type == XML_TEXT_NODE && n->content != nullptr) {
      out += reinterpret_cast<const char *>(n->content);
      out += ' ';
    }
  }
}

}  // namespace

std::vector<RSSItem> parseRSSItems(const std::string &xml, const std::string &baseURL) {
  std::vector<RSSItem> items;
  xmlDoc *doc = xmlReadMemory(xml.data(), static_cast<int>(xml.size()), baseURL.c_str(), nullptr,
                              XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_RECOVER);
  if (doc == nullptr) return items;
  xmlNode *root = xmlDocGetRootElement(doc);
  if (root != nullptr) collectItems(root, baseURL, items);
  xmlFreeDoc(doc);
  return items;
}

std::vector<std::string> tokenizeHTML(const std::string &html) {
  std::vector<std::string> tokens;
  htmlDocPtr doc = htmlReadMemory(html.data(), static_cast<int>(html.size()), "", nullptr,
                                  HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_RECOVER);
  if (doc == nullptr) return tokens;
  std::string text;
  xmlNode *root = xmlDocGetRootElement(doc);
  if (root != nullptr) collectVisibleText(root, text);
  xmlFreeDoc(doc);

  std::string cur;
  auto flush = [&] {
    if (cur.size() >= 2 && cur.size() <= 32) tokens.push_back(cur);
    cur.clear();
  };
  for (char ch : text) {
    unsigned char c = static_cast<unsigned char>(ch);
    if (std::isalnum(c)) cur += static_cast<char>(std::tolower(c));
    else flush();
  }
  flush();
  return tokens;
}

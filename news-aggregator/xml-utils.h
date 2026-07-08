/**
 * File: xml-utils.h
 * -----------------
 * libxml2-backed helpers: pull <item> title/link pairs out of an RSS document,
 * and extract lowercased word tokens from an HTML page's visible text.
 */

#pragma once
#include <string>
#include <vector>
#include <utility>

struct RSSItem {
  std::string title;
  std::string link;
};

// Parse an RSS/XML document; returns every <item>'s (title, link).
std::vector<RSSItem> parseRSSItems(const std::string &xml, const std::string &baseURL);

// Extract visible-text word tokens (lowercased, alphanumeric) from HTML.
std::vector<std::string> tokenizeHTML(const std::string &html);

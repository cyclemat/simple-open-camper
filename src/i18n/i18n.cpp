#include "i18n.h"

#include <string.h>
#include <unordered_map>

#define I18N_STR2(x) #x
#define I18N_STR(x) I18N_STR2(x)

struct TrEntry {
  const char *de, *tx;
};

// Tabelle der gewaehlten Sprache; Deutsch braucht keine
static const TrEntry TABLE[] = {
#if __has_include(I18N_STR(PANEL_LANG.h))
#include I18N_STR(PANEL_LANG.h)
#endif
    {nullptr, nullptr}};

namespace {
struct Hash {
  size_t operator()(const char *s) const {
    size_t h = 2166136261u;   // FNV-1a
    while (*s) h = (h ^ (unsigned char)*s++) * 16777619u;
    return h;
  }
};
struct Eq {
  bool operator()(const char *a, const char *b) const { return strcmp(a, b) == 0; }
};
}  // namespace

static std::unordered_map<const char *, const char *, Hash, Eq> *s_map = nullptr;

static void build() {
  s_map = new std::unordered_map<const char *, const char *, Hash, Eq>();
  for (const TrEntry *e = TABLE; e->de; e++)
    if (e->tx && e->tx[0]) (*s_map)[e->de] = e->tx;
}

const char *tr(const char *de) {
  if (!de || !de[0]) return de;
  if (!s_map) build();
  if (s_map->empty()) return de;
  auto it = s_map->find(de);
  return it == s_map->end() ? de : it->second;
}

const char *i18n_lang() { return I18N_STR(PANEL_LANG); }

std::string tr_lines(const char *de) {
  std::string out, line;
  for (const char *p = de;; p++) {
    if (*p == '\n' || !*p) {
      out += tr(line.c_str());
      if (!*p) break;
      out += '\n';
      line.clear();
    } else {
      line += *p;
    }
  }
  return out;
}

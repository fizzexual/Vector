#include "core/vdf.h"

#include <cctype>

namespace vector_core {
namespace {

struct Tok {
  enum Type { Str, Open, Close, End } type;
  std::string s;
};

std::vector<Tok> tokenize(const std::string& in) {
  std::vector<Tok> out;
  const size_t n = in.size();
  size_t i = 0;
  auto is_ws = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };

  while (i < n) {
    char c = in[i];
    if (is_ws(c)) { i++; continue; }

    // line comment: //
    if (c == '/' && i + 1 < n && in[i + 1] == '/') {
      while (i < n && in[i] != '\n') i++;
      continue;
    }
    // conditional token like [$WIN32] — ignore
    if (c == '[') {
      while (i < n && in[i] != ']') i++;
      if (i < n) i++;
      continue;
    }
    if (c == '{') { out.push_back({Tok::Open, "{"}); i++; continue; }
    if (c == '}') { out.push_back({Tok::Close, "}"}); i++; continue; }

    if (c == '"') {
      i++;  // opening quote
      std::string s;
      while (i < n && in[i] != '"') {
        char d = in[i];
        if (d == '\\' && i + 1 < n) {
          char e = in[i + 1];
          switch (e) {
            case '\\': s += '\\'; break;
            case '"':  s += '"';  break;
            case 'n':  s += '\n'; break;
            case 't':  s += '\t'; break;
            default:   s += e;    break;
          }
          i += 2;
          continue;
        }
        s += d;
        i++;
      }
      if (i < n) i++;  // closing quote
      out.push_back({Tok::Str, s});
      continue;
    }

    // bare (unquoted) token
    std::string s;
    while (i < n && !is_ws(in[i]) && in[i] != '{' && in[i] != '}' && in[i] != '"') {
      s += in[i];
      i++;
    }
    out.push_back({Tok::Str, s});
  }

  out.push_back({Tok::End, ""});
  return out;
}

VdfNode parse_value(const std::vector<Tok>& t, size_t& i);

VdfNode parse_object(const std::vector<Tok>& t, size_t& i) {
  VdfNode node;
  node.is_object = true;
  while (t[i].type != Tok::Close && t[i].type != Tok::End) {
    if (t[i].type != Tok::Str) { i++; continue; }
    std::string key = t[i].s;
    i++;
    VdfNode child = parse_value(t, i);
    node.children.emplace_back(std::move(key), std::move(child));
  }
  if (t[i].type == Tok::Close) i++;  // consume }
  return node;
}

// `i` points at the value following a key.
VdfNode parse_value(const std::vector<Tok>& t, size_t& i) {
  if (t[i].type == Tok::Open) {
    i++;  // consume {
    return parse_object(t, i);
  }
  VdfNode leaf;
  leaf.is_object = false;
  if (t[i].type == Tok::Str) { leaf.value = t[i].s; i++; }
  return leaf;
}

bool iequal(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t k = 0; k < a.size(); k++) {
    if (std::tolower((unsigned char)a[k]) != std::tolower((unsigned char)b[k])) return false;
  }
  return true;
}

}  // namespace

const VdfNode* VdfNode::get(const std::string& key) const {
  for (const auto& p : children) {
    if (iequal(p.first, key)) return &p.second;
  }
  return nullptr;
}

std::string VdfNode::get_str(const std::string& key, const std::string& def) const {
  const VdfNode* n = get(key);
  return (n && !n->is_object) ? n->value : def;
}

std::optional<VdfNode> parse_vdf(const std::string& text) {
  std::vector<Tok> toks = tokenize(text);
  size_t i = 0;
  VdfNode root;
  root.is_object = true;
  while (toks[i].type != Tok::End && toks[i].type != Tok::Close) {
    if (toks[i].type != Tok::Str) { i++; continue; }
    std::string key = toks[i].s;
    i++;
    VdfNode child = parse_value(toks, i);
    root.children.emplace_back(std::move(key), std::move(child));
  }
  return root;
}

}  // namespace vector_core

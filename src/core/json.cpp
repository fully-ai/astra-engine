#include "astra/json.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace astra::json {

namespace {

class Parser {
 public:
  explicit Parser(const std::string& s) : src_(s) {}

  Value parse_root() {
    skip_ws();
    auto v = parse_value(0);
    skip_ws();
    if (pos_ != src_.size()) throw std::runtime_error("trailing characters in JSON");
    return v;
  }

 private:
  const std::string& src_;
  size_t pos_ = 0;

  void skip_ws() {
    while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) ++pos_;
  }

  Value parse_value(int depth) {
    if (depth > 64) throw std::runtime_error("JSON max depth exceeded");
    skip_ws();
    if (pos_ >= src_.size()) throw std::runtime_error("unexpected end of JSON");
    char c = src_[pos_];
    if (c == '{') return parse_object(depth + 1);
    if (c == '[') return parse_array(depth + 1);
    if (c == '"') return Value(parse_string());
    if (c == 't' && src_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Value(true); }
    if (c == 'f' && src_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Value(false); }
    if (c == 'n' && src_.compare(pos_, 4, "null") == 0) { pos_ += 4; return Value(nullptr); }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
    throw std::runtime_error(std::string("unexpected JSON char: ") + c);
  }

  Value parse_object(int depth) {
    ++pos_;
    Object obj;
    skip_ws();
    if (pos_ < src_.size() && src_[pos_] == '}') { ++pos_; return Value(std::move(obj)); }
    while (true) {
      skip_ws();
      if (pos_ >= src_.size() || src_[pos_] != '"') throw std::runtime_error("expected object key string");
      std::string k = parse_string();
      skip_ws();
      if (pos_ >= src_.size() || src_[pos_] != ':') throw std::runtime_error("expected ':'");
      ++pos_;
      obj[std::move(k)] = parse_value(depth);
      skip_ws();
      if (pos_ < src_.size() && src_[pos_] == ',') { ++pos_; continue; }
      if (pos_ < src_.size() && src_[pos_] == '}') { ++pos_; break; }
      throw std::runtime_error("expected ',' or '}'");
    }
    return Value(std::move(obj));
  }

  Value parse_array(int depth) {
    ++pos_;
    Array arr;
    skip_ws();
    if (pos_ < src_.size() && src_[pos_] == ']') { ++pos_; return Value(std::move(arr)); }
    while (true) {
      arr.push_back(parse_value(depth));
      skip_ws();
      if (pos_ < src_.size() && src_[pos_] == ',') { ++pos_; continue; }
      if (pos_ < src_.size() && src_[pos_] == ']') { ++pos_; break; }
      throw std::runtime_error("expected ',' or ']'");
    }
    return Value(std::move(arr));
  }

  std::string parse_string() {
    ++pos_;
    std::string out;
    while (pos_ < src_.size()) {
      char c = src_[pos_++];
      if (c == '"') return out;
      if (c == '\\') {
        if (pos_ >= src_.size()) throw std::runtime_error("unterminated escape");
        char e = src_[pos_++];
        switch (e) {
          case '"': out.push_back('"'); break;
          case '\\': out.push_back('\\'); break;
          case '/': out.push_back('/'); break;
          case 'n': out.push_back('\n'); break;
          case 'r': out.push_back('\r'); break;
          case 't': out.push_back('\t'); break;
          default: out.push_back(e); break;
        }
      } else {
        out.push_back(c);
      }
    }
    throw std::runtime_error("unterminated string");
  }

  Value parse_number() {
    size_t start = pos_;
    if (src_[pos_] == '-') ++pos_;
    while (pos_ < src_.size() && (std::isdigit(static_cast<unsigned char>(src_[pos_])) ||
           src_[pos_] == '.' || src_[pos_] == 'e' || src_[pos_] == 'E' ||
           src_[pos_] == '+' || src_[pos_] == '-')) {
      ++pos_;
    }
    return Value(std::stod(src_.substr(start, pos_ - start)));
  }
};

void dump_escaped(std::ostringstream& os, const std::string& s) {
  os << '"';
  for (char c : s) {
    switch (c) {
      case '"': os << "\\\""; break;
      case '\\': os << "\\\\"; break;
      case '\n': os << "\\n"; break;
      case '\r': os << "\\r"; break;
      case '\t': os << "\\t"; break;
      default: os << c; break;
    }
  }
  os << '"';
}

void dump_val(std::ostringstream& os, const Value& v, int indent, int depth) {
  if (v.is_null()) os << "null";
  else if (v.is_bool()) os << (v.as_bool() ? "true" : "false");
  else if (v.is_number()) os << std::setprecision(9) << v.as_number();
  else if (v.is_string()) dump_escaped(os, v.as_string());
  else if (v.is_array()) {
    const auto& arr = v.as_array();
    os << '[';
    for (size_t i = 0; i < arr.size(); ++i) {
      if (i) os << ',';
      dump_val(os, arr[i], indent, depth + 1);
    }
    os << ']';
  } else if (v.is_object()) {
    const auto& obj = v.as_object();
    os << '{';
    bool first = true;
    for (const auto& [k, val] : obj) {
      if (!first) os << ',';
      first = false;
      if (indent > 0) os << '\n' << std::string((depth + 1) * indent, ' ');
      dump_escaped(os, k);
      os << (indent > 0 ? ": " : ":");
      dump_val(os, val, indent, depth + 1);
    }
    if (indent > 0 && !obj.empty()) os << '\n' << std::string(depth * indent, ' ');
    os << '}';
  }
}

}  // namespace

Value parse(const std::string& src) { return Parser(src).parse_root(); }

std::string stringify(const Value& v, int indent) {
  std::ostringstream os;
  dump_val(os, v, indent, 0);
  return os.str();
}

}  // namespace astra::json

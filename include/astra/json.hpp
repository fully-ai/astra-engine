#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace astra::json {

struct Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;

struct Value {
  using Variant = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;
  Variant data{nullptr};

  Value() = default;
  Value(std::nullptr_t) : data(nullptr) {}
  Value(bool b) : data(b) {}
  Value(int i) : data(static_cast<double>(i)) {}
  Value(double d) : data(d) {}
  Value(const char* s) : data(std::string(s)) {}
  Value(std::string s) : data(std::move(s)) {}
  Value(Array a) : data(std::move(a)) {}
  Value(Object o) : data(std::move(o)) {}

  bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
  bool is_bool() const { return std::holds_alternative<bool>(data); }
  bool is_number() const { return std::holds_alternative<double>(data); }
  bool is_string() const { return std::holds_alternative<std::string>(data); }
  bool is_array() const { return std::holds_alternative<Array>(data); }
  bool is_object() const { return std::holds_alternative<Object>(data); }

  double as_number(double def = 0.0) const {
    return is_number() ? std::get<double>(data) : def;
  }
  int as_int(int def = 0) const { return is_number() ? static_cast<int>(std::get<double>(data)) : def; }
  bool as_bool(bool def = false) const { return is_bool() ? std::get<bool>(data) : def; }
  std::string as_string(const std::string& def = "") const {
    return is_string() ? std::get<std::string>(data) : def;
  }
  const Array& as_array() const {
    static const Array empty;
    return is_array() ? std::get<Array>(data) : empty;
  }
  const Object& as_object() const {
    static const Object empty;
    return is_object() ? std::get<Object>(data) : empty;
  }
  const Value& get(const std::string& key) const {
    static const Value null_val;
    if (!is_object()) return null_val;
    const auto& obj = std::get<Object>(data);
    auto it = obj.find(key);
    return it == obj.end() ? null_val : it->second;
  }
};

Value parse(const std::string& src);
std::string stringify(const Value& v, int indent = 0);

}  // namespace astra::json

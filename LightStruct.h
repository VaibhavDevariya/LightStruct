#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <optional>
#include <fstream>
#include <sstream>

class LightStruct
{
public:

    using Object = std::unordered_map<std::string, LightStruct>;
    using Array = std::vector<LightStruct>;
    using VariantType = std::variant<bool, int, double, std::string, Array, Object>;

private:
    VariantType value_;

public:
    LightStruct() = default;
    LightStruct(const LightStruct& obj) = default;

    LightStruct(bool b) : value_(b) {}
    LightStruct(int i) : value_(i) {}
    LightStruct(double d) : value_(d) {}
    LightStruct(const std::string& s) : value_(s) {}
    LightStruct(const Array& arr) : value_(arr) {}
    LightStruct(const Object& obj) : value_(obj) {}
    
    static LightStruct fromFile(const std::string& filename) {
        std::ifstream filePtr(filename);
        if (!filePtr) throw std::runtime_error("Failed to open file");
    
        std::ostringstream ss;
        ss << filePtr.rdbuf();

        Parser parser;
        parser.text_ = std::move(ss.str());
        LightStruct ls = parser.parse();
        return ls;
    }

    template <typename T>
    T getValue(const std::string &key) const{
        if (auto obj = std::get_if<Object>(&value_)) {
            auto it = obj->find(key);
            if (it != obj->end()) {
                if (auto pval = std::get_if<T>(&it->second.value_))
                    return *pval;
            }
        }
        throw std::runtime_error("Type mismatch or missing key: " + key);
    }

    bool getBool(const std::string& key) const {
        return getValue<bool>(key);
    }

    int getInt(const std::string& key) const {
        return getValue<int>(key);
    }

    double getDouble(const std::string& key) const {
        return getValue<double>(key);
    }
    
    std::string getString(const std::string& key) const {
        return getValue<std::string>(key);
    }
    
    Array getArray(const std::string& key) const {
        return getValue<Array>(key);
    }

    Object getObject(const std::string& key) const {
        return getValue<Object>(key);
    }

    LightStruct& operator[](const std::string &key) {
        return std::get<Object>(value_).at(key);
    }

    bool asBool() const {
        return std::get<bool>(value_);
    }

    int asInt() const {
        return std::get<int>(value_);
    }
    
    double asDouble() const {
        return std::get<double>(value_);
    }

    const std::string& asStr() const {
        return std::get<std::string>(value_);
    }

    const Array& asArray() const {
        return std::get<Array>(value_);
    }

    const Object& asObject() const {
        return std::get<Object>(value_);
    }

    std::string serialize() {
        struct Visitor {
            std::string operator()(Object obj) {
                std::ostringstream oss;
                oss << "{";
                bool first = true;

                for (auto& [key, val] : obj) {
                    if (!first) oss << ",";
                    first = false;
                    oss << "\"" << escapeString(key) << "\":" << val.serialize();
                }

                oss << "}";
                return oss.str();
            }

            std::string operator()(Array arr) {
                std::ostringstream oss;
                oss << "[";
                bool first = true;

                for (auto& val : arr) {
                    if (!first) oss << ",";
                    first = false;
                    oss << val.serialize();
                }

                oss << "]";
                return oss.str();
            }

            std::string operator()(std::string& s) {
                return '"' + escapeString(s) + '"';
            }

            std::string operator()(bool b) const {
                return b ? "true" : "false";
            }

            std::string operator()(int i) const {
                return std::to_string(i);
            }

            std::string operator()(double d) const {
                std::ostringstream oss;
                oss.precision(15);
                oss << d;
                return oss.str();
            }

            std::string escapeString(const std::string& s) {
                std::ostringstream oss;
                for (char c : s) {
                    if (c == '"') oss << "\\\"";
                    else if (c == '\\') oss << "\\\\";
                    else oss << c;
                }
                return oss.str();
            }
        };

        return std::visit(Visitor{}, value_);
    }

private:

    class Parser {
    public:
        std::string text_;
        size_t pos_ = 0;

    public:
        LightStruct parse() {
            skipIgnored();
            return parseObject().value();
        }

        void skipIgnored() {
            while (pos_ < text_.size())
            {
                if (std::isspace(text_[pos_]))
                    ++pos_;
                else if (text_[pos_] == '/' && pos_ + 1 < text_.size() && text_[pos_ + 1] == '/') {
                    while (text_[pos_] != '\n')
                        ++pos_;
                }
                else if (text_[pos_] == '/' && pos_ + 1 < text_.size() && text_[pos_ + 1] == '*') {
                    while (pos_ + 1 < text_.size()) {
                        if (text_[pos_] == '*' && text_[pos_ + 1] == '/') {
                            pos_ += 2;
                            break;
                        }
                        ++pos_;
                    }
                }
                else break;
            }
        }

        std::optional<Object> parseObject() {
            if (text_[pos_] != '{') return std::nullopt;
            ++pos_;
            skipIgnored();

            Object obj;

            while (pos_ < text_.size()) {
                skipIgnored();

                if (text_[pos_] == '}') { ++pos_; break; }

                std::string key = parseString().value();

                if (text_[pos_] != ':') throw std::runtime_error("Expected ':' after key");
                ++pos_;
                skipIgnored();

                LightStruct value = parseValue().value();
                obj[key] = value;

                skipIgnored();
                if (text_[pos_] == ',') {
                    ++pos_;
                    skipIgnored();
                }
                else if (text_[pos_] == '}')
                {
                    ++pos_;
                    break;
                }
                else {
                    throw std::runtime_error("Expected ',' or '}' in object");
                }
            }
            return obj;
        }

        std::optional<std::string> parseString() {
            if (text_[pos_] != '"') return std::nullopt;
            ++pos_;
            std::ostringstream oss;

            while (pos_ < text_.size()) {
                char c = text_[pos_++];
                if (c == '"') break;
                if (c == '\\') {
                    if (pos_ < text_.size()) {
                        char next = text_[pos_++];
                        if (next == '"') oss << '"';
                        else if (next == '\\') oss << '\\';
                        else throw std::runtime_error("Unsupported escape sequence");
                    }
                }
                else {
                    oss << c;
                }
            }
            return oss.str();
        }

        std::optional<LightStruct> parseValue() {
            if (pos_ >= text_.size()) return std::nullopt;

            if (text_[pos_] == '"') {
                auto str = parseString();
                return LightStruct(str.value());
            }
            else if (text_[pos_] == '{') {
                Object obj = parseObject().value();
                return LightStruct{ obj };
            }
            else if (text_[pos_] == '[') {
                Array arr = parseArray().value();
                return LightStruct{ arr };
            }
            else {
                return parseLiteral();
            }
        }

        std::optional<Array> parseArray() {
            if (text_[pos_] != '[') return std::nullopt;
            ++pos_;
            skipIgnored();

            Array arr;

            while (pos_ < text_.size()) {
                if (text_[pos_] == ']') { ++pos_; break; }

                arr.emplace_back(std::move(parseValue().value()));
                skipIgnored();
                if (text_[pos_] == ',') {
                    ++pos_;
                    skipIgnored();
                }
                else if (text_[pos_] == ']') {
                    ++pos_;
                    break;
                }
                else {
                    throw std::runtime_error("Expected ',' or ']' in array");
                }
            }
            return arr;
        }

        std::optional<LightStruct> parseLiteral() {
            size_t start = pos_;
            while (pos_ < text_.size() && (isalnum(text_[pos_]) || text_[pos_] == '.' || text_[pos_] == '-'))
                ++pos_;

            std::string token = text_.substr(start, pos_ - start);

            if (token == "true") return LightStruct(true);
            if (token == "false") return LightStruct(false);

            try {
                if (token.find('.') != std::string::npos)
                    return LightStruct(std::stod(token));
                else
                    return LightStruct(std::stoi(token));
            }
            catch (...) {
                throw std::runtime_error("Invalid literal: " + token);
            }
        }
    };
};
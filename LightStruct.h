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
    std::string text_;
    size_t pos_ = 0;

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
        std::ifstream in(filename);
        if (!in) throw std::runtime_error("Failed to open file");
    
        std::ostringstream ss;
        ss << in.rdbuf();

        LightStruct ls;
        ls.text_ = std::move(ss.str());
        ls.parse();
        return ls;
    }

    bool getBool(const std::string& key) const {
        if (auto obj = std::get_if<Object>(&value_)) {
            if (auto pval = std::get_if<bool>(&obj->at(key).value_))
                return *pval;
        }
        throw std::runtime_error("Value is not a bool for: " + key);
    }

    int getInt(const std::string& key) const {
        if (auto obj = std::get_if<Object>(&value_)) {
            if (auto pval = std::get_if<int>(&obj->at(key).value_)) 
                return *pval;
        }
        throw std::runtime_error("Value is not an int for: " + key);
    }

    double getDouble(const std::string& key) const {
        if (auto obj = std::get_if<Object>(&value_)) {
            if (auto pval = std::get_if<double>(&obj->at(key).value_))
                return *pval;
        }
        throw std::runtime_error("Value is not a double for: " + key);
    }
    
    std::string getString(const std::string& key) const {
        if (auto obj = std::get_if<Object>(&value_)) {
            if (auto pval = std::get_if<std::string>(&obj->at(key).value_)) 
                return *pval;
        }
        throw std::runtime_error("Value is not a string for: " + key);
    }
    
    Array getArray(const std::string& key) const {
        if (auto obj = std::get_if<Object>(&value_)) {
            if (auto pval = std::get_if<Array>(&obj->at(key).value_))
                return *pval;
        }
        throw std::runtime_error("Value is not an array for: " + key);
    }

    Object getObject(const std::string& key) const {
        if (auto obj = std::get_if<Object>(&value_)) {
            if (auto pval = std::get_if<Object>(&obj->at(key).value_))
                return *pval;
        }
        throw std::runtime_error("Value is not an object for: " + key);
    }

    LightStruct& operator[](const std::string &key) {
        return std::get<Object>(value_).at(key);
    }

    bool asBool() {
        return std::get<bool>(value_);
    }

    int asInt() {
        return std::get<int>(value_);
    }
    
    double asDouble() {
        return std::get<double>(value_);
    }

    std::string asStr() {
        return std::get<std::string>(value_);
    }

    Array asArray() {
        return std::get<Array>(value_);
    }

    Object asObject() {
        return std::get<Object>(value_);
    }

private:

// ---------------------- PARSING
    void parse() {
        skipWhitespaces();
        value_ = parseObject().value(); 
    }

    void skipWhitespaces() {
        while(pos_ < text_.size() && std::isspace(text_[pos_])) 
            ++pos_;
    }

    std::optional<Object> parseObject() {
        if (text_[pos_] != '{') return std::nullopt;
        ++pos_;
        skipWhitespaces();

        Object obj;

        while(pos_ < text_.size()) {
            if (text_[pos_] == '}') { ++pos_; break; }

            std::string key = parseString().value();
            skipWhitespaces();

            if (text_[pos_] != ':') throw std::runtime_error("Expected ':' after key");
            ++pos_;
            skipWhitespaces();

            LightStruct value = parseValue().value();
            obj[key] = value;

            skipWhitespaces();
            if (text_[pos_] == ',') {
                ++pos_;
                skipWhitespaces();
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
            return LightStruct{arr};
        } 
        else {
            return parseLiteral();
        }
    }

    std::optional<Array> parseArray() {
        if (text_[pos_] != '[') return std::nullopt;
        ++pos_;
        skipWhitespaces();

        Array arr;

        while (pos_ < text_.size()) {
            if (text_[pos_] == ']') { ++pos_; break; }

            arr.emplace_back(std::move(parseValue().value()));
            skipWhitespaces();
            if (text_[pos_] == ',') {
                ++pos_;
                skipWhitespaces();
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

    inline std::string escapeString(const std::string& s) {
        std::ostringstream oss;
        oss << '"';
        for (char c : s) {
            if (c == '"') oss << "\\\"";
            else if (c == '\\') oss << "\\\\";
            else oss << c;
        }
        oss << '"';
        return oss.str();
    }
};
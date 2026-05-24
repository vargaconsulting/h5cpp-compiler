/* Copyright (c) 2018-2026 Steven Varga, steven@vargalabs.com Toronto, ON Canada */

#pragma once

// Issue #32 — source-level translator for the clean `[[h5::xxx(...)]]`
// attribute syntax.
//
// Clang 20's standard-attribute parser drops the argument list for plugin-
// registered namespace-scoped attributes when they use C++11 `[[ns::name(...)]]`
// syntax. To deliver the clean user-facing surface, h5cpp-compiler rewrites the
// source before handing it to Clang Tooling:
//
//     [[h5::name("x")]]      →  [[clang::annotate("h5::name", "x")]]
//     [[h5::ignore]]         →  [[clang::annotate("h5::ignore")]]
//     [[h5::chunk(1024)]]    →  [[clang::annotate("h5::chunk", 1024)]]
//     [[h5::compress(gzip, 6)]]
//                            →  [[clang::annotate("h5::compress", gzip, 6)]]
//
// The rewritten source goes through Clang as a standard `clang::annotate`
// annotation. The user only ever sees the clean syntax; the wrapper is internal.

#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/MemoryBuffer.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace h5_attr_translator {

// Identifiers we recognize after `h5::` inside an attribute. Anything else
// is left verbatim so user code can carry unrelated attributes alongside.
inline bool is_h5_attr_name(llvm::StringRef name) {
    return name == "name"      || name == "ignore"      || name == "chunk"
        || name == "compress"  || name == "doc"         || name == "on_missing"
        || name == "alias"     || name == "version"     || name == "name_all"
        || name == "serialize_full";
}

inline bool is_json_attr_name(llvm::StringRef name) {
    return name == "name"      || name == "ignore"      || name == "doc"
        || name == "alias"     || name == "version"     || name == "name_all"
        || name == "required"  || name == "format"      || name == "pattern"
        || name == "min"       || name == "max"         || name == "tool_format";
}

inline bool is_msgpack_attr_name(llvm::StringRef name) {
    return name == "name"      || name == "ignore"      || name == "doc"
        || name == "alias"     || name == "required"    || name == "ext";
}

// Skip whitespace and comments at position `i` in `src`. Returns the new
// position. Comments are emitted to `out` verbatim.
inline std::size_t skip_ws(llvm::StringRef src, std::size_t i, std::string& out) {
    while (i < src.size()) {
        char c = src[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            out.push_back(c); ++i;
        } else if (c == '/' && i + 1 < src.size() && src[i+1] == '/') {
            while (i < src.size() && src[i] != '\n') { out.push_back(src[i]); ++i; }
        } else if (c == '/' && i + 1 < src.size() && src[i+1] == '*') {
            out.push_back(src[i++]); out.push_back(src[i++]);
            while (i + 1 < src.size() && !(src[i] == '*' && src[i+1] == '/')) {
                out.push_back(src[i++]);
            }
            if (i + 1 < src.size()) { out.push_back(src[i++]); out.push_back(src[i++]); }
        } else {
            break;
        }
    }
    return i;
}

// Find the position of the closing `)` matching the `(` at `start`. Returns
// `start` on failure. Respects nested parens and string/char literals.
inline std::size_t find_matching_paren(llvm::StringRef src, std::size_t start) {
    if (start >= src.size() || src[start] != '(') return start;
    int depth = 1;
    std::size_t i = start + 1;
    while (i < src.size() && depth > 0) {
        char c = src[i];
        if (c == '(') ++depth;
        else if (c == ')') --depth;
        else if (c == '"' || c == '\'') {
            char q = c;
            ++i;
            while (i < src.size() && src[i] != q) {
                if (src[i] == '\\' && i + 1 < src.size()) ++i;
                ++i;
            }
        }
        if (depth == 0) return i;
        ++i;
    }
    return start;
}

// Find the matching `]]` for the `[[` at `start`. Returns `start` on failure.
inline std::size_t find_attr_close(llvm::StringRef src, std::size_t start) {
    if (start + 1 >= src.size() || src[start] != '[' || src[start+1] != '[') return start;
    std::size_t i = start + 2;
    int paren_depth = 0;
    while (i + 1 < src.size()) {
        char c = src[i];
        if (c == '(' || c == '[' || c == '{') ++paren_depth;
        else if (c == ')' || c == '}') --paren_depth;
        else if (c == ']') {
            if (paren_depth == 0 && src[i+1] == ']') return i;
            --paren_depth;
        }
        else if (c == '"' || c == '\'') {
            char q = c;
            ++i;
            while (i < src.size() && src[i] != q) {
                if (src[i] == '\\' && i + 1 < src.size()) ++i;
                ++i;
            }
        }
        ++i;
    }
    return start;
}

// Split an attribute block's contents by top-level commas.
inline std::vector<llvm::StringRef> split_attrs(llvm::StringRef block) {
    std::vector<llvm::StringRef> out;
    int depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i < block.size(); ++i) {
        char c = block[i];
        if (c == '(' || c == '[' || c == '{') ++depth;
        else if (c == ')' || c == ']' || c == '}') --depth;
        else if (c == '"' || c == '\'') {
            char q = c;
            ++i;
            while (i < block.size() && block[i] != q) {
                if (block[i] == '\\' && i + 1 < block.size()) ++i;
                ++i;
            }
        }
        else if (c == ',' && depth == 0) {
            out.push_back(block.substr(start, i - start));
            start = i + 1;
        }
    }
    out.push_back(block.substr(start));
    return out;
}

// Rewrite one attribute spec. If it starts with `h5::<recognized-name>`,
// convert to `clang::annotate("h5::<name>", <args>)`. Otherwise leave verbatim.
inline std::string rewrite_one_attr(llvm::StringRef spec) {
    std::size_t start = 0;
    while (start < spec.size() && std::isspace(static_cast<unsigned char>(spec[start]))) ++start;
    llvm::StringRef leading = spec.substr(0, start);
    llvm::StringRef body    = spec.substr(start);

    bool is_h5  = body.starts_with("h5::");
    bool is_json = body.starts_with("json::");
    if (!is_h5 && !is_json) return spec.str();
    llvm::StringRef ns = is_h5 ? "h5::" : "json::";
    bool is_h5  = body.starts_with("h5::");
    bool is_msgpack = body.starts_with("msgpack::");
    if (!is_h5 && !is_msgpack) return spec.str();
    llvm::StringRef ns = is_h5 ? "h5::" : "msgpack::";
    if (!body.starts_with(ns)) return spec.str();

    body = body.drop_front(ns.size());
    std::size_t i = 0;
    while (i < body.size()
           && (std::isalnum(static_cast<unsigned char>(body[i])) || body[i] == '_')) ++i;
    if (i == 0) return spec.str();
    llvm::StringRef name = body.substr(0, i);
    if (is_h5 && !is_h5_attr_name(name)) return spec.str();
    if (is_json && !is_json_attr_name(name)) return spec.str();
    if (is_h5 && !is_h5_attr_name(name)) return spec.str();
    if (is_msgpack && !is_msgpack_attr_name(name)) return spec.str();

    while (i < body.size() && std::isspace(static_cast<unsigned char>(body[i]))) ++i;

    std::string out;
    out.append(leading.str());
    if (i >= body.size() || body[i] != '(') {
        out.append("clang::annotate(\"");
        out.append(ns.str());
        out.append(name.str());
        out.append("\")");
        out.append(body.substr(i).str());
        return out;
    }
    std::size_t paren_end = find_matching_paren(body, i);
    if (paren_end == i) return spec.str();
    llvm::StringRef args_inside = body.substr(i + 1, paren_end - i - 1);
    out.append("clang::annotate(\"");
    out.append(ns.str());
    out.append(name.str());
    out.append("\"");
    if (!args_inside.trim().empty()) {
        out.append(", ");
        out.append(args_inside.str());
    }
    out.append(")");
    if (paren_end + 1 < body.size()) {
        out.append(body.substr(paren_end + 1).str());
    }
    return out;
}

// Whole-file rewrite. Respects // and /* */ comments and string/char literals.
inline std::string rewrite(llvm::StringRef src) {
    std::string out;
    out.reserve(src.size() + src.size() / 16);
    std::size_t i = 0, n = src.size();
    while (i < n) {
        char c = src[i];
        if (c == '/' && i + 1 < n && src[i+1] == '/') {
            while (i < n && src[i] != '\n') out.push_back(src[i++]);
            continue;
        }
        if (c == '/' && i + 1 < n && src[i+1] == '*') {
            out.push_back(src[i++]); out.push_back(src[i++]);
            while (i + 1 < n && !(src[i] == '*' && src[i+1] == '/')) out.push_back(src[i++]);
            if (i + 1 < n) { out.push_back(src[i++]); out.push_back(src[i++]); }
            continue;
        }
        if (c == '"' || c == '\'') {
            char q = c;
            out.push_back(src[i++]);
            while (i < n && src[i] != q) {
                if (src[i] == '\\' && i + 1 < n) {
                    out.push_back(src[i++]);
                    out.push_back(src[i++]);
                    continue;
                }
                out.push_back(src[i++]);
            }
            if (i < n) out.push_back(src[i++]);
            continue;
        }
        if (c == '[' && i + 1 < n && src[i+1] == '[') {
            std::size_t close = find_attr_close(src, i);
            if (close == i) {
                out.push_back(src[i++]);
                continue;
            }
            llvm::StringRef block = src.substr(i + 2, close - i - 2);
            if (block.contains("h5::") || block.contains("json::")) {
            if (block.contains("h5::") || block.contains("msgpack::")) {
                auto attrs = split_attrs(block);
                out.append("[[");
                for (std::size_t k = 0; k < attrs.size(); ++k) {
                    if (k) out.append(",");
                    out.append(rewrite_one_attr(attrs[k]));
                }
                out.append("]]");
            } else {
                out.append(src.substr(i, close + 2 - i).str());
            }
            i = close + 2;
            continue;
        }
        out.push_back(src[i++]);
    }
    return out;
}

inline std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

inline void install_virtual_files(clang::tooling::ClangTool& Tool,
                                    const std::vector<std::string>& paths,
                                    std::vector<std::string>& storage) {
    storage.reserve(paths.size());
    for (const auto& p : paths) {
        std::string content = read_file(p);
        if (content.empty()) continue;
        if (content.find("h5::") == std::string::npos && content.find("json::") == std::string::npos) continue;
        if (content.find("h5::") == std::string::npos && content.find("msgpack::") == std::string::npos) continue;
        storage.push_back(rewrite(content));
        Tool.mapVirtualFile(p, storage.back());
    }
}

} // namespace h5_attr_translator

/* Copyright (c) 2018-2026 Steven Varga, steven@vargalabs.com Toronto, ON Canada */

#pragma once

// Issue #31 Phase 1b — source-level translator for the clean `[[pb::xxx(...)]]`
// attribute syntax.
//
// Clang 20's standard-attribute parser drops the argument list for plugin-
// registered namespace-scoped attributes when they use C++11 `[[ns::name(...)]]`
// syntax (the args are parsed for `__attribute__((name(...)))` GNU syntax, but
// not for the C++11 form — empirically verified against ParsedAttrInfo plugins).
// To deliver the clean user-facing surface anyway, h5cpp-compiler rewrites the
// source before handing it to Clang Tooling:
//
//     [[pb::field(N)]]       →  [[clang::annotate("pb::field", N)]]
//     [[pb::wire(sint32)]]   →  [[clang::annotate("pb::wire", sint32)]]
//     [[pb::adapter("Ts")]]  →  [[clang::annotate("pb::adapter", "Ts")]]
//     [[pb::ignore]]         →  [[clang::annotate("pb::ignore")]]
//     [[pb::field(a), pb::adapter("X")]]
//                            →  [[clang::annotate("pb::field", a),
//                                  clang::annotate("pb::adapter", "X")]]
//
// The rewritten source goes through Clang as a standard `clang::annotate`
// annotation, whose Phase 1a multi-arg form is already understood by
// consumer_pb.hpp. The user only ever sees the clean syntax; the wrapper is
// internal.

#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/MemoryBuffer.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace pb_attr_translator {

// Identifiers we recognize after `pb::` inside an attribute. Anything else
// is left verbatim so user code can carry unrelated attributes alongside.
inline bool is_pb_attr_name(llvm::StringRef name) {
    return name == "field"  || name == "wire"       || name == "adapter"
        || name == "ignore" || name == "oneof_name" || name == "enum_zero"
        // Future-proof: add new universal/tier-2/tier-3 names here as they
        // ship. The taxonomy doc §3-§4 has the full list.
        || name == "name"   || name == "doc"        || name == "alias"
        || name == "version"|| name == "name_all"   || name == "on_missing"
        || name == "reserved" || name == "packed"   || name == "deprecated"
        || name == "package"|| name == "json_name"
        || name == "unknown_field_set" || name == "target_syntax"
        || name == "descriptor_set_out"|| name == "service"
        || name == "encode_with" || name == "decode_with"
        || name == "tier"  || name == "reject";
}

// Skip whitespace and comments at position `i` in `src`. Returns the new
// position. Comments are emitted to `out` verbatim — diagnostics keep their
// source locations roughly aligned.
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

// Try to match an identifier at position `i`; returns the identifier text and
// advances `i` past it. Empty StringRef if not an identifier start.
inline llvm::StringRef read_identifier(llvm::StringRef src, std::size_t& i) {
    auto is_id_start = [](char c){ return (c == '_') || std::isalpha(static_cast<unsigned char>(c)); };
    auto is_id_cont  = [](char c){ return (c == '_') || std::isalnum(static_cast<unsigned char>(c)); };
    if (i >= src.size() || !is_id_start(src[i])) return {};
    std::size_t start = i;
    while (i < src.size() && is_id_cont(src[i])) ++i;
    return src.substr(start, i - start);
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
// Respects nested parens (attribute args may contain `]` inside template
// argument lists or array subscripts).
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

// Split an attribute block's contents by top-level commas (commas not inside
// parens or brackets). Each piece is one attribute spec.
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

// Rewrite one attribute spec. If it starts with `pb::<recognized-name>`,
// convert to `clang::annotate("pb::<name>", <args>)`. Otherwise leave it
// verbatim.
inline std::string rewrite_one_attr(llvm::StringRef spec) {
    // Trim leading whitespace; preserve it as a prefix.
    std::size_t start = 0;
    while (start < spec.size() && std::isspace(static_cast<unsigned char>(spec[start]))) ++start;
    llvm::StringRef leading = spec.substr(0, start);
    llvm::StringRef body    = spec.substr(start);

    constexpr llvm::StringRef ns = "pb::";
    if (!body.starts_with(ns)) return spec.str();   // not a pb:: attribute

    body = body.drop_front(ns.size());
    // Identifier follows the `pb::` prefix.
    std::size_t i = 0;
    while (i < body.size()
           && (std::isalnum(static_cast<unsigned char>(body[i])) || body[i] == '_')) ++i;
    if (i == 0) return spec.str();    // empty identifier — shouldn't happen
    llvm::StringRef name = body.substr(0, i);
    if (!is_pb_attr_name(name)) return spec.str();   // not one of ours

    // Skip whitespace before `(` (if any).
    while (i < body.size() && std::isspace(static_cast<unsigned char>(body[i]))) ++i;

    std::string out;
    out.append(leading.str());
    if (i >= body.size() || body[i] != '(') {
        // No args.
        out.append("clang::annotate(\"pb::");
        out.append(name.str());
        out.append("\")");
        // Append anything trailing (shouldn't be any).
        out.append(body.substr(i).str());
        return out;
    }
    // Args: `(` ... `)`
    std::size_t paren_end = find_matching_paren(body, i);
    if (paren_end == i) return spec.str();   // unbalanced; leave alone
    llvm::StringRef args_inside = body.substr(i + 1, paren_end - i - 1);
    out.append("clang::annotate(\"pb::");
    out.append(name.str());
    out.append("\"");
    if (!args_inside.trim().empty()) {
        out.append(", ");
        out.append(args_inside.str());
    }
    out.append(")");
    // Append trailing chars after `)` (rare; usually empty).
    if (paren_end + 1 < body.size()) {
        out.append(body.substr(paren_end + 1).str());
    }
    return out;
}

// Whole-file rewrite. Respects // and /* */ comments and string/char literals
// so attribute-like text inside them stays intact.
inline std::string rewrite(llvm::StringRef src) {
    std::string out;
    out.reserve(src.size() + src.size() / 16);
    std::size_t i = 0, n = src.size();
    while (i < n) {
        char c = src[i];
        // Line comment.
        if (c == '/' && i + 1 < n && src[i+1] == '/') {
            while (i < n && src[i] != '\n') out.push_back(src[i++]);
            continue;
        }
        // Block comment.
        if (c == '/' && i + 1 < n && src[i+1] == '*') {
            out.push_back(src[i++]); out.push_back(src[i++]);
            while (i + 1 < n && !(src[i] == '*' && src[i+1] == '/')) out.push_back(src[i++]);
            if (i + 1 < n) { out.push_back(src[i++]); out.push_back(src[i++]); }
            continue;
        }
        // String / char literal.
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
        // Attribute block: `[[ ... ]]`
        if (c == '[' && i + 1 < n && src[i+1] == '[') {
            std::size_t close = find_attr_close(src, i);
            if (close == i) {
                out.push_back(src[i++]);
                continue;
            }
            llvm::StringRef block = src.substr(i + 2, close - i - 2);
            // Only rewrite if the block contains "pb::" — fast path for the
            // common case of no-pb attributes.
            if (block.contains("pb::")) {
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

// Read a file from disk into a string. Returns empty on failure.
inline std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

// Apply the rewrite to every source path in `paths`, registering each
// rewritten buffer as a virtual file overriding the on-disk copy. Skips
// files that don't reference any pb:: attribute so the common case is a
// no-op.
inline void install_virtual_files(clang::tooling::ClangTool& Tool,
                                    const std::vector<std::string>& paths,
                                    std::vector<std::string>& storage) {
    storage.reserve(paths.size());   // keep buffers alive for Tool's lifetime
    for (const auto& p : paths) {
        std::string content = read_file(p);
        if (content.empty()) continue;
        if (content.find("pb::") == std::string::npos) continue;
        storage.push_back(rewrite(content));
        Tool.mapVirtualFile(p, storage.back());
    }
}

} // namespace pb_attr_translator

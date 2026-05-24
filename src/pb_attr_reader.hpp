/* Copyright (c) 2018-2026 Steven Varga, steven@vargalabs.com Toronto, ON Canada */

#pragma once

// Commit 3 (issue #31): shared attribute readers for pb::* annotations.
//
// Both consumer_pb.hpp (.hpp shim emission) and consumer_proto.hpp
// (.proto schema emission) used to carry their own copies of the same
// AnnotateAttr-walking helpers. This header extracts the readers into
// one place so adding a new backend doesn't mean copy-pasting the
// boilerplate yet again.
//
// The user-facing rewriter (src/pb_attr_translator.hpp) lowers every
// [[pb::xxx(args)]] into a [[clang::annotate("pb::xxx", args)]] before
// Clang sees the source, so all backends see the same uniform shape in
// the AST: a `clang::AnnotateAttr` whose first string is "pb::<kind>"
// and whose trailing args are clang::Expr* nodes.
//
// Adding a new backend is, at the reader layer, just "include this
// header." The emission layer is where each backend differs.

#include <clang/AST/Attr.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pb_attr_reader {

// Lowest-level: find the first AnnotateAttr on `d` whose annotation
// string equals `kind`. Returns nullptr if none.
inline const clang::AnnotateAttr*
find_annotate(const clang::Decl* d, llvm::StringRef kind) {
    if (!d) return nullptr;
    for (const clang::Attr* attr : d->attrs()) {
        const auto* ann = llvm::dyn_cast<clang::AnnotateAttr>(attr);
        if (!ann) continue;
        if (ann->getAnnotation() == kind) return ann;
    }
    return nullptr;
}

inline bool has_attr(const clang::Decl* d, llvm::StringRef kind) {
    return find_annotate(d, kind) != nullptr;
}

// First string-literal arg in the annotation, or empty.
inline std::string read_string_arg(const clang::AnnotateAttr* ann) {
    if (!ann) return {};
    for (const clang::Expr* arg : ann->args()) {
        if (const auto* sl =
                llvm::dyn_cast<clang::StringLiteral>(arg->IgnoreParenImpCasts())) {
            return sl->getString().str();
        }
    }
    return {};
}

// All string-literal args in declaration order. Used for variadic forms
// like [[pb::reserved("old_name", "older_name", ...)]].
inline std::vector<std::string>
read_string_args(const clang::AnnotateAttr* ann) {
    std::vector<std::string> out;
    if (!ann) return out;
    for (const clang::Expr* arg : ann->args()) {
        if (const auto* sl =
                llvm::dyn_cast<clang::StringLiteral>(arg->IgnoreParenImpCasts())) {
            out.push_back(sl->getString().str());
        }
    }
    return out;
}

// All integer / enum-constant args, evaluated via Expr::EvaluateAsInt.
// Used for [[pb::field(N1, N2, ...)]] (variadic on variant for oneof),
// [[pb::reserved(N1, N2, ...)]], [[pb::version(N)]], etc.
inline std::vector<std::uint32_t>
read_int_args(const clang::AnnotateAttr* ann, clang::ASTContext& ctx) {
    std::vector<std::uint32_t> out;
    if (!ann) return out;
    for (const clang::Expr* arg : ann->args()) {
        clang::Expr::EvalResult res;
        if (arg->EvaluateAsInt(res, ctx)) {
            out.push_back(static_cast<std::uint32_t>(res.Val.getInt().getZExtValue()));
        }
    }
    return out;
}

// Single integer convenience wrapper.
inline std::optional<std::uint32_t>
read_int_arg(const clang::AnnotateAttr* ann, clang::ASTContext& ctx) {
    auto v = read_int_args(ann, ctx);
    if (v.empty()) return std::nullopt;
    return v.front();
}

// Pretty-print the first argument as source text. Used by attributes
// whose argument shape is a typed expression we want to round-trip
// verbatim (e.g. [[pb::on_missing(0.5)]] → "0.5"; [[pb::encode_with(
// &my_fn)]] → "&my_fn"). Returns nullopt if no args.
inline std::optional<std::string>
read_first_arg_text(const clang::AnnotateAttr* ann, clang::ASTContext& ctx) {
    if (!ann || ann->args().empty()) return std::nullopt;
    const clang::Expr* arg = ann->args().begin()[0];
    std::string out;
    llvm::raw_string_ostream os(out);
    arg->printPretty(os, nullptr, clang::PrintingPolicy{ctx.getLangOpts()});
    return out;
}

// ── Convenience wrappers that compose find_annotate + read_* ──────────

inline std::string
read_field_string(const clang::FieldDecl* fld, llvm::StringRef kind) {
    return read_string_arg(find_annotate(fld, kind));
}
inline std::string
read_class_string(const clang::CXXRecordDecl* rec, llvm::StringRef kind) {
    return read_string_arg(find_annotate(rec, kind));
}
inline std::string
read_enum_string(const clang::EnumDecl* ed, llvm::StringRef kind) {
    return read_string_arg(find_annotate(ed, kind));
}
inline std::vector<std::uint32_t>
read_class_ints(const clang::CXXRecordDecl* rec, llvm::StringRef kind) {
    return read_int_args(find_annotate(rec, kind), rec->getASTContext());
}
inline std::vector<std::string>
read_class_strings(const clang::CXXRecordDecl* rec, llvm::StringRef kind) {
    return read_string_args(find_annotate(rec, kind));
}
inline std::optional<std::uint32_t>
read_class_int(const clang::CXXRecordDecl* rec, llvm::StringRef kind) {
    return read_int_arg(find_annotate(rec, kind), rec->getASTContext());
}
inline std::vector<std::uint32_t>
read_field_ints(const clang::FieldDecl* fld, llvm::StringRef kind) {
    return read_int_args(find_annotate(fld, kind), fld->getASTContext());
}

} // namespace pb_attr_reader

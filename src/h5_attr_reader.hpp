/* Copyright (c) 2018-2026 Steven Varga, steven@vargalabs.com Toronto, ON Canada */

#pragma once

// Issue #32 — shared attribute readers for h5::* annotations.
//
// The user-facing rewriter (src/h5_attr_translator.hpp) lowers every
// [[h5::xxx(args)]] into a [[clang::annotate("h5::xxx", args)]] before
// Clang sees the source, so the AST consumer sees a uniform shape:
// a `clang::AnnotateAttr` whose first string is "h5::<kind>" and whose
// trailing args are clang::Expr* nodes.

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

namespace h5_attr_reader {

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

inline std::optional<std::uint32_t>
read_int_arg(const clang::AnnotateAttr* ann, clang::ASTContext& ctx) {
    auto v = read_int_args(ann, ctx);
    if (v.empty()) return std::nullopt;
    return v.front();
}

inline std::optional<std::string>
read_first_arg_text(const clang::AnnotateAttr* ann, clang::ASTContext& ctx) {
    if (!ann || ann->args().empty()) return std::nullopt;
    const clang::Expr* arg = ann->args().begin()[0];
    std::string out;
    llvm::raw_string_ostream os(out);
    arg->printPretty(os, nullptr, clang::PrintingPolicy{ctx.getLangOpts()});
    return out;
}

// Convenience wrappers

inline std::string
read_field_string(const clang::FieldDecl* fld, llvm::StringRef kind) {
    return read_string_arg(find_annotate(fld, kind));
}

inline std::string
read_class_string(const clang::CXXRecordDecl* rec, llvm::StringRef kind) {
    return read_string_arg(find_annotate(rec, kind));
}

inline std::vector<std::uint32_t>
read_field_ints(const clang::FieldDecl* fld, llvm::StringRef kind) {
    return read_int_args(find_annotate(fld, kind), fld->getASTContext());
}

inline std::vector<std::uint32_t>
read_class_ints(const clang::CXXRecordDecl* rec, llvm::StringRef kind) {
    return read_int_args(find_annotate(rec, kind), rec->getASTContext());
}

inline std::optional<std::uint32_t>
read_class_int(const clang::CXXRecordDecl* rec, llvm::StringRef kind) {
    return read_int_arg(find_annotate(rec, kind), rec->getASTContext());
}

inline std::vector<std::string>
read_class_strings(const clang::CXXRecordDecl* rec, llvm::StringRef kind) {
    return read_string_args(find_annotate(rec, kind));
}

} // namespace h5_attr_reader

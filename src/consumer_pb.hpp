/* Copyright (c) 2018-2026 Steven Varga, steven@vargalabs.com Toronto, ON Canada */

#pragma once

#include <cstdint>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/TemplateBase.h>
#include <clang/AST/Type.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>

#include "utils.hpp"

// PbTemplateCallback drives a PbProducer to emit pb::meta::descriptor_t<T>
// specializations from matched call sites (pb::encode/decode/...). Unlike
// H5TemplateCallback (which emits one register_struct<T>() function per
// matched record, with nested compound declarations inside), this callback
// emits ONE standalone descriptor_t<R> specialization per record encountered,
// in topological order (nested records before outer).
//
// Tier 1 only (stage 1):
//   - field tag from [[clang::annotate("pb::field=N")]] (required for every
//     non-static data member; missing-annotation is a compile error per CCC#7)
//   - only POD-shaped scalars + nested POD messages
//   - no std::vector / std::optional / std::variant / std::string yet (Tier 3+)
//   - no enum / Timestamp / Duration adapters yet (Tier 4)
template <typename Producer>
class PbTemplateCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    explicit PbTemplateCallback(const std::string& path){
        io.open(path);
        producer.file_begin();
    }

    ~PbTemplateCallback(){
        producer.file_end();
        io << producer;
        io.close();
    }

    bool error() const { return had_error_; }

    virtual void run(const clang::ast_matchers::MatchFinder::MatchResult& Result) override {
        const clang::CXXRecordDecl* node = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("cxxRecordDecl");
        if(!is_eligible(node)) return;
        if(!matched_.insert(node).second) return;       // dedup repeat matches

        // Topological order: emit deps-first so each descriptor references
        // already-known descriptor specializations for nested types.
        std::vector<const clang::CXXRecordDecl*> order;
        collect_deps_(node, order);

        for(const clang::CXXRecordDecl* rec : order){
            if(!emitted_.insert(rec).second) continue;
            emit_descriptor_(rec);
        }
    }

private:
    // is_user_record_ filters out stdlib types (std::, __cxx11::, __1::) so
    // members like std::string / std::vector / std::optional appear as leaf
    // pb::field<N, &T::m>{} entries instead of generating spurious descriptors
    // for stdlib internals. Stage-3+ extensions of pb.hpp (e.g. adapter_t
    // specializations for std::map → MapEntry) attach at the field-type
    // level, not by walking into the stdlib type.
    bool is_user_record_(const clang::CXXRecordDecl* rec) const {
        if(!rec || !rec->hasDefinition()) return false;
        if(rec->isUnion()) return false;
        for(const clang::DeclContext* ctx = rec->getDeclContext(); ctx; ctx = ctx->getParent()){
            if(const auto* ns = clang::dyn_cast<clang::NamespaceDecl>(ctx)){
                const std::string n = ns->getNameAsString();
                if(n == "std" || n == "__cxx11" || n == "__1") return false;
            }
        }
        return true;
    }

    bool is_eligible(const clang::CXXRecordDecl* node) const {
        // Eligible = a user-defined record that should receive its own
        // pb::meta::descriptor_t<T> specialization. POD restriction is dropped
        // (stage 2): structs with std::string / enum / std::vector / std::optional
        // / std::variant members are all eligible and emit the same shape of
        // descriptor — pb.hpp's trait dispatch handles each member type.
        return is_user_record_(node);
    }

    void collect_deps_(const clang::CXXRecordDecl* node,
                       std::vector<const clang::CXXRecordDecl*>& order){
        if(!seen_for_deps_.insert(node).second) return;
        for(const clang::FieldDecl* fld : node->fields()){
            const clang::Type* tp = fld->getType().getTypePtrOrNull();
            if(tp && tp->isRecordType()){
                const clang::CXXRecordDecl* nested = tp->getAsCXXRecordDecl();
                // Only recurse into user-defined records (not std::string,
                // std::vector, std::optional, etc.). stdlib types appear as
                // leaf fields handled by pb.hpp's trait dispatch.
                if(is_user_record_(nested))
                    collect_deps_(nested, order);
            }
        }
        order.push_back(node);
    }

    void emit_descriptor_(const clang::CXXRecordDecl* node){
        const std::string rec_name = utils::type_name(node);
        producer.template_decl(rec_name);

        for(const clang::FieldDecl* fld : node->fields()){
            // FR6/stage 4: variant fields take the oneof emission path; they
            // carry [[clang::annotate("pb::oneof_tags=N1,N2,...")]] instead
            // of pb::field=N.
            if(is_std_variant_(fld)){
                const auto alts = extract_variant_alt_types_(fld);
                const auto tags = parse_pb_oneof_tags_attr_(fld);
                if(alts.empty()){
                    emit_variant_extract_failure_(node, fld);
                    continue;
                }
                if(alts.size() != tags.size()){
                    emit_oneof_tag_mismatch_(node, fld, alts.size(), tags.size());
                    continue;
                }
                producer.emit_oneof(utils::name(fld), utils::name(node), alts, tags);
                continue;
            }

            std::optional<std::uint32_t> tag = parse_pb_field_attr_(fld);
            if(!tag){
                emit_missing_annotation_(node, fld);
                continue;
            }
            // Stage 6 (Tier 4): optional pb::adapter=Name annotation routes
            // the field through pb::Name_adapter on encode/decode. Used for
            // chrono types (std::chrono::system_clock::time_point ↔
            // Timestamp_adapter, std::chrono::nanoseconds ↔ Duration_adapter).
            const std::optional<std::string> adapter_name = parse_pb_adapter_attr_(fld);
            if(adapter_name){
                if(!validate_adapter_name_(node, fld, *adapter_name)) continue;
                producer.emit_adapter_field(*tag,
                                             utils::name(fld),
                                             utils::name(node),
                                             *adapter_name);
                continue;
            }

            // Stage 5: optional pb::wire=spec annotation pins the WireSpec
            // template arg on pb::field<N, &T::m, WireSpec>. Without it,
            // emit the 2-arg form which defaults to pb::wire::auto_t.
            const std::optional<std::string> wire_spec = parse_pb_wire_attr_(fld);
            if(wire_spec){
                if(!validate_wire_spec_(node, fld, *wire_spec)) continue;
                producer.emit_field_with_spec(*tag,
                                               utils::name(fld),
                                               utils::name(node),
                                               *wire_spec);
            } else {
                // PbProducer's type_insert overloads `var` to carry the field tag
                // as a decimal string. record_name is the qualified record;
                // field_name is the C++ member identifier.
                producer.type_insert(std::to_string(*tag),
                                      utils::name(fld),
                                      utils::name(node),
                                      utils::type_name(fld));
            }
        }
        producer.return_type("");
    }

    std::optional<std::string> parse_pb_wire_attr_(const clang::FieldDecl* fld) const {
        for(const clang::Attr* attr : fld->attrs()){
            const auto* ann = llvm::dyn_cast<clang::AnnotateAttr>(attr);
            if(!ann) continue;
            llvm::StringRef text = ann->getAnnotation();
            constexpr llvm::StringRef prefix = "pb::wire=";
            if(!text.starts_with(prefix)) continue;
            return text.drop_front(prefix.size()).str();
        }
        return std::nullopt;
    }

    bool validate_wire_spec_(const clang::CXXRecordDecl* rec, const clang::FieldDecl* fld,
                              const std::string& spec){
        static const std::set<std::string> valid_specs{
            "sint32", "sint64", "fixed32", "fixed64", "sfixed32", "sfixed64",
        };
        if(valid_specs.count(spec) == 0u){
            llvm::errs() << "h5cpp-compiler --protocol-buffers: '"
                          << utils::name(fld) << "' of '" << utils::name(rec)
                          << "' has invalid pb::wire spec '" << spec
                          << "'. Valid: sint32, sint64, fixed32, fixed64, "
                          << "sfixed32, sfixed64.\n";
            had_error_ = true;
            return false;
        }
        return true;
    }

    std::optional<std::string> parse_pb_adapter_attr_(const clang::FieldDecl* fld) const {
        for(const clang::Attr* attr : fld->attrs()){
            const auto* ann = llvm::dyn_cast<clang::AnnotateAttr>(attr);
            if(!ann) continue;
            llvm::StringRef text = ann->getAnnotation();
            constexpr llvm::StringRef prefix = "pb::adapter=";
            if(!text.starts_with(prefix)) continue;
            return text.drop_front(prefix.size()).str();
        }
        return std::nullopt;
    }

    bool validate_adapter_name_(const clang::CXXRecordDecl* rec, const clang::FieldDecl* fld,
                                 const std::string& name){
        static const std::set<std::string> valid_adapters{
            "Timestamp", "Duration",
        };
        if(valid_adapters.count(name) == 0u){
            llvm::errs() << "h5cpp-compiler --protocol-buffers: '"
                          << utils::name(fld) << "' of '" << utils::name(rec)
                          << "' has invalid pb::adapter name '" << name
                          << "'. Valid built-ins: Timestamp, Duration. (Add custom "
                          << "adapters by extending the library's pb::<Name>_adapter set.)\n";
            had_error_ = true;
            return false;
        }
        return true;
    }

    // Variant detection: a class template specialization whose template name
    // is `variant` in namespace `std` (handles std::__1, std::__cxx11).
    bool is_std_variant_(const clang::FieldDecl* fld) const {
        const clang::Type* tp = fld->getType().getTypePtrOrNull();
        if(!tp || !tp->isRecordType()) return false;
        const clang::CXXRecordDecl* rec = tp->getAsCXXRecordDecl();
        if(!rec) return false;
        const auto* spec = clang::dyn_cast<clang::ClassTemplateSpecializationDecl>(rec);
        if(!spec) return false;
        if(spec->getName() != "variant") return false;
        for(const clang::DeclContext* ctx = spec->getDeclContext(); ctx; ctx = ctx->getParent()){
            if(const auto* ns = clang::dyn_cast<clang::NamespaceDecl>(ctx)){
                const std::string n = ns->getNameAsString();
                if(n == "std") return true;
            }
        }
        return false;
    }

    // Extract the non-monostate alternative type names from a
    // std::variant<std::monostate, T1, T2, ...> specialization.
    //
    // Two-step name resolution so emitted pb::alt<...> reads as the user
    // wrote it rather than as clang canonicalized it:
    //   1. Prefer the sugared TemplateSpecializationType when clang
    //      preserved it — its template_arguments() carry the as-written
    //      types (`std::int64_t` stays `std::int64_t`, not `long`).
    //   2. Fall back to the canonical ClassTemplateSpecializationDecl args
    //      with a PrintingPolicy that drops `class`/`struct` keywords, then
    //      collapse stdlib typedef chains (basic_string<char> → string).
    std::vector<std::string> extract_variant_alt_types_(const clang::FieldDecl* fld) const {
        std::vector<std::string> out;
        const clang::QualType field_qt = fld->getType();

        clang::PrintingPolicy policy(fld->getASTContext().getLangOpts());
        policy.SuppressTagKeyword     = true;   // drop `class`/`struct`/`union`
        policy.SuppressUnwrittenScope = true;   // drop inline-ns (`std::__1::`)

        auto pretty = [&](clang::QualType qt) -> std::string {
            std::string s = qt.getAsString(policy);
            // Collapse stdlib basic_string back to its writable typedef.
            // Both the fully-defaulted and the elided-defaults forms appear
            // depending on how clang folded the template arguments.
            constexpr std::string_view prefixes[] = {
                "std::__cxx11::basic_string<char>",
                "std::basic_string<char>",
            };
            for (auto p : prefixes) {
                if (s.rfind(p, 0) == 0) { s.replace(0, p.size(), "std::string"); break; }
            }
            return s;
        };

        auto push_alt = [&](clang::QualType qt){
            std::string s = pretty(qt);
            if (s.find("monostate") != std::string::npos) return;
            out.push_back(std::move(s));
        };

        // Sugar path: as-written types from the TemplateSpecializationType.
        if (const auto* tst = field_qt->getAs<clang::TemplateSpecializationType>()) {
            for (const clang::TemplateArgument& arg : tst->template_arguments()) {
                if (arg.getKind() == clang::TemplateArgument::Type) {
                    push_alt(arg.getAsType());
                } else if (arg.getKind() == clang::TemplateArgument::Pack) {
                    for (const clang::TemplateArgument& packed : arg.getPackAsArray()) {
                        if (packed.getKind() == clang::TemplateArgument::Type)
                            push_alt(packed.getAsType());
                    }
                }
            }
            if (!out.empty()) return out;
        }

        // Canonical fallback.
        const clang::Type* tp = field_qt.getTypePtrOrNull();
        if(!tp || !tp->isRecordType()) return out;
        const auto* spec = clang::dyn_cast<clang::ClassTemplateSpecializationDecl>(
            tp->getAsCXXRecordDecl());
        if(!spec) return out;
        const clang::TemplateArgumentList& args = spec->getTemplateArgs();
        for(unsigned i = 0; i < args.size(); ++i){
            const clang::TemplateArgument& arg = args[i];
            if(arg.getKind() == clang::TemplateArgument::Type){
                push_alt(arg.getAsType());
            } else if(arg.getKind() == clang::TemplateArgument::Pack){
                for(const auto& packed : arg.getPackAsArray()){
                    if(packed.getKind() == clang::TemplateArgument::Type)
                        push_alt(packed.getAsType());
                }
            }
        }
        return out;
    }

    std::vector<std::uint32_t> parse_pb_oneof_tags_attr_(const clang::FieldDecl* fld) const {
        std::vector<std::uint32_t> out;
        for(const clang::Attr* attr : fld->attrs()){
            const auto* ann = llvm::dyn_cast<clang::AnnotateAttr>(attr);
            if(!ann) continue;
            llvm::StringRef text = ann->getAnnotation();
            constexpr llvm::StringRef prefix = "pb::oneof_tags=";
            if(!text.starts_with(prefix)) continue;
            llvm::StringRef rest = text.drop_front(prefix.size());
            while(!rest.empty()){
                auto comma = rest.find(',');
                llvm::StringRef one = (comma == llvm::StringRef::npos) ? rest : rest.substr(0, comma);
                std::uint32_t n = 0;
                if(!one.getAsInteger(10, n) && n >= 1u && n < (1u << 29))
                    out.push_back(n);
                rest = (comma == llvm::StringRef::npos) ? llvm::StringRef{} : rest.substr(comma + 1);
            }
            break;
        }
        return out;
    }

    void emit_variant_extract_failure_(const clang::CXXRecordDecl* rec, const clang::FieldDecl* fld){
        llvm::errs() << "h5cpp-compiler --protocol-buffers: variant member '"
                      << utils::name(fld) << "' of '" << utils::name(rec)
                      << "' has no extractable alternative types (empty variant?) [FR6]\n";
        had_error_ = true;
    }
    void emit_oneof_tag_mismatch_(const clang::CXXRecordDecl* rec, const clang::FieldDecl* fld,
                                    std::size_t alts, std::size_t tags){
        llvm::errs() << "h5cpp-compiler --protocol-buffers: oneof tag count mismatch on '"
                      << utils::name(fld) << "' of '" << utils::name(rec)
                      << "': " << alts << " variant alternative(s) (excluding monostate) but "
                      << tags << " tag(s) in [[clang::annotate(\"pb::oneof_tags=...\")]] [FR6]\n";
        had_error_ = true;
    }

    std::optional<std::uint32_t> parse_pb_field_attr_(const clang::FieldDecl* fld) const {
        for(const clang::Attr* attr : fld->attrs()){
            const auto* ann = llvm::dyn_cast<clang::AnnotateAttr>(attr);
            if(!ann) continue;
            llvm::StringRef text = ann->getAnnotation();
            // Match "pb::field=<digits>" — the C++17-portable annotation form
            // (Clang exposes [[clang::annotate("pb::field=N")]] via AnnotateAttr).
            constexpr llvm::StringRef prefix = "pb::field=";
            if(!text.starts_with(prefix)) continue;
            std::uint32_t n = 0;
            if(text.drop_front(prefix.size()).getAsInteger(10, n)) continue;
            if(n == 0 || n >= (1u << 29)) continue;     // proto3 valid range
            return n;
        }
        return std::nullopt;
    }

    void emit_missing_annotation_(const clang::CXXRecordDecl* rec, const clang::FieldDecl* fld){
        llvm::errs() << "h5cpp-compiler --protocol-buffers: member '"
                      << utils::name(fld) << "' of '"
                      << utils::name(rec)
                      << "' is missing [[clang::annotate(\"pb::field=N\")]]"
                      << " or has invalid N (must be in [1, 2^29-1]) [CCC#7]\n";
        had_error_ = true;
    }

    std::ofstream io;
    Producer producer;
    std::set<const clang::CXXRecordDecl*> matched_;
    std::set<const clang::CXXRecordDecl*> emitted_;
    std::set<const clang::CXXRecordDecl*> seen_for_deps_;
    bool had_error_ = false;
};

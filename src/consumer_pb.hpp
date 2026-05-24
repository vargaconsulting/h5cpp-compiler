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
#include "pb_attr_reader.hpp"

// PbTemplateCallback drives a PbProducer to emit pb::meta::descriptor_t<T>
// specializations from matched call sites (pb::encode/decode/...). Unlike
// H5TemplateCallback (which emits one register_struct<T>() function per
// matched record, with nested compound declarations inside), this callback
// emits ONE standalone descriptor_t<R> specialization per record encountered,
// in topological order (nested records before outer).
//
// Tier 1 only (stage 1):
//   - field tag from [[pb::field(N)]] (required for every
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

        // Commit 2 (Tier 4): [[pb::reject]] — emit a diagnostic and skip
        // this record entirely. Used to assert "this type is HDF5-only,
        // never on the wire."
        if (has_pb_reject_attr_(node)) {
            llvm::errs() << "h5cpp-compiler --protocol-buffers: record '"
                          << utils::name(node)
                          << "' carries [[pb::reject]] — skipping descriptor "
                          << "emission per Tier-4 explicit-reject contract.\n";
            return;
        }

        // Commit 4: [[pb::service("Name")]] — record is an RPC handler
        // surface, not a message. The .proto emitter routes it to a
        // `service` block; the .hpp shim emits NOTHING for it (services
        // aren't pb::encode/decode-able). Silent skip — no diagnostic
        // because this is intentional, unlike pb::reject.
        for (const clang::Attr* attr : node->attrs()) {
            const auto* ann = llvm::dyn_cast<clang::AnnotateAttr>(attr);
            if (ann && ann->getAnnotation() == "pb::service") return;
        }

        // Phase 2: class-level [[pb::name(...)]] / [[pb::doc(...)]] surface
        // as leading comments above the descriptor specialization. Phase 3
        // consumes the same metadata to produce the `message Foo { ... }`
        // header in the emitted .proto.
        const auto class_proto_name = parse_pb_name_attr_class_(node).value_or("");
        const auto class_doc        = parse_pb_doc_attr_class_(node).value_or("");
        producer.emit_class_metadata(class_proto_name, class_doc);

        producer.template_decl(rec_name);

        for(const clang::FieldDecl* fld : node->fields()){
            // Phase 2: capture field-level universal metadata. Emitted as
            // a leading-comment preamble before the descriptor entry.
            const auto fld_proto_name = parse_pb_name_attr_(fld).value_or("");
            const auto fld_doc        = parse_pb_doc_attr_(fld).value_or("");
            const auto fld_on_missing = parse_pb_on_missing_attr_(fld).value_or("");
            producer.emit_field_metadata(fld_proto_name, fld_doc, fld_on_missing);

            // FR12: [[pb::ignore]] — explicit opt-out, no wire emission.
            // Checked FIRST so an ignored variant member doesn't get pulled
            // into the oneof path below. Emits pb::ignore<&T::m>{} so the
            // descriptor satisfies CCC#7 (every member accounted for).
            if(has_pb_ignore_attr_(fld)){
                producer.emit_ignore(utils::name(fld), utils::name(node));
                continue;
            }

            // FR6/stage 4: variant fields take the oneof emission path; they
            // carry the variadic [[pb::field(N1, N2, ...)]] form rather
            // than the single-tag form parsed by parse_pb_field_attr_.
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
            // Stage 6 (Tier 4): optional [[pb::adapter("Name")]] annotation
            // routes the field through pb::Name_adapter on encode/decode.
            // Used for chrono types (std::chrono::system_clock::time_point ↔
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

            // Stage 5: optional [[pb::wire(spec)]] annotation pins the
            // WireSpec template arg on pb::field<N, &T::m, WireSpec>.
            // Without it, emit the 2-arg form which defaults to pb::wire::auto_t.
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

        // Commit 3: emit pb::default_for_t<T, &T::m> specializations next
        // to the descriptor for each field carrying [[pb::on_missing(v)]].
        // The pb.hpp runtime (sandbox issue #4 — pb::default_for_t) reads
        // these at decode time via pb::impl::apply_defaults. Skip fields
        // without on_missing OR whose primary descriptor entry kind isn't
        // a plain pb::field (adapter / oneof / ignore don't participate).
        // Skip string defaults too — constexpr-std::string requires C++23+
        // and isn't reliably portable in v1; tracked as taxonomy §11 open
        // question for later expansion.
        for (const clang::FieldDecl* fld : node->fields()) {
            if (has_pb_ignore_attr_(fld)) continue;
            if (is_std_variant_(fld))    continue;
            if (parse_pb_adapter_attr_(fld)) continue;
            const auto value = parse_pb_on_missing_attr_(fld);
            if (!value) continue;
            const std::string type_name = utils::type_name(fld);
            if (type_name.find("basic_string") != std::string::npos) {
                llvm::errs() << "h5cpp-compiler --protocol-buffers: '"
                              << utils::name(fld) << "' of '" << utils::name(node)
                              << "' carries [[pb::on_missing(...)]] on a string-typed "
                              << "field — string defaults skipped in v1 (constexpr "
                              << "std::string requires C++23+; primitive defaults "
                              << "only). The .hpp shim shows the user's intent as "
                              << "a comment.\n";
                continue;
            }
            producer.emit_default_for(utils::name(fld),
                                       utils::name(node),
                                       type_name,
                                       *value);
        }
    }

    // ── Issue #31 attribute readers ──────────────────────────────────────
    //
    // User-facing surface is the clean [[pb::field(N)]] / [[pb::wire(spec)]]
    // / [[pb::adapter("Name")]] / [[pb::ignore]] vocabulary. The source
    // rewriter (src/pb_attr_translator.hpp) lowers each clean attribute to
    // an equivalent `clang::annotate("pb::<kind>", args...)` form before
    // Clang's parser sees it; that intermediate representation lands in the
    // AST as an AnnotateAttr whose trailing args are Expr* nodes, so
    // Expr::EvaluateAsInt(ctx) resolves both integer literals and enum
    // constant references uniformly. The internal envelope is invisible to
    // users — only the rewriter and the readers below touch it.

    // Commit 3 refactor: thin wrappers around pb_attr_reader::. The
    // canonical implementations live in pb_attr_reader.hpp so a new
    // backend (JSON, SQL, Avro) reuses the SAME readers without copy-
    // pasting the AnnotateAttr-walking boilerplate.
    const clang::AnnotateAttr*
    find_pb_annotate_(const clang::FieldDecl* fld, llvm::StringRef kind) const {
        return pb_attr_reader::find_annotate(fld, kind);
    }

    std::vector<std::uint32_t>
    read_int_args_(const clang::AnnotateAttr* ann, clang::ASTContext& ctx) const {
        return pb_attr_reader::read_int_args(ann, ctx);
    }

    std::optional<std::string>
    read_string_arg_(const clang::AnnotateAttr* ann) const {
        std::string s = pb_attr_reader::read_string_arg(ann);
        if (s.empty()) return std::nullopt;
        return s;
    }

    std::optional<std::string> parse_pb_wire_attr_(const clang::FieldDecl* fld) const {
        // User writes [[pb::wire(sint32)]]; the source rewriter lowers it to
        // [[clang::annotate("pb::wire", "sint32")]] which lands in the AST
        // as an AnnotateAttr. Internal plumbing only — users never type
        // the clang::annotate envelope.
        if(const auto* ann = find_pb_annotate_(fld, "pb::wire")){
            if(auto s = read_string_arg_(ann); s) return s;
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
        // User writes [[pb::adapter("Timestamp")]] (or "Duration"); the
        // source rewriter lowers it to clang::annotate. Same internal-only
        // plumbing as parse_pb_wire_attr_.
        if(const auto* ann = find_pb_annotate_(fld, "pb::adapter")){
            if(auto s = read_string_arg_(ann); s) return s;
        }
        return std::nullopt;
    }

    // FR12: user writes [[pb::ignore]] (no args); the source rewriter
    // lowers it to clang::annotate("pb::ignore") with an empty arg list.
    // Predicate, not a value-bearing parser — presence of the kind is the
    // whole answer.
    bool has_pb_ignore_attr_(const clang::FieldDecl* fld) const {
        return find_pb_annotate_(fld, "pb::ignore") != nullptr;
    }

    // Phase 2 — universal attribute readers.
    //
    // [[pb::name("on_wire_name")]]   — string arg; renames the field on
    //                                   the wire (proto3 field name in
    //                                   the .proto, ignored by pb.hpp's
    //                                   runtime since the wire uses tag
    //                                   numbers, not names).
    // [[pb::doc("description")]]      — string arg; emitted as a trailing
    //                                   `//` comment in the generated
    //                                   .proto for self-documenting
    //                                   schemas.
    // [[pb::on_missing(value)]]       — value arg (int, double, or string
    //                                   literal); intended default the
    //                                   decoder applies when the wire
    //                                   field is absent. Runtime support
    //                                   in pb.hpp lands in a separate
    //                                   commit (cross-repo).
    // All three are emitted today as leading `//` comments in the .hpp
    // shim — the Phase 3 .proto emitter consumes the same parsers to
    // produce actual schema output.
    std::optional<std::string> parse_pb_name_attr_(const clang::FieldDecl* fld) const {
        if (const auto* ann = find_pb_annotate_(fld, "pb::name")) {
            return read_string_arg_(ann);
        }
        return std::nullopt;
    }
    std::optional<std::string> parse_pb_doc_attr_(const clang::FieldDecl* fld) const {
        if (const auto* ann = find_pb_annotate_(fld, "pb::doc")) {
            return read_string_arg_(ann);
        }
        return std::nullopt;
    }
    // Reads the raw textual representation of the on_missing arg (so int,
    // float, and string-literal forms all round-trip into the .hpp comment
    // without parsing them as a typed value).
    std::optional<std::string> parse_pb_on_missing_attr_(const clang::FieldDecl* fld) const {
        const auto* ann = find_pb_annotate_(fld, "pb::on_missing");
        if (!ann || ann->args().empty()) return std::nullopt;
        // Take the first arg's source text via the Expr's pretty-printer.
        const clang::Expr* arg = ann->args().begin()[0];
        std::string out;
        llvm::raw_string_ostream os(out);
        arg->printPretty(os, nullptr,
                          clang::PrintingPolicy{fld->getASTContext().getLangOpts()});
        return out;
    }

    // Class-level variants — same readers, taking a record decl. Used for
    // class-scope [[pb::name(...)]] (proto3 message rename) and
    // [[pb::doc(...)]] (message-level doc comment).
    std::optional<std::string> parse_pb_name_attr_class_(const clang::CXXRecordDecl* rec) const {
        for (const clang::Attr* attr : rec->attrs()) {
            const auto* ann = llvm::dyn_cast<clang::AnnotateAttr>(attr);
            if (!ann) continue;
            if (ann->getAnnotation() != "pb::name") continue;
            return read_string_arg_(ann);
        }
        return std::nullopt;
    }
    std::optional<std::string> parse_pb_doc_attr_class_(const clang::CXXRecordDecl* rec) const {
        for (const clang::Attr* attr : rec->attrs()) {
            const auto* ann = llvm::dyn_cast<clang::AnnotateAttr>(attr);
            if (!ann) continue;
            if (ann->getAnnotation() != "pb::doc") continue;
            return read_string_arg_(ann);
        }
        return std::nullopt;
    }

    // Commit 2: [[pb::reject]] on a class declaration is a presence check.
    bool has_pb_reject_attr_(const clang::CXXRecordDecl* rec) const {
        for (const clang::Attr* attr : rec->attrs()) {
            const auto* ann = llvm::dyn_cast<clang::AnnotateAttr>(attr);
            if (!ann) continue;
            if (ann->getAnnotation() == "pb::reject") return true;
        }
        return false;
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
        // User writes [[pb::field(N1, N2, N3, ...)]] on a std::variant
        // member — the variadic form. After the source rewriter lowers
        // it to clang::annotate, EvaluateAsInt accepts both integer
        // literals and enum constant references in one code path.
        if(const auto* ann = find_pb_annotate_(fld, "pb::field")){
            auto ints = read_int_args_(ann, fld->getASTContext());
            if(!ints.empty()) return ints;
        }
        return {};
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
                      << tags << " tag(s) in [[pb::field(N1, N2, ...)]] [FR6]\n";
        had_error_ = true;
    }

    std::optional<std::uint32_t> parse_pb_field_attr_(const clang::FieldDecl* fld) const {
        // User writes [[pb::field(N)]] with N an integer literal OR an enum
        // value — EvaluateAsInt handles both. The source rewriter lowers
        // this to clang::annotate before Clang sees it. A multi-tag form
        // on a non-variant field is the wrong shape; the variant path
        // picks it up via parse_pb_oneof_tags_attr_ instead.
        if(const auto* ann = find_pb_annotate_(fld, "pb::field")){
            auto ints = read_int_args_(ann, fld->getASTContext());
            if(ints.size() == 1u){
                const std::uint32_t n = ints[0];
                if(n >= 1u && n < (1u << 29)) return n;
            }
        }
        return std::nullopt;
    }

    void emit_missing_annotation_(const clang::CXXRecordDecl* rec, const clang::FieldDecl* fld){
        llvm::errs() << "h5cpp-compiler --protocol-buffers: member '"
                      << utils::name(fld) << "' of '"
                      << utils::name(rec)
                      << "' is missing [[pb::field(N)]]"
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

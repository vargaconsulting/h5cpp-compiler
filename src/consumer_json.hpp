/* Copyright (c) 2018-2026 Steven Varga, steven@vargalabs.com Toronto, ON Canada */

#pragma once

#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <vector>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "utils.hpp"
#include "h5_attr_reader.hpp"

// Issue #33 — JSON Schema backend emitter.
//
// Walks the same AST as the HDF5 backend, but emits a JSON Schema document
// describing each matched struct. Nested structs are emitted into $defs and
// referenced via $ref.
//
// All AST-dependent work happens inside run() while the AST is still alive.
// The destructor only formats and writes the accumulated JSON fragments.

class JsonTemplateCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
	explicit JsonTemplateCallback(const std::string& output_path)
		: output_path_(output_path), first_record_(nullptr) {}

	~JsonTemplateCallback() {
		std::ofstream io(output_path_);
		if (!io) return;

		io << "{\n";
		io << "  \"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n";
		io << "  \"$id\": \"\",\n";

		if (first_record_) {
			io << "  \"title\": \"" << escape_json_(title_) << "\",\n";
			io << "  \"description\": \"" << escape_json_(description_) << "\",\n";
		}

		if (!defs_json_.empty()) {
			io << "  \"$defs\": {\n" << defs_json_ << "\n  },\n";
		}

		if (first_record_) {
			io << "  \"type\": \"object\",\n";
			io << props_json_ << "\n";
		} else {
			io << "  \"type\": \"object\",\n";
			io << "  \"properties\": {}\n";
		}
		io << "}\n";
	}

	void run(const clang::ast_matchers::MatchFinder::MatchResult& Result) override {
		const clang::CXXRecordDecl* node = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("cxxRecordDecl");
		if (!node) node = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("classDecl");
		if (!node) return;

		auto it = emitted_.insert(node);
		if (!it.second) return;

		if (!first_record_) {
			first_record_ = node;
			title_ = h5_attr_reader::read_class_string(node, "json::alias");
			if (title_.empty()) title_ = utils::type_name(node);
			description_ = h5_attr_reader::read_class_string(node, "json::doc");

			std::ostringstream props;
			emit_properties_(props, node, 1);
			props_json_ = props.str();
		}

		// Emit $defs for all dependencies (excluding the first record)
		std::set<const clang::CXXRecordDecl*> deps;
		collect_deps_(node, deps);
		std::ostringstream defs;
		bool first_def = defs_json_.empty();
		for (auto* dep : deps) {
			if (dep == first_record_) continue;
			if (!emitted_.insert(dep).second) continue;
			if (!first_def) defs << ",\n";
			first_def = false;
			emit_record_schema_(defs, dep, 2);
		}
		if (!defs.str().empty()) {
			if (!defs_json_.empty()) defs_json_ += ",\n";
			defs_json_ += defs.str();
		}
	}

private:
	std::string output_path_;
	std::set<const clang::CXXRecordDecl*> emitted_;
	const clang::CXXRecordDecl* first_record_;
	std::string title_;
	std::string description_;
	std::string defs_json_;
	std::string props_json_;

	static std::string escape_json_(const std::string& s) {
		std::string out;
		out.reserve(s.size());
		for (char c : s) {
			switch (c) {
				case '"': out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\b': out += "\\b"; break;
				case '\f': out += "\\f"; break;
				case '\n': out += "\\n"; break;
				case '\r': out += "\\r"; break;
				case '\t': out += "\\t"; break;
				default: out += c; break;
			}
		}
		return out;
	}

	static std::string indent_(int level) {
		return std::string(level * 2, ' ');
	}

	void collect_deps_(const clang::CXXRecordDecl* node, std::set<const clang::CXXRecordDecl*>& deps) {
		if (!deps.insert(node).second) return;
		for (clang::FieldDecl* fld : node->fields()) {
			clang::QualType qt = fld->getType();
			const clang::Type* tp = qt.getTypePtrOrNull();
			if (!tp) continue;
			if (tp->isRecordType() && !utils::is_vector_type(qt) && !utils::is_string_type(qt)) {
				if (auto* rec = tp->getAsCXXRecordDecl()) {
					if (rec->getDefinition()) collect_deps_(rec->getDefinition(), deps);
				}
			}
			if (tp->isConstantArrayType()) {
				clang::QualType et = qt->getAsArrayTypeUnsafe()->getElementType();
				const clang::Type* etp = et.getTypePtrOrNull();
				if (etp && etp->isRecordType()) {
					if (auto* rec = etp->getAsCXXRecordDecl()) {
						if (rec->getDefinition()) collect_deps_(rec->getDefinition(), deps);
					}
				}
			}
			if (utils::is_vector_type(qt)) {
				clang::QualType et = utils::get_vector_element_type(qt);
				const clang::Type* etp = et.getTypePtrOrNull();
				if (etp && etp->isRecordType()) {
					if (auto* rec = etp->getAsCXXRecordDecl()) {
						if (rec->getDefinition()) collect_deps_(rec->getDefinition(), deps);
					}
				}
			}
		}
	}

	std::string json_schema_type_(clang::QualType qt) const {
		const clang::Type* tp = qt.getTypePtrOrNull();
		if (!tp) return "{\"type\": \"object\"}";

		if (tp->isBuiltinType()) {
			clang::QualType ct = qt->getCanonicalTypeInternal();
			std::string name = ct.getAsString();
			if (name == "bool" || name == "_Bool") return "{\"type\": \"boolean\"}";
			if (name == "char" || name == "signed char" || name == "unsigned char") return "{\"type\": \"integer\"}";
			if (name == "short" || name == "unsigned short") return "{\"type\": \"integer\"}";
			if (name == "int" || name == "unsigned int") return "{\"type\": \"integer\"}";
			if (name == "long" || name == "unsigned long") return "{\"type\": \"integer\"}";
			if (name == "long long" || name == "unsigned long long") return "{\"type\": \"integer\", \"format\": \"int64\"}";
			if (name == "float") return "{\"type\": \"number\"}";
			if (name == "double" || name == "long double") return "{\"type\": \"number\"}";
			return "{\"type\": \"object\"}";
		}

		if (tp->isEnumeralType()) {
			return "{\"type\": \"string\"}";
		}

		if (utils::is_string_type(qt)) {
			return "{\"type\": \"string\"}";
		}

		if (utils::is_vector_type(qt)) {
			clang::QualType et = utils::get_vector_element_type(qt);
			return "{\"type\": \"array\", \"items\": " + json_schema_type_(et) + "}";
		}

		if (tp->isConstantArrayType()) {
			clang::QualType et = qt->getAsArrayTypeUnsafe()->getElementType();
			return "{\"type\": \"array\", \"items\": " + json_schema_type_(et) + "}";
		}

		if (tp->isRecordType() && !utils::is_vector_type(qt) && !utils::is_string_type(qt)) {
			clang::CXXRecordDecl* rec = tp->getAsCXXRecordDecl();
			if (rec) {
				std::string ref_name = utils::type_name(rec);
				return "{\"$ref\": \"#/$defs/" + escape_json_(ref_name) + "\"}";
			}
		}

		return "{\"type\": \"object\"}";
	}

	void emit_record_schema_(std::ostream& io, const clang::CXXRecordDecl* node, int indent_level) const {
		std::string name = utils::type_name(node);
		io << indent_(indent_level) << "\"" << escape_json_(name) << "\": {\n";
		io << indent_(indent_level + 1) << "\"type\": \"object\",\n";

		std::string doc = h5_attr_reader::read_class_string(node, "json::doc");
		if (!doc.empty()) {
			io << indent_(indent_level + 1) << "\"description\": \"" << escape_json_(doc) << "\",\n";
		}

		emit_properties_(io, node, indent_level + 1);
		io << "\n" << indent_(indent_level) << "}";
	}

	void emit_properties_(std::ostream& io, const clang::CXXRecordDecl* node, int indent_level) const {
		std::vector<std::string> required;

		io << indent_(indent_level) << "\"properties\": {\n";
		bool first = true;
		for (clang::FieldDecl* fld : node->fields()) {
			if (h5_attr_reader::has_attr(fld, "json::ignore")) continue;

			std::string json_name = h5_attr_reader::read_field_string(fld, "json::name");
			if (json_name.empty()) json_name = utils::name(fld);

			if (!first) io << ",\n";
			first = false;

			io << indent_(indent_level + 1) << "\"" << escape_json_(json_name) << "\": ";

			clang::QualType qt = fld->getType();
			std::string schema = json_schema_type_(qt);

			std::string format = h5_attr_reader::read_field_string(fld, "json::format");
			if (!format.empty()) {
				size_t pos = schema.rfind('}');
				if (pos != std::string::npos) {
					schema.insert(pos, ", \"format\": \"" + escape_json_(format) + "\"");
				}
			}

			io << schema;

			if (h5_attr_reader::has_attr(fld, "json::required")) {
				required.push_back(json_name);
			}
		}
		io << "\n" << indent_(indent_level) << "}";

		if (!required.empty()) {
			io << ",\n" << indent_(indent_level) << "\"required\": [";
			for (std::size_t i = 0; i < required.size(); ++i) {
				if (i > 0) io << ", ";
				io << "\"" << escape_json_(required[i]) << "\"";
			}
			io << "]";
		}
	}
};

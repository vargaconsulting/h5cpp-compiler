/* Copyright (c) 2018-2026 Steven Varga, steven@vargalabs.com Toronto, ON Canada */

#pragma once

#include <cstdint>
#include <string>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>

namespace utils {

	inline std::string get_type_name(const clang::QualType& qt );
	enum class type{ builtin, array, record, invalid };
	enum class tier { pod, scatter, invalid };

	template <typename T, typename F> T as( F );

	// Issue #32: detect std::vector<T> in Clang AST
	inline bool is_vector_type(const clang::QualType& qt) {
		const clang::Type* tp = qt.getTypePtrOrNull();
		if (!tp) return false;
		if (const auto* tst = tp->getAs<clang::TemplateSpecializationType>()) {
			if (const auto* td = tst->getTemplateName().getAsTemplateDecl()) {
				return td->getNameAsString() == "vector";
			}
		}
		return false;
	}

	// Issue #32: detect std::string (std::basic_string<char>)
	inline bool is_string_type(const clang::QualType& qt) {
		const clang::Type* tp = qt.getTypePtrOrNull();
		if (!tp) return false;
		std::string name = qt.getAsString();
		return name == "std::string" || name == "std::basic_string<char>" ||
		       name.find("std::basic_string<char,") == 0;
	}

	// Issue #32: get element type of std::vector<T>
	inline clang::QualType get_vector_element_type(const clang::QualType& qt) {
		const clang::Type* tp = qt.getTypePtrOrNull();
		if (const auto* tst = tp->getAs<clang::TemplateSpecializationType>()) {
			auto args = tst->template_arguments();
			if (!args.empty() && args[0].getKind() == clang::TemplateArgument::Type)
				return args[0].getAsType();
		}
		return clang::QualType();
	}

	// Issue #32: classify a struct into tier 1 (POD) or tier 2 (scatter)
	inline tier determine_tier(const clang::CXXRecordDecl* node) {
		bool has_scatter_field = false;
		for (clang::FieldDecl* fld : node->fields()) {
			clang::QualType qt = fld->getType();
			const clang::Type* tp = qt.getTypePtrOrNull();
			if (!tp) return tier::invalid;

			if (is_vector_type(qt) || is_string_type(qt)) {
				has_scatter_field = true;
				// For vectors, ensure element type is scalar/POD
				if (is_vector_type(qt)) {
					clang::QualType et = get_vector_element_type(qt);
					const clang::Type* etp = et.getTypePtrOrNull();
					if (!etp || !(etp->isBuiltinType() || etp->isEnumeralType()))
						return tier::invalid; // vector of non-scalar not yet supported
				}
				continue;
			}

			// Check if field is POD scalar, array, or record
			if (tp->isBuiltinType() || tp->isEnumeralType()) continue;
			if (tp->isConstantArrayType()) continue;
			if (tp->isRecordType() && qt->getAsCXXRecordDecl()->isPOD()) continue;

			return tier::invalid; // unsupported field type
		}
		return has_scatter_field ? tier::scatter : tier::pod;
	}

	template <typename T> uint64_t size( const T* ptr );
	template <typename T> std::string type_name( const T* ptr );
	template <typename T> std::string name( const T* ptr );
	template <typename T> std::string element_type_name( const T* ptr );

	template <> inline std::string type_name( const clang::CXXRecordDecl* ptr ){
   		 return ptr->getQualifiedNameAsString();
	}

	template <> inline std::string type_name( const clang::ConstantArrayType* ptr ){
		return ptr->desugar().getAsString();
	}

	template <> inline std::string type_name<>(const clang::FieldDecl* field ){
		const clang::QualType qt = field->getType();
		auto type = get_type_name( qt );
		return type;
	}

	template <> inline std::string name<>(const clang::FieldDecl* field ){
		return field->getNameAsString();
	}

	template <> inline std::string name<>(const clang::CXXRecordDecl* field ){
		return field->getQualifiedNameAsString();
	}

	template <> inline uint64_t size( const clang::ConstantArrayType* ptr ){
  		return ptr->getSize().getLimitedValue();
	}

	template <> inline std::string element_type_name( const clang::ConstantArrayType* ptr ){
		return get_type_name( ptr->getElementType());
   	}
	// conversions
	template <> inline const clang::ConstantArrayType* as<>( clang::QualType qt ){
		const clang::Type* tp = qt.getTypePtrOrNull();
		return (const clang::ConstantArrayType*) tp;
	}

	template <> inline clang::CXXRecordDecl const* as<>(  clang::QualType qt ){
		const clang::Type* tp = qt.getTypePtrOrNull();
		return tp->getAsCXXRecordDecl();
	}

	template <> inline utils::type as<>( clang::QualType qt ){
		const clang::Type* tp = qt.getTypePtrOrNull();
		if( tp->isConstantArrayType() )	return type::array;
		else if( tp->isBuiltinType() ) return type::builtin;
		else if( tp->isEnumeralType() ) return type::builtin;
		else if( tp->isRecordType() &&
			as<const clang::CXXRecordDecl*>(qt)->isPOD() ) return type::record;
		else
			return type::invalid;
	}

	template <> inline const clang::QualType as<>(  clang::FieldDecl* field ){
		return field->getType();
	}
	template <> inline const clang::ConstantArrayType* as<>(  const void* ptr ){
		return (const clang::ConstantArrayType*) ptr;
	}
	template <> inline const clang::CXXRecordDecl* as<>(  const void* ptr ){
		return (const clang::CXXRecordDecl*) ptr;
	}

	inline std::string get_type_name(const clang::QualType& qt ){
		const clang::Type* tp = qt.getTypePtrOrNull();

		if( tp->isBuiltinType() ){
			clang::QualType dqt = qt->getCanonicalTypeInternal();
			return dqt.getAsString();
		} else if( tp->isEnumeralType() ) {
			if( const auto* et = tp->getAs<clang::EnumType>() )
				return get_type_name( et->getDecl()->getIntegerType() );
			return "int";
		} else if( tp->isRecordType() ){
			clang::CXXRecordDecl* node = tp->getAsCXXRecordDecl();
			return type_name( node );
		} else if( tp->isConstantArrayType() )
			return type_name( (const clang::ConstantArrayType*) tp);
		else
			return "<unknown_type>";
	}
}


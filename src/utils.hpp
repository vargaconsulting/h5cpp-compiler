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

	template <typename T> uint64_t size( const T* ptr );
	template <typename T> std::string type_name( const T* ptr );
	template <typename T> std::string name( const T* ptr );
	template <typename T> std::string element_type_name( const T* ptr );
	template <typename T, typename F> T as( F );

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
		} else if( tp->isRecordType() ){
			clang::CXXRecordDecl* node = tp->getAsCXXRecordDecl();
			return type_name( node );
		} else if( tp->isConstantArrayType() )
			return type_name( (const clang::ConstantArrayType*) tp);
		else
			return "<unknown_type>";
	}
}


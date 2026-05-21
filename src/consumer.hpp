/* Copyright (c) 2018-2026 Steven Varga, steven@vargalabs.com Toronto, ON Canada */

#pragma once

#include <fstream>
#include <queue>
#include <set>
#include <string>
#include <utility>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "utils.hpp"

template <typename Producer> class H5TemplateCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
public :

	H5TemplateCallback(const std::string& path){
		io.open( path );
		producer.file_begin();
	}

	~H5TemplateCallback(){
		producer.file_end();
		io << producer;
		io.close();
	}

	virtual void run(const clang::ast_matchers::MatchFinder::MatchResult& Result){
		const clang::CXXRecordDecl* node = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("cxxRecordDecl");
		auto didnt_try = nodes.insert( node );
		// save this, we'll need it to map typedef-s to builtin types
		if( !node || !node->isPOD() || !didnt_try.second  ) return;

		topological_order( node );

		std::string var, type, rn;
		rn = utils::type_name(node);
		producer.template_decl( rn );

		while( !store.empty() ){
			auto node = store.front();

			const clang::ConstantArrayType* ar;
			const clang::CXXRecordDecl* re;

			switch( node.first ){
				case utils::type::array:
					ar = utils::as<const clang::ConstantArrayType*>( node.second );
					type = producer.cache( utils::element_type_name( ar ) );
					var  = producer.array_decl( type, utils::size( ar ));
					producer.cache_add( utils::type_name( ar ), var );
				break;
				case utils::type::record:
					re = utils::as<const clang::CXXRecordDecl*>( node.second );
					var = producer.record_decl( utils::type_name( re ) );
					for(clang::FieldDecl* fld: re->fields() ){
						type = producer.cache( utils::type_name( fld ) );
						producer.type_insert(var, utils::name( fld ), utils::name( re ), type );
					}
					producer.cache_add( utils::type_name( re ), var );
				break;
				default:
					break;
			}
			store.pop();
		}
		producer.type_release();
		producer.return_type( var );
		unique.clear();
	}

private:
	void topological_order(const clang::CXXRecordDecl* node){
		for(clang::FieldDecl* fld: node->fields() )
			topological_order( utils::as<const clang::QualType>( fld ) );
		auto it = unique.insert( node );
		if( it.second )
			store.push( {utils::type::record, node} );
	}

	void topological_order(const clang::QualType& qt){
		std::pair<std::set<const void*>::const_iterator, bool> it;
		const clang::ConstantArrayType* ar;

		switch( utils::as<utils::type>( qt ) ){
			case utils::type::array:
				ar = utils::as<const clang::ConstantArrayType*>(qt);
				topological_order( ar->getElementType() );
				it = unique.insert( ar );
				if( it.second )
				   	store.push({utils::type::array, ar});
				break;

			case utils::type::record:
				topological_order( utils::as<const clang::CXXRecordDecl*>( qt ) );
				break;
			default:
				;
		}
	}

	std::ofstream io;
	Producer producer;
	std::set<const void*> unique, nodes;
	std::queue<std::pair<utils::type, const void*>> store;
};

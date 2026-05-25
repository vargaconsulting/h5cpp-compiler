/* Copyright (c) 2018-2026 Steven Varga, steven@vargalabs.com Toronto, ON Canada */

#pragma once

#include <fstream>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "utils.hpp"
#include "h5_attr_reader.hpp"

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
		if( !node || !didnt_try.second ) return;

		// Issue #32: read class-level metadata attributes
		std::string doc     = h5_attr_reader::read_class_string(node, "h5::doc");
		std::string alias   = h5_attr_reader::read_class_string(node, "h5::alias");
		std::string version = h5_attr_reader::read_class_string(node, "h5::version");
		std::string on_missing = h5_attr_reader::read_class_string(node, "h5::on_missing");
		std::vector<std::string> name_all_parts = h5_attr_reader::read_class_strings(node, "h5::name_all");
		std::string name_prefix = name_all_parts.size() > 0 ? name_all_parts[0] : "";
		std::string name_suffix = name_all_parts.size() > 1 ? name_all_parts[1] : "";
		bool serialize_full = h5_attr_reader::has_attr(node, "h5::serialize_full");

		// Issue #32: classify struct tier (pod / scatter / invalid)
		// serialize_full forces POD emission (non-POD fields are implicitly skipped)
		utils::tier tier = serialize_full ? utils::tier::pod : utils::determine_tier(node);
		if( tier == utils::tier::invalid ) return;

		std::string rn = utils::type_name(node);

		if( tier == utils::tier::pod ){
			// --- tier 1: standard register_struct emission ---
			topological_order( node );
				// collect header files for all records before emitting preamble
				if( Result.Context ) {
					for( const auto& item : store ) {
						if( item.first == utils::type::record ) {
							auto rec = utils::as<const clang::CXXRecordDecl*>( item.second );
							std::string hdr = get_header_name(rec, Result.Context->getSourceManager());
							if( !hdr.empty() )
								producer.add_include(hdr);
						}
					}
				}

			producer.template_decl( rn, doc, alias, version );

			std::string var, type;
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
						if( Result.Context ) {
							std::string hdr = get_header_name(re, Result.Context->getSourceManager());
							if( !hdr.empty() )
								producer.add_include(hdr);
						}
						var = producer.record_decl( utils::type_name( re ) );
						for(clang::FieldDecl* fld: re->fields() ){
							// Issue #32: skip fields annotated with [[h5::ignore]]
							if (h5_attr_reader::has_attr(fld, "h5::ignore"))
								continue;
							// Issue #32: with serialize_full, skip non-POD fields
							if (serialize_full) {
								clang::QualType qt = fld->getType();
								if (utils::as<utils::type>(qt) == utils::type::invalid)
									continue;
							}
							type = producer.cache( utils::type_name( fld ) );
							// Issue #32: read [[h5::name("on_disk_name")]] override
							std::string on_disk_name = h5_attr_reader::read_field_string(fld, "h5::name");
							if (on_disk_name.empty()) {
								on_disk_name = name_prefix + utils::name(fld) + name_suffix;
							}
							producer.type_insert(var, utils::name( fld ), utils::name( re ), type, on_disk_name);
						}
						producer.cache_add( utils::type_name( re ), var );
					break;
					default:
						break;
				}
				store.pop_front();
			}
			producer.type_release();
			producer.return_type( var );
			unique.clear();
		} else if( tier == utils::tier::scatter ){
			// --- tier 2: scatter/gather emission ---
			std::vector<typename Producer::scatter_field_t> fields;
			for( clang::FieldDecl* fld : node->fields() ){
				// Issue #32: skip fields annotated with [[h5::ignore]]
				if( h5_attr_reader::has_attr(fld, "h5::ignore") )
					continue;

				typename Producer::scatter_field_t info;
				info.cpp_name = utils::name(fld);
				info.h5_name = h5_attr_reader::read_field_string(fld, "h5::name");
				if( info.h5_name.empty() )
					info.h5_name = name_prefix + info.cpp_name + name_suffix;

				clang::QualType qt = fld->getType();
				if( utils::is_vector_type(qt) || utils::is_string_type(qt) ){
					info.is_vlen = true;
					info.is_string = utils::is_string_type(qt);
					if( info.is_string ){
						info.cpp_type = "std::string";
						info.h5_type = ""; // strings use special H5Tcopy(H5T_C_S1) path
					} else {
						clang::QualType et = utils::get_vector_element_type(qt);
						info.cpp_type = utils::get_type_name(et);
						info.h5_type = producer.cache(info.cpp_type);
					}
				} else {
					info.is_vlen = false;
					info.is_string = false;
					info.cpp_type = utils::type_name(fld);
					info.h5_type = producer.cache(info.cpp_type);
				}
				fields.push_back(info);
			}
			// Issue #32: read class-level [[h5::chunk]] and [[h5::compress]]
			std::vector<std::uint32_t> chunk_vals = h5_attr_reader::read_class_ints(node, "h5::chunk");
			std::string chunk_size = chunk_vals.empty() ? "64" : std::to_string(chunk_vals[0]);

			std::vector<std::string> compress_strs = h5_attr_reader::read_class_strings(node, "h5::compress");
			std::vector<std::uint32_t> compress_ints = h5_attr_reader::read_class_ints(node, "h5::compress");
			std::string compress_algo;
			int compress_level = -1;
			if (!compress_ints.empty()) {
				compress_level = static_cast<int>(compress_ints[0]);
				compress_algo = compress_strs.empty() ? "gzip" : compress_strs[0];
			}

			if (Result.Context) {
				std::string hdr = get_header_name(node, Result.Context->getSourceManager());
				if (!hdr.empty())
					producer.add_include(hdr);
			}
			producer.scatter_type(rn, fields, chunk_size, compress_algo, compress_level, doc, alias, version, on_missing);
		}
	}

private:
	void topological_order(const clang::CXXRecordDecl* node){
		for(clang::FieldDecl* fld: node->fields() )
			topological_order( utils::as<const clang::QualType>( fld ) );
		auto it = unique.insert( node );
		if( it.second )
			store.push_back( {utils::type::record, node} );
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
			   		store.push_back({utils::type::array, ar});
				break;

			case utils::type::record:
				topological_order( utils::as<const clang::CXXRecordDecl*>( qt ) );
				break;
			default:
				;
		}
	}

	static std::string get_header_name(const clang::CXXRecordDecl* decl, const clang::SourceManager& sm){
		if( !decl ) return "";
		clang::SourceLocation loc = decl->getLocation();
		if( !loc.isValid() ) return "";
		llvm::StringRef path = sm.getFilename(loc);
		if( path.size() < 3 ) return "";
		if( !(path.ends_with(".h") || path.ends_with(".hpp") || path.ends_with(".hxx") || path.ends_with(".hh")) )
			return "";
		size_t pos = path.rfind('/');
		if( pos == llvm::StringRef::npos )
			return path.str();
		return path.substr(pos + 1).str();
	}

	std::ofstream io;
	Producer producer;
	std::set<const void*> unique, nodes;
	std::deque<std::pair<utils::type, const void*>> store;
};

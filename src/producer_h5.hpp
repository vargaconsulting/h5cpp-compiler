/* Copyright (c) 2018-2026 Steven Varga, steven@vargalabs.com Toronto, ON Canada */

#pragma once

#include "producer.hpp"

#include <cstdint>
#include <cstddef>
#include <map>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

struct H5Producer : Producer<H5Producer> {

	void file_begin_impl(){
		io << "#pragma once" << std::endl << std::endl;
		io << "#include <hdf5.h>" << std::endl << std::endl;
	}

	void file_end_impl(){
		io << "\n";
	}

	void template_decl_impl(const std::string& record){
		record_name = record;
		io <<
		"namespace h5 {\n"
		<<indent<< "template<> hid_t inline register_struct<" << record << ">(){\n";

		cpp2hid = std::map<const std::string, const std::string>{
			{"_Bool",     "H5T_NATIVE_HBOOL"},
	  		{"char",      "H5T_NATIVE_CHAR"  },{"unsigned char",      "H5T_NATIVE_UCHAR" },
	  		{"short",     "H5T_NATIVE_SHORT" },{"unsigned short",     "H5T_NATIVE_USHORT" },
	  		{"int",       "H5T_NATIVE_INT"   },{"unsigned int",       "H5T_NATIVE_UINT" },
	  		{"long",      "H5T_NATIVE_LONG"  },{"unsigned long",      "H5T_NATIVE_ULONG" },
	  		{"long long", "H5T_NATIVE_LLONG" },{"unsigned long long", "H5T_NATIVE_ULLONG" },
	  		{"float",     "H5T_NATIVE_FLOAT" },
			{"double",    "H5T_NATIVE_DOUBLE"},{"long double",        "H5T_NATIVE_LDOUBLE" },
		};
		type_cache.clear();
	}

	void record_decl_impl(const std::string& var, const std::string& record_name){
		cpp2hid.insert( std::make_pair(var, var) );
		io <<"\n"<< indent << "hid_t " << var << " = H5Tcreate(H5T_COMPOUND, sizeof (" << record_name << "));\n";
	}

	void return_type_impl(const std::string& var){
		io << "\n";
		io << indent <<"    return " << var  << ";\n"
		   << indent <<"};\n"
		"}\n";
		io << "H5CPP_REGISTER_STRUCT("<< record_name <<");\n\n";
	}

	void array_decl_impl(const std::string& var, const std::string& type, uint64_t size){
		// note the postfix 'size' variable: ar01_ = [23];
		io	<<indent<<"hsize_t " << var << "_[] ={" <<  size  << "};    "
			<<indent<<"hid_t "   << var << " = H5Tarray_create(" << type << ",1," << var << "_" << ");\n";
	}

	void type_insert_impl(const std::string& var, const std::string& field_name,
	                      const std::string& record_name, const std::string& type){
		io <<indent<<"H5Tinsert(" << var << ", \"" << field_name << "\",\tHOFFSET(" << record_name << "," << field_name << ")," << type <<");\n";
	}

	void type_release_impl(){
		if( type_cache.size() <= 1 ) return;

		io <<"\n" <<indent<<"//closing all hid_t allocations to prevent resource leakage\n"
		<< indent;
		for( std::size_t i=0; i<type_cache.size()-1; i++ ){
			io << "H5Tclose(" << type_cache[i] <<");";
			io << ( (i+1)%5 ? " " : "\n"+indent);
		}
		io << "\n";
	}

	bool cache_add_impl(const std::string& key, const std::string& type){
		type_cache.push_back(type);
		cpp2hid.insert( std::make_pair(key, type) );
		return true;
	}

	std::string cache_impl(const std::string& type){
		return cpp2hid[type];
	}

	private:
	std::map<const std::string, const std::string> cpp2hid;
	std::vector<std::string> type_cache;
	std::string hid_t_record, record_name;
	int count;
};

/* Copyright (c) 2018-2026 Steven Varga, steven@vargalabs.com Toronto, ON Canada */
#pragma once

#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stack>
#include <string>

template <typename Derived>
struct Producer {

	void llvm_license(){
	}

	void h5cpp_license(){
	}

	void copyrights(){
	}

	void file_begin(){
		static_cast<Derived*>(this)->file_begin_impl();
	}

	void file_end(){
		static_cast<Derived*>(this)->file_end_impl();
	}

	bool cache_add(const std::string& key, const std::string& type){
		return static_cast<Derived*>(this)->cache_add_impl(key, type);
	}

	std::string cache(const std::string& type){
		return static_cast<Derived*>(this)->cache_impl(type);
	}

	void template_decl(const std::string& name, const std::string& doc = "",
	                   const std::string& alias = "", const std::string& version = ""){
		rec_c = arr_c = 0; indent = "    ";
		static_cast<Derived*>(this)->template_decl_impl(name, doc, alias, version);
		indent_push();
	}

	std::string record_decl(const std::string& name){
		std::stringstream sa;
		sa << "ct_" << std::setw(2) << std::setfill('0') << rec_c++;
		static_cast<Derived*>(this)->record_decl_impl(sa.str(), name);
		return sa.str();
	}

	void return_type(const std::string& type){
		indent_pop();
		static_cast<Derived*>(this)->return_type_impl(type);
	}

	std::string array_decl(const std::string& type, uint64_t size){
		std::stringstream sa;
		sa << "at_" << std::setw(2) << std::setfill('0') << arr_c++;
		static_cast<Derived*>(this)->array_decl_impl(sa.str(), type, size);
		return sa.str();
	}

	void type_insert(const std::string& var, const std::string& field_name,
	                 const std::string& record_name, const std::string& type,
	                 const std::string& on_disk_name = ""){
		static_cast<Derived*>(this)->type_insert_impl(var, field_name, record_name, type, on_disk_name);
	}

	void type_release(){
		static_cast<Derived*>(this)->type_release_impl();
	}

	void add_include(const std::string& path){
		static_cast<Derived*>(this)->add_include_impl(path);
	}

	// Issue #32: scatter/gather emission for tier-2 types.
	struct scatter_field_t {
		std::string cpp_name;    // C++ field name
		std::string h5_name;     // on-disk name
		std::string cpp_type;    // C++ type name (for row_t)
		bool is_vlen;            // true for vector/string
		bool is_string;          // true only for string
		std::string h5_type;     // H5T_NATIVE_* or base type for vlen
	};

	void scatter_type(const std::string& record_name,
	                  const std::vector<scatter_field_t>& fields,
	                  const std::string& chunk_size = "64",
	                  const std::string& compress_algo = "",
	                  int compress_level = -1,
	                  const std::string& doc = "",
	                  const std::string& alias = "",
	                  const std::string& version = "",
	                  const std::string& on_missing = ""){
		static_cast<Derived*>(this)->scatter_type_impl(record_name, fields, chunk_size, compress_algo, compress_level, doc, alias, version, on_missing);
	}

	friend std::ostream& operator<<(std::ostream& os, const Producer<Derived>& producer){
		os << producer.io.str();
		return os;
	}

	void indent_push(){
		stack.push(indent); indent += "    ";
	}

	void indent_pop(){
		indent = stack.top();
		stack.pop();
	}

	private:
	Producer(){};
	friend Derived;
	std::stack<std::string> stack;
	std::string indent;
	std::stringstream io;
	uint64_t rec_c, arr_c;
};

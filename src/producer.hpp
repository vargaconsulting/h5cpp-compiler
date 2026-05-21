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

	void template_decl(const std::string& name){
		rec_c = arr_c = 0; indent = "    ";
		static_cast<Derived*>(this)->template_decl_impl(name);
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
	                 const std::string& record_name, const std::string& type){
		static_cast<Derived*>(this)->type_insert_impl(var, field_name, record_name, type);
	}

	void type_release(){
		static_cast<Derived*>(this)->type_release_impl();
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

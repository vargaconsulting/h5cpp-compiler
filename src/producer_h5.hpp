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
#include <set>

struct H5Producer : Producer<H5Producer> {
	H5Producer(){
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
	}

	std::set<std::string> includes;
	std::set<std::string> emitted_includes;
	bool preamble_done = false;

	void file_begin_impl(){
	}

	void add_include_impl(const std::string& path){
		includes.insert(path);
	}

	void ensure_preamble(){
		if (!preamble_done) {
			io << "#pragma once" << std::endl << std::endl;
			io << "#include <h5cpp/all>" << std::endl;
			preamble_done = true;
		}
		bool any_new = false;
		for (const auto& inc : includes) {
			if (emitted_includes.insert(inc).second) {
				io << "#include \"" << inc << "\"" << std::endl;
				any_new = true;
			}
		}
		if (any_new)
			io << std::endl;
	}

	void file_end_impl(){
		io << "\n";
	}

	void template_decl_impl(const std::string& record, const std::string& doc,
	                        const std::string& alias, const std::string& version){
		ensure_preamble();
		record_name = record;
		if( !doc.empty() )    io << "// doc: \""    << doc    << "\"\n";
		if( !alias.empty() )  io << "// alias: \""  << alias  << "\"\n";
		if( !version.empty() ) io << "// version: \"" << version << "\"\n";
		io <<
		"namespace h5 {\n"
		<<indent<< "template<> hid_t inline register_struct<" << record << ">(){\n";

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
	                      const std::string& record_name, const std::string& type,
	                      const std::string& on_disk_name = ""){
		const std::string& name_to_use = on_disk_name.empty() ? field_name : on_disk_name;
		io <<indent<<"H5Tinsert(" << var << ", \"" << name_to_use << "\",\tHOFFSET(" << record_name << "," << field_name << ")," << type <<");\n";
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

		// Issue #32: emit visitor-only scatter/gather specializations for tier-2 types.
		void scatter_type_impl(const std::string& record_name,
		                       const std::vector<scatter_field_t>& fields,
		                       const std::string& /*chunk_size*/,
		                       const std::string& /*compress_algo*/,
		                       int /*compress_level*/,
		                       const std::string& doc,
		                       const std::string& alias,
		                       const std::string& version,
		                       const std::string& /*on_missing*/){
			ensure_preamble();
			// Generate a valid namespace identifier from the record name or alias.
			std::string ns_name = (alias.empty() ? record_name : alias) + "_";
			std::replace(ns_name.begin(), ns_name.end(), ':', '_');

			// --- generated namespace: row_t + compound_type() ---
			if( !doc.empty() )    io << "// doc: ""    << doc    << ""\n";
			if( !alias.empty() )  io << "// alias: ""  << alias  << ""\n";
			if( !version.empty() ) io << "// version: "" << version << ""\n";
			io << "namespace h5::generated::" << ns_name << " {\n\n";

			// row_t struct
			io << "struct row_t {\n";
			for (const auto& f : fields) {
				if (f.is_vlen) {
					if (f.is_string) {
						io << "    char*    " << f.cpp_name << ";\n";
					} else {
						io << "    hvl_t    " << f.cpp_name << ";\n";
					}
				} else {
					io << "    " << f.cpp_type << " " << f.cpp_name << ";\n";
				}
			}
			io << "};\n\n";

			// compound_type() factory
			io << "inline hid_t compound_type() {\n"
			   << "    static const hid_t ct = []{\n";

			// Emit VLEN / string type helpers for vlen fields
			for (const auto& f : fields) {
				if (!f.is_vlen) continue;
				std::string var = "v_" + f.cpp_name;
				if (f.is_string) {
					io << "        hid_t " << var << " = H5Tcopy(H5T_C_S1);\n"
					   << "        H5Tset_size(" << var << ", H5T_VARIABLE);\n";
				} else {
					io << "        hid_t " << var << " = H5Tvlen_create(" << f.h5_type << ");\n";
				}
			}

			io << "        hid_t ct = H5Tcreate(H5T_COMPOUND, sizeof(row_t));\n";
			for (const auto& f : fields) {
				std::string type_expr;
				if (f.is_vlen) {
					type_expr = "v_" + f.cpp_name;
				} else {
					type_expr = f.h5_type;
				}
				io << "        H5Tinsert(ct, \"" << f.h5_name << "\", HOFFSET(row_t, "
				   << f.cpp_name << "), " << type_expr << ");\n";
			}
			io << "        return ct;\n"
			   << "    }();\n"
			   << "    return ct;\n"
			   << "}\n\n"
			   << "} // namespace h5::generated::" << ns_name << "\n\n";

			// --- scatter_traits<T> specialization (must come before scatter/gather) ---
			io << "namespace h5 {\n"
			   << "template<> struct scatter_traits<" << record_name << "> {\n"
			   << "    using row_type = ::h5::generated::" << ns_name << "::row_t;\n"
			   << "    static hid_t compound_type() { return ::h5::generated::" << ns_name << "::compound_type(); }\n"
			   << "};\n"
			   << "} // namespace h5\n\n";

			// --- scatter<T> specialization (visitor: pure conversion) ---
			io << "namespace h5 {\n"
			   << "template<> inline std::pair<::h5::generated::" << ns_name << "::row_t*, std::size_t>\n"
			   << "scatter<" << record_name << ">(\n"
			   << "    const " << record_name << "* src, std::size_t n) {\n"
			   << "    using namespace ::h5::generated::" << ns_name << ";\n"
			   << "    auto* rows = new row_t[n]();\n";

			for (const auto& f : fields) {
				io << "    for (std::size_t i = 0; i < n; ++i) {\n";
				if (f.is_vlen) {
					if (f.is_string) {
						io << "        rows[i]." << f.cpp_name << " = const_cast<char*>(src[i]."
						   << f.cpp_name << ".c_str());\n";
					} else {
						io << "        rows[i]." << f.cpp_name << " = hvl_t{src[i]."
						   << f.cpp_name << ".size(), (void*)src[i]." << f.cpp_name << ".data()};\n";
					}
				} else {
					io << "        rows[i]." << f.cpp_name << " = src[i]." << f.cpp_name << ";\n";
				}
				io << "    }\n";
			}
			io << "    return {rows, n};\n"
			   << "}\n"
			   << "} // namespace h5\n\n";

			// --- gather<T> specialization (visitor: pure conversion + reclamation) ---
			io << "namespace h5 {\n"
			   << "template<> inline std::pair<" << record_name << "*, std::size_t>\n"
			   << "gather<" << record_name << ">(\n"
			   << "    ::h5::generated::" << ns_name << "::row_t* rows, std::size_t n, hid_t reclaim_space) {\n"
			   << "    using namespace ::h5::generated::" << ns_name << ";\n"
			   << "    auto* dst = new " << record_name << "[n];\n";

			for (const auto& f : fields) {
				io << "    for (std::size_t i = 0; i < n; ++i) {\n";
				if (f.is_vlen) {
					if (f.is_string) {
						io << "        if (rows[i]." << f.cpp_name << ") dst[i]."
						   << f.cpp_name << ".assign(rows[i]." << f.cpp_name << ");\n";
					} else {
						io << "        dst[i]." << f.cpp_name << ".assign(static_cast<" << f.cpp_type
						   << "*>(rows[i]." << f.cpp_name << ".p), static_cast<" << f.cpp_type
						   << "*>(rows[i]." << f.cpp_name << ".p) + rows[i]." << f.cpp_name << ".len);\n";
					}
				} else {
					io << "        dst[i]." << f.cpp_name << " = rows[i]." << f.cpp_name << ";\n";
				}
				io << "    }\n";
			}
			io << "#if H5_VERSION_GE(1,12,0)\n"
			   << "    H5Treclaim(compound_type(), reclaim_space, H5P_DEFAULT, rows);\n"
			   << "#else\n"
			   << "    H5Dvlen_reclaim(compound_type(), reclaim_space, H5P_DEFAULT, rows);\n"
			   << "#endif\n"
			   << "    delete[] rows;\n"
			   << "    return {dst, n};\n"
			   << "}\n"
			   << "} // namespace h5\n\n";

			// --- registration macro ---
			io << "H5CPP_REGISTER_SCATTER(" << record_name << ");\n\n";
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

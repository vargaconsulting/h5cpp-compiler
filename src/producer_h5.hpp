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

	// Issue #32: emit scatter/gather specializations for tier-2 types.
	void scatter_type_impl(const std::string& record_name,
	                       const std::vector<scatter_field_t>& fields,
	                       const std::string& chunk_size,
	                       const std::string& compress_algo,
	                       int compress_level,
	                       const std::string& doc,
	                       const std::string& alias,
	                       const std::string& version,
	                       const std::string& on_missing){
		ensure_preamble();
		// Generate a valid namespace identifier from the record name or alias.
		std::string ns_name = (alias.empty() ? record_name : alias) + "_";
		std::replace(ns_name.begin(), ns_name.end(), ':', '_');

		// --- generated namespace: row_t + compound_type() ---
		// Contents indented one level (4 spaces) inside the namespace block.
		if( !doc.empty() )    io << "// doc: \""    << doc    << "\"\n";
		if( !alias.empty() )  io << "// alias: \""  << alias  << "\"\n";
		if( !version.empty() ) io << "// version: \"" << version << "\"\n";
		io << "namespace h5::generated::" << ns_name << " {\n";

		// row_t struct (4-space indent for struct, 8-space for members)
		io << "    struct row_t {\n";
		for (const auto& f : fields) {
			if (f.is_vlen) {
				if (f.is_string) {
					io << "        char*    " << f.cpp_name << ";\n";
				} else {
					io << "        hvl_t    " << f.cpp_name << ";\n";
				}
			} else {
				io << "        " << f.cpp_type << " " << f.cpp_name << ";\n";
			}
		}
		io << "    };\n";

		// compound_type() factory (4-space for function, 8-space inside, 12-space inside lambda)
		io << "    inline hid_t compound_type() {\n"
		   << "        static const hid_t ct = []{\n";

		// Emit VLEN / string type helpers for vlen fields
		for (const auto& f : fields) {
			if (!f.is_vlen) continue;
			std::string var = "v_" + f.cpp_name;
			if (f.is_string) {
				io << "            hid_t " << var << " = H5Tcopy(H5T_C_S1);\n"
				   << "            H5Tset_size(" << var << ", H5T_VARIABLE);\n";
			} else {
				io << "            hid_t " << var << " = H5Tvlen_create(" << f.h5_type << ");\n";
			}
		}

		io << "            hid_t ct = H5Tcreate(H5T_COMPOUND, sizeof(row_t));\n";
		for (const auto& f : fields) {
			std::string type_expr;
			if (f.is_vlen) {
				type_expr = "v_" + f.cpp_name;
			} else {
				type_expr = f.h5_type;
			}
			io << "            H5Tinsert(ct, \"" << f.h5_name << "\", HOFFSET(row_t, "
			   << f.cpp_name << "), " << type_expr << ");\n";
		}
		io << "            return ct;\n"
		   << "        }();\n"
		   << "        return ct;\n"
		   << "    }\n"
		   << "} // namespace h5::generated::" << ns_name << "\n\n";

		// --- scatter<T> specialization ---
		// Template + body indented one level (4 spaces) inside `namespace h5 {`.
		io << "namespace h5 {\n"
		   << "    template<> inline h5::ds_t scatter<" << record_name << ">(\n"
		   << "        hid_t fd, const std::string& path, const " << record_name << "& obj) {\n"
		   << "        using namespace ::h5::generated::" << ns_name << ";\n"
		   << "        h5::ds_t ds;\n"
		   << "        h5::mute();\n"
		   << "        bool exists = H5Lexists(fd, path.c_str(), H5P_DEFAULT) > 0;\n"
		   << "        h5::unmute();\n";
		if( on_missing == "error" ){
			io << "        if (!exists) throw std::runtime_error(\"dataset not found: \" + path);\n"
			   << "        ds = h5::open(fd, path, h5::default_dapl);\n";
		} else if( on_missing == "ignore" ){
			io << "        if (!exists) return ds;\n"
			   << "        ds = h5::open(fd, path, h5::default_dapl);\n";
		} else {
			// Flipped: larger branch (create) on top; smaller (open) becomes a
			// single-statement trailing else with no braces.
			io << "        if (!exists) {\n"
			   << "            h5::dcpl_t dcpl{H5Pcreate(H5P_DATASET_CREATE)};\n"
			   << "            hsize_t chunk = " << chunk_size << ";\n"
			   << "            H5Pset_chunk(dcpl, 1, &chunk);\n";
			if (!compress_algo.empty()) {
				if (compress_algo == "gzip") {
					io << "            H5Pset_deflate(dcpl, " << compress_level << ");\n";
				}
			}
			io
			   << "            hsize_t cur = 0;\n"
			   << "            hsize_t max = H5S_UNLIMITED;\n"
			   << "            hid_t space = H5Screate_simple(1, &cur, &max);\n"
			   << "            ds = h5::createds(fd, path, compound_type(), h5::sp_t{space},\n"
			   << "                h5::default_lcpl, dcpl, h5::default_dapl);\n"
			   << "        } else ds = h5::open(fd, path, h5::default_dapl);\n";
		}
		io << "        hsize_t row = h5::detail::next_row(ds);\n";

		// Pre-compute vlen helper variables (size + data pointer)
		for (const auto& f : fields) {
			if (!f.is_vlen) continue;
			io << "        hsize_t " << f.cpp_name << "_len = obj." << f.cpp_name << ".size();\n";
			if (f.is_string) {
				io << "        const char* " << f.cpp_name << "_ptr = obj." << f.cpp_name << ".c_str();\n";
			} else {
				io << "        auto* " << f.cpp_name << "_ptr = obj." << f.cpp_name << ".data();\n";
			}
		}

		io << "        row_t r{\n";
		for (std::size_t i = 0; i < fields.size(); ++i) {
			const auto& f = fields[i];
			io << "            ";
			if (f.is_vlen) {
				if (f.is_string) {
					io << "(char*)" << f.cpp_name << "_ptr";
				} else {
					io << "hvl_t{" << f.cpp_name << "_len, (void*)" << f.cpp_name << "_ptr}";
				}
			} else {
				io << "obj." << f.cpp_name;
			}
			io << (i + 1 < fields.size() ? ",\n" : "\n");
		}
		io << "        };\n";

		io << "        herr_t err = h5::detail::write_one_row(ds, compound_type(), row, &r);\n"
		   << "        (void)err;\n"
		   << "        return ds;\n"
		   << "    }\n"
		   << "} // namespace h5\n\n";

		// --- gather<T> specialization ---
		// Same indentation scheme as scatter above.
		io << "namespace h5 {\n"
		   << "    template<> inline void gather<" << record_name << ">(\n"
		   << "        hid_t fd, const std::string& path, " << record_name << "& obj) {\n"
		   << "        using namespace ::h5::generated::" << ns_name << ";\n";
		if( on_missing == "error" || on_missing == "ignore" ){
			io << "        h5::mute();\n"
			   << "        bool exists = H5Lexists(fd, path.c_str(), H5P_DEFAULT) > 0;\n"
			   << "        h5::unmute();\n";
			if( on_missing == "error" ){
				io << "        if (!exists) throw std::runtime_error(\"dataset not found: \" + path);\n";
			} else {
				io << "        if (!exists) return;\n";
			}
		}
		io << "        h5::ds_t ds = h5::open(fd, path, h5::default_dapl);\n"
		   << "        hsize_t nrows = h5::detail::next_row(ds);\n"
		   << "        if (nrows == 0) return;\n"
		   << "        row_t r{};\n"
		   << "        herr_t err = h5::detail::read_one_row(ds, compound_type(), nrows - 1, &r);\n"
		   << "        (void)err;\n";

		for (const auto& f : fields) {
			if (f.is_vlen) {
				if (f.is_string) {
					io << "        if (r." << f.cpp_name << ") obj." << f.cpp_name << ".assign(r."
					   << f.cpp_name << ");\n";
				} else {
					io << "        obj." << f.cpp_name << ".assign(static_cast<" << f.cpp_type
					   << "*>(r." << f.cpp_name << ".p), static_cast<" << f.cpp_type
					   << "*>(r." << f.cpp_name << ".p) + r." << f.cpp_name << ".len);\n";
				}
			} else {
				io << "        obj." << f.cpp_name << " = r." << f.cpp_name << ";\n";
			}
		}
		// Use the HDF5 version-aware reclaim API:
		//   HDF5 >= 1.12: H5Treclaim (canonical name; the old H5Dvlen_reclaim is
		//                 deprecated and may be hidden when the library is built
		//                 with H5_NO_DEPRECATED_SYMBOLS).
		//   HDF5  < 1.12: H5Dvlen_reclaim (the only available spelling).
		io << "        hid_t reclaim_space = H5Screate(H5S_SCALAR);\n"
		   << "        #if H5_VERSION_GE(1,12,0)\n"
		   << "            H5Treclaim(compound_type(), reclaim_space, H5P_DEFAULT, &r);\n"
		   << "        #else\n"
		   << "            H5Dvlen_reclaim(compound_type(), reclaim_space, H5P_DEFAULT, &r);\n"
		   << "        #endif\n"
		   << "        H5Sclose(reclaim_space);\n"
		   << "    }\n"
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

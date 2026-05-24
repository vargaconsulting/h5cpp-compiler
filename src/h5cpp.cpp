/* Copyright (c) 2018-2026 Steven Varga, steven@vargalabs.com Toronto, ON Canada */

// Declares clang::SyntaxOnlyAction.
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
// Declares llvm::cl::extrahelp.
#include <llvm/Support/CommandLine.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclTemplate.h>

#include <cstdio>
#include <iostream>
#include <fstream>

#include "producer_h5.hpp"
#include "consumer.hpp"
#include "h5_attr_translator.hpp"

clang::ast_matchers::StatementMatcher h5templateMatcher = clang::ast_matchers::callExpr( clang::ast_matchers::allOf(
	clang::ast_matchers::hasDescendant( clang::ast_matchers::declRefExpr( clang::ast_matchers::to( clang::ast_matchers::varDecl().bind("variableDecl")  ) ) ),
	clang::ast_matchers::hasDescendant( clang::ast_matchers::declRefExpr( clang::ast_matchers::to(
		clang::ast_matchers::functionDecl( clang::ast_matchers::allOf(
			clang::ast_matchers::eachOf(
				clang::ast_matchers::hasName("h5::write"),  clang::ast_matchers::hasName("h5::create"),  clang::ast_matchers::hasName("h5::read"), clang::ast_matchers::hasName("h5::append"),
				clang::ast_matchers::hasName("h5::awrite"), clang::ast_matchers::hasName("h5::acreate"), clang::ast_matchers::hasName("h5::aread")
			),
			clang::ast_matchers::hasTemplateArgument(0,  clang::ast_matchers::refersToType( clang::ast_matchers::qualType( clang::ast_matchers::eachOf(
				clang::ast_matchers::hasDeclaration( clang::ast_matchers::cxxRecordDecl(clang::ast_matchers::isStruct()).bind("cxxRecordDecl")),
				clang::ast_matchers::hasDeclaration( clang::ast_matchers::classTemplateSpecializationDecl(
					clang::ast_matchers::hasTemplateArgument(0,  clang::ast_matchers::refersToType( clang::ast_matchers::qualType( clang::ast_matchers::eachOf(
					clang::ast_matchers::hasDeclaration( clang::ast_matchers::cxxRecordDecl(clang::ast_matchers::isStruct()).bind("cxxRecordDecl")),
					clang::ast_matchers::hasDeclaration( clang::ast_matchers::classTemplateSpecializationDecl(
						clang::ast_matchers::hasTemplateArgument(0,  clang::ast_matchers::refersToType( clang::ast_matchers::qualType(
						clang::ast_matchers::hasDeclaration( clang::ast_matchers::cxxRecordDecl(clang::ast_matchers::isStruct()).bind("cxxRecordDecl"))))))
					)
					)) ))  ) ),
				clang::ast_matchers::hasDeclaration( clang::ast_matchers::cxxRecordDecl( clang::ast_matchers::isClass()  ).bind("classDecl")) )
			) )),
			clang::ast_matchers::isTemplateInstantiation()
	))  )))
));

enum class OutputFormat { hdf5, protobuf, json, msgpack, cbor, bson, avro, rlp };

static llvm::cl::OptionCategory MyToolCategory("h5cpp options");
static llvm::cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);

static llvm::cl::opt<std::string> OutputFile("o",
    llvm::cl::desc("Output file"),
    llvm::cl::value_desc("file"),
    llvm::cl::Required,
    llvm::cl::cat(MyToolCategory));

static llvm::cl::alias OutputFileLong("output",
    llvm::cl::desc("Alias for -o"),
    llvm::cl::aliasopt(OutputFile));

static llvm::cl::opt<OutputFormat> Format(llvm::cl::desc("Output format:"),
    llvm::cl::values(
        clEnumValN(OutputFormat::hdf5,     "hdf5",     "HDF5 compound type registrations (default)"),
        clEnumValN(OutputFormat::protobuf, "protobuf", "Protocol Buffers descriptor"),
        clEnumValN(OutputFormat::json,     "json",     "JSON Schema descriptor"),
        clEnumValN(OutputFormat::msgpack,  "msgpack",  "MessagePack descriptor"),
        clEnumValN(OutputFormat::cbor,     "cbor",     "CBOR descriptor"),
        clEnumValN(OutputFormat::bson,     "bson",     "BSON descriptor"),
        clEnumValN(OutputFormat::avro,     "avro",     "Avro descriptor"),
        clEnumValN(OutputFormat::rlp,      "rlp",      "RLP descriptor")
    ),
    llvm::cl::init(OutputFormat::hdf5),
    llvm::cl::cat(MyToolCategory));

static llvm::cl::opt<bool> CheckMode("check",
    llvm::cl::desc("Verify that the existing generated file is up to date (exit 1 if stale)"),
    llvm::cl::cat(MyToolCategory),
    llvm::cl::init(false));

int main(int argc, const char **argv) {
	std::cerr <<
		"H5CPP: Copyright (c) 2018-2026, VargaLABS, Toronto, ON Canada\n"
   	 	"LLVM : Copyright (c) 2003-2010, University of Illinois at Urbana-Champaign.\n"
	;
	auto ExpectedParser = clang::tooling::CommonOptionsParser::create(argc, argv, MyToolCategory);
	if (!ExpectedParser) {
		llvm::errs() << ExpectedParser.takeError();
		return 1;
	}
	clang::tooling::CommonOptionsParser &OptionsParser = ExpectedParser.get();
	clang::tooling::ClangTool Tool(OptionsParser.getCompilations(),
				 OptionsParser.getSourcePathList());

	// Issue #32: rewrite [[h5::xxx(...)]] → [[clang::annotate("h5::xxx", ...)]]
	// for each source path before Clang sees it.
	std::vector<std::string> _h5_attr_storage;
	h5_attr_translator::install_virtual_files(
		Tool, OptionsParser.getSourcePathList(), _h5_attr_storage);

	std::string work_path = OutputFile;
	if (CheckMode) {
		work_path = OutputFile + ".h5cpp-check";
	}

	int rc = 0;
	{
		clang::ast_matchers::MatchFinder Finder;
		switch (Format) {
			case OutputFormat::hdf5: {
				H5TemplateCallback<H5Producer> callback(work_path);
				Finder.addMatcher(h5templateMatcher, &callback);
				rc = Tool.run(clang::tooling::newFrontendActionFactory(&Finder).get());
				break;
			}
			case OutputFormat::protobuf:
				llvm::errs() << "h5cpp-compiler: --format protobuf not yet implemented\n";
				rc = 1;
				break;
			case OutputFormat::json:
				llvm::errs() << "h5cpp-compiler: --format json not yet implemented\n";
				rc = 1;
				break;
			case OutputFormat::msgpack:
				llvm::errs() << "h5cpp-compiler: --format msgpack not yet implemented\n";
				rc = 1;
				break;
			case OutputFormat::cbor:
				llvm::errs() << "h5cpp-compiler: --format cbor not yet implemented\n";
				rc = 1;
				break;
			case OutputFormat::bson:
				llvm::errs() << "h5cpp-compiler: --format bson not yet implemented\n";
				rc = 1;
				break;
			case OutputFormat::avro:
				llvm::errs() << "h5cpp-compiler: --format avro not yet implemented\n";
				rc = 1;
				break;
			case OutputFormat::rlp:
				llvm::errs() << "h5cpp-compiler: --format rlp not yet implemented\n";
				rc = 1;
				break;
		}
	}

	if (CheckMode && rc == 0) {
		std::ifstream generated(work_path);
		std::ifstream existing(OutputFile);
		bool same = false;
		if (generated && existing) {
			std::string g((std::istreambuf_iterator<char>(generated)),
			              std::istreambuf_iterator<char>());
			std::string e((std::istreambuf_iterator<char>(existing)),
			              std::istreambuf_iterator<char>());
			same = (g == e);
		}
		std::remove(work_path.c_str());
		if (!same) {
			llvm::errs() << "h5cpp-compiler --check: generated file is out of date: "
			             << OutputFile << "\n";
			return 1;
		}
	}

	return rc;
}

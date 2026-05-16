/*
 * Copyright (c) 2018-2020 Steven Varga, Toronto,ON Canada
 * Author: Varga, Steven <steven@vargaconsulting.ca>
 */

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
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

#include <cstdio>
#include <set>
#include <stack>
#include <algorithm>
#include <iostream>
#include <fstream>

using namespace clang::tooling;
using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

#include "producer.hpp"
#include "producer_h5.hpp"
#include "consumer.hpp"

/// variables
StatementMatcher h5templateMatcher = callExpr( allOf(
	hasDescendant( declRefExpr( to( varDecl().bind("variableDecl")  ) ) ),
	hasDescendant( declRefExpr( to(
		functionDecl( allOf(
			eachOf(
				hasName("h5::write"),  hasName("h5::create"),  hasName("h5::read"), hasName("h5::append"), // dataset
				hasName("h5::awrite"), hasName("h5::acreate"), hasName("h5::aread") // attributes
			),
			/* locate T template argument, and declarations of T
			 * h5::write<T>( ... ) 
			 */
			hasTemplateArgument(0,  refersToType( qualType( eachOf( // all inners must match qualType
				/* if 'T' := some_struct */
				hasDeclaration( cxxRecordDecl(isStruct()).bind("cxxRecordDecl")),
				/* if 'T' := std::container<some_struct>*/
				hasDeclaration( classTemplateSpecializationDecl(
					hasTemplateArgument(0,  refersToType( qualType( eachOf(
					// T:=std::container<some_struct>
					hasDeclaration( cxxRecordDecl(isStruct()).bind("cxxRecordDecl")),
					// T:=std::vector<std::vector<some_struct>>   rugged array of structs
					hasDeclaration( classTemplateSpecializationDecl(
						hasTemplateArgument(0,  refersToType( qualType( 
						hasDeclaration( cxxRecordDecl(isStruct()).bind("cxxRecordDecl"))))))
					)
					))/*.bind("TemplateArg")*/ ))/*End templSpecDecl */ ) ),
				hasDeclaration( cxxRecordDecl( isClass()  ).bind("classDecl")) )
			) /*.bind("TemplateArg")*/ )),
			isTemplateInstantiation()
	)) /*END functionDecl*/ )))
));

static llvm::cl::OptionCategory MyToolCategory("h5cpp options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

static cl::opt<bool> CheckMode("check",
    cl::desc("Verify that the existing generated file is up to date (exit 1 if stale)"),
    cl::cat(MyToolCategory),
    cl::init(false));

/* the actual program */
int main(int argc, const char **argv) {
	//FIXME: last argument is the generated file, this could be improved upon
	std::string arg( strdup( argv[argc-1]) );
	std::string path = arg.substr(2);
	argc --;

	std::cerr <<
		"H5CPP: Copyright (c) 2018-2020, VargaConsulting, Toronto,ON Canada\n"
   	 	"LLVM : Copyright (c) 2003-2010, University of Illinois at Urbana-Champaign.\n"
	;
	auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
	if (!ExpectedParser) {
		llvm::errs() << ExpectedParser.takeError();
		return 1;
	}
	CommonOptionsParser &OptionsParser = ExpectedParser.get();
	ClangTool Tool(OptionsParser.getCompilations(),
				 OptionsParser.getSourcePathList());

	std::string work_path = path;
	if (CheckMode) {
		work_path = path + ".h5cpp-check";
	}

	int rc = 0;
	{
		H5TemplateCallback<H5Producer> callback( work_path );
		MatchFinder Finder;
		Finder.addMatcher(h5templateMatcher, &callback );
		//Tool.setDiagnosticConsumer( new IgnoringDiagConsumer() );
		//TODO: 
		// in first pass h5::operators trip, as template specializations are not yet generated
		// for now the entire diagnostic messages are disabled and error diagnostics left for final
		// compile phase. 
		rc = Tool.run( newFrontendActionFactory (&Finder).get());
	}

	if (CheckMode && rc == 0) {
		std::ifstream generated(work_path);
		std::ifstream existing(path);
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
			             << path << "\n";
			return 1;
		}
	}

	return rc;
}

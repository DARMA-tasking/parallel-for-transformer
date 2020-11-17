
// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/ExprCXX.h"

// This moved in later versions of clang
#include "clang/Tooling/Core/QualTypeNames.h"
//#include "clang/AST/QualTypeNames.h"

#include "fmt/format.h"

#include <memory>
#include <list>
#include <tuple>

#include "qualified_name.h"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;

static FILE* out = nullptr;

static cl::opt<std::string> Filename("o", cl::desc("Filename to output generated code"));

static cl::list<std::string> Includes("I", cl::desc("Include directories"), cl::ZeroOrMore);

static cl::opt<bool> GenerateInline("inline", cl::desc("Generate code inline and modify files"));

StatementMatcher CallMatcher =
  callExpr(
    callee(
      functionDecl(
        matchesName("::Kokkos::parallel_*") // hasName("::Kokkos::parallel_for")
      )
    )
  ).bind("callExpr");

struct ParallelForRewriter : MatchFinder::MatchCallback {
  explicit ParallelForRewriter(Rewriter& in_rw)
    : rw(in_rw)
  { }

  virtual void run(const MatchFinder::MatchResult &Result) {
    fmt::print("=== Invoking rewriter on matched result ===\n");

    if (CallExpr const *ce = Result.Nodes.getNodeAs<clang::CallExpr>("callExpr")) {
      fmt::print(
        "Traversing function \"{}\" ptr={}\n",
        ce->getDirectCallee()->getNameInfo().getAsString(),
        static_cast<void const*>(ce)
      );
      ce->dumpPretty(ce->getDirectCallee()->getASTContext());
      fmt::print("\n");
      ce->dumpColor();

      auto loc = ce->getLocEnd();
      int offset = Lexer::MeasureTokenLength(loc, rw.getSourceMgr(), rw.getLangOpts()) + 1;

      SourceLocation loc2 = loc.getLocWithOffset(offset);
      rw.InsertText(loc2, "\n/* inserting fence here */", true, true);
    } else {
      fmt::print(stderr, "traversing something unknown?\n");
      exit(1);
    }
  }

  Rewriter& rw;
};

struct MyASTConsumer : ASTConsumer {
  MyASTConsumer(Rewriter& in_rw) : call_handler_(in_rw) {
    matcher_.addMatcher(CallMatcher, &call_handler_);
  }

  void HandleTranslationUnit(ASTContext& Context) override {
    // Run the matchers when we have the whole TU parsed.
    matcher_.matchAST(Context);
  }

private:
  ParallelForRewriter call_handler_;
  MatchFinder matcher_;
};

// For each source file provided to the tool, a new FrontendAction is created.
struct MyFrontendAction : ASTFrontendAction {
  void EndSourceFileAction() override {
    //rw_.getEditBuffer(rw_.getSourceMgr().getMainFileID()).write(llvm::outs());
    //rw_.getSourceMgr().getMainFileID()

    auto& sm = rw_.getSourceMgr();
    for (auto iter = rw_.buffer_begin(); iter != rw_.buffer_end(); ++iter) {
      fmt::print(
        stderr, "Modified file {}\n",
        sm.getFileEntryForID(iter->first)->getName().str()
      );
    }

    rw_.overwriteChangedFiles();
  }

  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    rw_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());

    return llvm::make_unique<MyASTConsumer>(rw_);
  }

private:
  Rewriter rw_;
};

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory SerializeCheckerCategory("Parallel for transformer");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nInserts fences after parallel-for/scan/etc.\n");

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, SerializeCheckerCategory);

  ClangTool Tool(
    OptionsParser.getCompilations(), OptionsParser.getSourcePathList()
  );

  if (Filename == "") {
    out = stdout;
  } else {
    out = fopen(Filename.c_str(), "w");
  }

  for (auto&& e : Includes) {
    auto str = std::string("-I") + e;
    ArgumentsAdjuster ad1 = getInsertArgumentAdjuster(str.c_str());
    Tool.appendArgumentsAdjuster(ad1);
    fmt::print(stderr, "Including {}\n", e);
  }

  Tool.run(newFrontendActionFactory<MyFrontendAction>().get());

  if (Filename == "" and out != nullptr) {
    fclose(out);
  }
  return 0;
}

/*
//@HEADER
// *****************************************************************************
//
//                                transform.cc
//                          DARMA Toolkit v. 1.0.0
//                    DARMA/pft => parallel-for transformer
//
// Copyright 2019 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact darma@sandia.gov
//
// *****************************************************************************
//@HEADER
*/

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

#include "fmt/format.h"

#include <memory>
#include <list>
#include <tuple>
#include <regex>
#include <set>
#include <fstream>

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;

static FILE* out = nullptr;

static cl::opt<std::string> Filename("o", cl::desc("Filename to output generated code"));

static cl::opt<std::string> Processed("x", cl::desc("Database of processed files"));

static cl::opt<std::string> Matcher("f", cl::desc("Only transform filenames matching this pattern"));

static cl::list<std::string> Includes("I", cl::desc("Include directories"), cl::ZeroOrMore);

static cl::opt<bool> GenerateInline("inline", cl::desc("Generate code inline and modify files"));

static std::set<std::string> new_processed_files;
static std::set<std::string> processed_files;

StatementMatcher CallMatcher =
  callExpr(
    callee(
      functionDecl(
        matchesName("::Kokkos::parallel_*") // hasName("::Kokkos::parallel_for")
      )
    )
  ).bind("callExpr");

StatementMatcher FenceMatcher =
  callExpr(
    callee(
      functionDecl(
        hasName("::Kokkos::fence")
      )
    )
  ).bind("fenceExpr");

struct FenceCallback : MatchFinder::MatchCallback {
  virtual void run(const MatchFinder::MatchResult &Result) {
    if (CallExpr const *ce = Result.Nodes.getNodeAs<clang::CallExpr>("fenceExpr")) {
      found = true;
    }
  }
  bool found = false;
};

template <typename T>
auto getBegin(T const& t) {
#if LLVM_VERSION_MAJOR > 7
  return t->getBeginLoc();
#else
  return t->getBeginLoc();
#endif
}

template <typename T>
auto getEnd(T const& t) {
#if LLVM_VERSION_MAJOR > 7
  return t->getEndLoc();
#else
  return t->getLocEnd();
#endif
}

struct RewriteBlocking {
  explicit RewriteBlocking(Rewriter& in_rw)
    : rw(in_rw)
  { }

  void operator()(CallExpr const* par, CallExpr const* fence) const {
    // Change the namespace
    {
      auto begin = getBegin(par);

      auto str = rw.getRewrittenText(SourceRange(begin, begin.getLocWithOffset(6)));
      fmt::print("str={}\n", str);
      if (str == "Kokkos::") {
        rw.ReplaceText(begin, 6, "empire");
      } else {
        rw.InsertTextBefore(begin, "empire::");
      }
    }

    // Switch to parallel_* blocking
    {
      auto end = getEnd(par->getCallee());
      rw.InsertTextAfterToken(end, "_blocking");
    }

    // Remove the fence line
    {
      auto begin = getBegin(fence);
      auto end = getEnd(fence);
      typename Rewriter::RewriteOptions ro;
      ro.RemoveLineIfEmpty = true;
      rw.RemoveText(SourceRange{begin, end.getLocWithOffset(1)}, ro);
    }
  }

private:
  Rewriter& rw;
};

struct RewriteAsync {
  explicit RewriteAsync(Rewriter& in_rw)
    : rw(in_rw)
  { }

  void operator()(CallExpr const* par) const {
    // Change the namespace
    {
      auto begin = getBegin(par);

      auto str = rw.getRewrittenText(SourceRange(begin, begin.getLocWithOffset(6)));
      fmt::print("str={}\n", str);
      if (str == "Kokkos::") {
        rw.ReplaceText(begin, 6, "empire");
      } else {
        rw.InsertTextBefore(begin, "empire::");
      }
    }

    // Switch to parallel_* async
    {
      auto end = getEnd(par->getCallee());
      rw.InsertTextAfterToken(end, "_async");
    }
  }

private:
  Rewriter& rw;
};

struct ParallelForRewriter : MatchFinder::MatchCallback {
  explicit ParallelForRewriter(Rewriter& in_rw)
    : rw(in_rw)
  { }

  virtual void run(const MatchFinder::MatchResult &Result) {
    if (CallExpr const *ce = Result.Nodes.getNodeAs<clang::CallExpr>("callExpr")) {

      auto& ctx = Result.Context;
      auto p = ctx->getParents(*ce);
      fmt::print("size={}\n", p.size());

      if (p.size() != 1) {
        return;
      }

      bool found_fence = false;
      CallExpr const* fence = nullptr;
      for (std::size_t i = 0; i < p.size(); i++) {
        Stmt const* st = p[i].get<Stmt>();
        //st->dumpColor();
        if (isa<CompoundStmt>(st)) {
          auto const& cs = cast<CompoundStmt>(st);
          for (auto iter = cs->child_begin(); iter != cs->child_end(); ++iter) {
            if (*iter == ce) {
              iter++;
              if (iter != cs->child_end()) {
                MatchFinder fence_matcher;
                auto fc = std::make_unique<FenceCallback>();
                fence_matcher.addMatcher(FenceMatcher, fc.get());
                fence_matcher.match(**iter, *Result.Context);
                found_fence = fc->found;
                if (fc->found) {
                  fence = cast<CallExpr>(*iter);
                  ///fence = *iter;
                  fmt::print("FOUND fence\n");
                } else {
                  fmt::print("NOT FOUND fence\n");
                }

              }
            }
          }
        }

        break;
      }

      if (found_fence) {
        // rewrite to blocking with empire
        auto rb = std::make_unique<RewriteBlocking>(rw);
        rb->operator()(ce, fence);
      } else {
        // rewrite to async with empire
        auto ra = std::make_unique<RewriteAsync>(rw);
        ra->operator()(ce);
      }

//       // Only match files based on user's input
//       if (Matcher != "") {
// #if LLVM_VERSION_MAJOR > 7
//         auto file = rw.getSourceMgr().getFilename(ce->getEndLoc());
// #else
//         auto file = rw.getSourceMgr().getFilename(ce->getLocEnd());
// #endif
//         fmt::print("considering filename={}, regex={}\n", file.str(), Matcher);

//         std::regex re(Matcher);
//         std::cmatch m;
//         if (std::regex_match(file.str().c_str(), m, re)) {
//           fmt::print("=== Invoking rewriter on matched result in {} ===\n", file.str());
//           // we need to process this match
//         } else {
//           // break out and ignore this file
//           return;
//         }

//         if (processed_files.find(file.str()) != processed_files.end()) {
//           fmt::print("already generated for filename={}\n", file.str());
//           return;
//         }

//         new_processed_files.insert(file.str());
//       }

//       fmt::print(
//         "Traversing function \"{}\" ptr={}\n",
//         ce->getDirectCallee()->getNameInfo().getAsString(),
//         static_cast<void const*>(ce)
//       );
//       ce->dumpPretty(ce->getDirectCallee()->getASTContext());
//       fmt::print("\n");
//       ce->dumpColor();

// #if LLVM_VERSION_MAJOR > 7
//       auto loc = ce->getEndLoc();
// #else
//       auto loc = ce->getLocEnd();
// #endif

//       int offset = Lexer::MeasureTokenLength(loc, rw.getSourceMgr(), rw.getLangOpts()) + 1;

//       SourceLocation loc2 = loc.getLocWithOffset(offset);
//       rw.InsertText(loc2, "\nKokkos::fence();", true, true);
//     } else {
//       fmt::print(stderr, "traversing something unknown?\n");
//       exit(1);
//     }
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

    return std::make_unique<MyASTConsumer>(rw_);
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

  if (Processed != "") {
    std::ifstream file(Processed);
    if (file.good()) {
      std::string line;
      while (std::getline(file, line)) {
        processed_files.insert(line);
      }
      file.close();
    }
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

  if (Processed != "") {
    std::ofstream file(Processed);
    for (auto&& elm : new_processed_files) {
      file << elm << "\n";
    }
    for (auto&& elm : processed_files) {
      file << elm << "\n";
    }
    file.close();
  };

  return 0;
}

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

static cl::opt<bool> DoFencesOnly("fence-only", cl::desc("Rewrite fences only"));

static cl::opt<bool> RequireStrings("require-strings", cl::desc("Add strings to empire::parallel*"));

static cl::opt<bool> DeepCopy("deep-copy", cl::desc("Transform Kokkos::deep_copy to empire::deep_copy"));

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

StatementMatcher DeepCopyMatcher =
  callExpr(
    callee(
      functionDecl(
        hasName("::Kokkos::deep_copy")
      )
    )
  ).bind("callExpr");

StatementMatcher EmpireCallMatcher =
  callExpr(
    callee(
      functionDecl(
        anyOf(
          matchesName("::empire::parallel_*"),
          matchesName("::empire::deep_copy*")
        )
      )
    )
  ).bind("callExpr");

StatementMatcher FenceMatcher =
  hasDescendant(callExpr(
    callee(
      functionDecl(
        hasName("::Kokkos::fence")
      )
    )
  ).bind("fenceExpr"));


struct FenceCallback : MatchFinder::MatchCallback {
  virtual void run(const MatchFinder::MatchResult &Result) {
    if (CallExpr const *ce = Result.Nodes.getNodeAs<clang::CallExpr>("fenceExpr")) {
      out_ce = ce;
      found = true;
    }
  }
  bool found = false;
  CallExpr const *out_ce = nullptr;
};

StatementMatcher TemporaryMatcher = cxxTemporaryObjectExpr().bind("tempExpr");

struct TempCallback : MatchFinder::MatchCallback {
  virtual void run(const MatchFinder::MatchResult &Result) {
    if (Result.Nodes.getNodeAs<clang::CXXTemporaryObjectExpr>("tempExpr")) {
      found = true;
    }
  }
  bool found = false;
};


StatementMatcher DeclRefMatcher = hasDescendant(declRefExpr().bind("declRefExpr"));

struct DeclRefCallback : MatchFinder::MatchCallback {
  virtual void run(const MatchFinder::MatchResult &Result) {
    if (Result.Nodes.getNodeAs<clang::DeclRefExpr>("declRefExpr")) {
      expr = Result.Nodes.getNodeAs<clang::DeclRefExpr>("declRefExpr");
      found = true;
    }
  }

  DeclRefExpr const *expr = nullptr;
  bool found = false;
};

DeclarationMatcher CXXConstructMatcher = varDecl(
  has(cxxConstructExpr().bind("cxxConstructExpr")),
  hasType(classTemplateSpecializationDecl().bind("typename"))
).bind("varname");

struct ConstructCallback : MatchFinder::MatchCallback {
  virtual void run(const MatchFinder::MatchResult &Result) {
    fmt::print("ConstructCallback: {}\n", Result.Nodes.getMap().size());
    if (Result.Nodes.getNodeAs<clang::CXXConstructExpr>("cxxConstructExpr")) {
      expr = Result.Nodes.getNodeAs<clang::CXXConstructExpr>("cxxConstructExpr");
      found = true;
    }
  }

  CXXConstructExpr const *expr = nullptr;
  bool found = false;
};

DeclarationMatcher TemporaryObjectMatcher =
  varDecl(
    hasDescendant(cxxTemporaryObjectExpr().bind("cxxTemp"))
  );

struct TemporaryObjectCallback : MatchFinder::MatchCallback {
  virtual void run(const MatchFinder::MatchResult &Result) {
    fmt::print("TemporaryObjectCallback: {}\n", Result.Nodes.getMap().size());
    if (Result.Nodes.getNodeAs<clang::CXXTemporaryObjectExpr>("cxxTemp")) {
      expr = Result.Nodes.getNodeAs<clang::CXXTemporaryObjectExpr>("cxxTemp");
      found = true;
    }
  }

  CXXTemporaryObjectExpr const *expr = nullptr;
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

struct RewriteArgument {
  explicit RewriteArgument(Rewriter& in_rw)
    : rw(in_rw)
  { }

  void operator()(
    CallExpr const* par, SourceManager const* sm,
    MatchFinder::MatchResult const& Result
  ) const {
    bool first_is_string = false;
    auto arg_iter = par->arg_begin();
    auto const& first_arg = *arg_iter;
    QualType const& type = first_arg->getType();
    // fmt::print("type={}\n", type.getAsString());
    if (type.getAsString() == "std::string" or type.getAsString() == "const std::string") {
      // fmt::print("first argument is string\n");
      first_is_string = true;
      arg_iter++;
    }
    auto const& policy = *arg_iter;
    policy->dumpColor();
    QualType const& policy_type = policy->getType();
    fmt::print("policy=\"{}\"\n", policy_type.getAsString());
    if (
      policy_type.getAsString() == "size_t" or
      policy_type.getAsString() == "std::size_t" or
      policy_type.getAsString() == "int" or
      policy_type.getAsString() == "long" or
      policy_type.getAsString() == "const long" or
      policy_type.getAsString() == "const size_t" or
      policy_type.getAsString() == "const int" or
      policy_type.getAsString() == "const unsigned long" or
      policy_type.getAsString() == "const empire::LocalOrdinal"
    ) {
      fmt::print("range is a basic policy={}\n", policy_type.getAsString());
    } else {
      // advanced policy must lift to builder
      fmt::print("range is advanced policy\n");

      // MatchFinder temp_matcher;
      // auto tc = std::make_unique<TempCallback>();
      // temp_matcher.addMatcher(TemporaryMatcher, tc.get());
      // temp_matcher.match(*policy, *Result.Context);

      // if (not tc->found) {
      //   fmt::print("advanced policy--not-found\n");

      //   MatchFinder decl_ref_matcher;
      //   auto dc = std::make_unique<DeclRefCallback>();
      //   decl_ref_matcher.addMatcher(DeclRefMatcher, dc.get());
      //   decl_ref_matcher.match(*policy, *Result.Context);

      //   if (dc->found) {
      //     fmt::print("declRefExpr descendant found\n");
      //     auto expr = dc->expr;
      //     expr->dumpColor();
      //     auto decl = expr->getDecl();
      //     decl->dumpColor();
      //     fmt::print("type={}\n",decl->getType().getAsString());
      //     fmt::print("name={}\n", decl->getNameAsString());

      //     QualType type = decl->getType();
      //     auto type_name = type.getBaseTypeIdentifier()->getName();
      //     fmt::print("typename={}\n",type_name.str());

      //     MatchFinder cxx_matcher;
      //     auto cc = std::make_unique<ConstructCallback>();
      //     cxx_matcher.addMatcher(CXXConstructMatcher, cc.get());
      //     cxx_matcher.match(*decl, *Result.Context);

      //     fmt::print("found={} construct\n", cc->found);

      //     if (cc->found) {
      //       PrintingPolicy p(LangOptions{});
      //       p.SuppressTagKeyword = true;
      //       replaceType(sm, type.getAsString(p), getBegin(cc->expr));
      //     } else {
      //       MatchFinder temp_object_matcher;
      //       auto to = std::make_unique<TemporaryObjectCallback>();
      //       temp_object_matcher.addMatcher(TemporaryObjectMatcher, to.get());
      //       temp_object_matcher.match(*decl, *Result.Context);

      //       fmt::print("TO callback {}\n", to->found);
      //       if (to->found) {
      //         QualType totype = to->expr->getType();
      //         PrintingPolicy p(LangOptions{});
      //         p.SuppressTagKeyword = true;
      //         replaceType(sm, totype.getAsString(p), getBegin(to->expr));
      //       }
      //     }

      //     if (isa<VarDecl>(decl)) {
      //       auto var = cast<VarDecl>(decl);
      //       char const* ci = sm->getCharacterData(var->getTypeSourceInfo()->getTypeLoc().getBeginLoc());
      //       std::string first5 = std::string{ci, 5};
      //       //fmt::print("CICI: {}\n", first5);
      //       if (first5 != "auto ") {
      //         rw.ReplaceText(
      //           SourceRange{
      //             var->getTypeSourceInfo()->getTypeLoc().getBeginLoc(),
      //             var->getTypeSourceInfo()->getTypeLoc().getEndLoc()
      //           },
      //           "auto"
      //         );
      //       }
      //     }
      //   }

      //   return;
      // } else {
      //   PrintingPolicy p(LangOptions{});
      //   p.SuppressTagKeyword = true;
      //   replaceType(sm, policy_type.getAsString(p), getBegin(policy));
      // }

      // policy->dumpColor();
    }

  }

  template <typename T>
  void replaceType(SourceManager const* sm, std::string str, T begin) const {
    int offset = 0;
    //
    // unfortunately, this doesn't exist in Clang 7-9. it's introduced a lot
    // later in clang 13?
    //
    // p.SplitTemplateClosers = false;
    //
    if (str.substr(0, 6) == "const ") {
      str = str.substr(6, str.size()-1);
      offset += 6;
    }
    if (str.substr(0, 6) == "class ") {
      str = str.substr(6, str.size()-1);
      offset += 6;
    }
    if (str.substr(0, 7) == "struct ") {
      str = str.substr(7, str.size()-1);
      offset += 7;
    }

    auto t = str;
    std::string main_type = "";
    std::string temp_type = "";
    for (std::size_t i = 0; i < t.size(); i++) {
      if (t[i] == '<') {
        main_type = t.substr(0, i);
        temp_type = t.substr(i+1, t.size()-i-2);
        break;
      }
    }

    if (main_type == "") {
      main_type = t;
    }

    temp_type = replaceSpacedTemplates(temp_type);

    while (temp_type.size() > 0 and temp_type[temp_type.size()-1] == ' ') {
      temp_type = temp_type.substr(0, temp_type.size()-1);
    }

    fmt::print("MAIN: {}\n", main_type);
    fmt::print("TEMP: {}\n", temp_type);

    std::string new_type = "";
    if (temp_type == "") {
      new_type = fmt::format("empire::buildKokkosPolicy<{}>", main_type);
    } else {
      new_type = fmt::format("empire::buildKokkosPolicy<{}, {}>", main_type, temp_type);
    }
    fmt::print("new type={}\n", new_type);

    char const* ci = sm->getCharacterData(begin);
    char const* c = ci;
    while (*c != '(' and *c != '{') {
      c++;
    }
    auto end = c - ci;

    rw.ReplaceText(begin, end, new_type);
  }

  std::string replaceSpacedTemplates(std::string in) const {
    for (size_t i = 0; i < in.size(); i++) {
      auto sub = in.substr(i, std::string::npos);
      if (sub.size() >= 3) {
        if (sub.substr(0, 3) == "> >") {
          in.replace(i, 3, ">>");
          return replaceSpacedTemplates(in);
        }
      }
    }
    return in;
  }

private:
  Rewriter& rw;
};


struct RewriteArgumentString {
  explicit RewriteArgumentString(Rewriter& in_rw)
    : rw(in_rw)
  { }

  void operator()(
    CallExpr const* par, SourceManager const* sm,
    MatchFinder::MatchResult const& Result
  ) const {
    auto arg_iter = par->arg_begin();
    auto const& first_arg = *arg_iter;
    QualType const& type = first_arg->getType();
    fmt::print("type={}\n", type.getAsString());
    if (type.getAsString() == "std::string" or
        type.getAsString() == "const std::string" or
        type.getAsString() == "std::string const" or
        type.getAsString() == "std::string&" or
        type.getAsString() == "const std::string&" or
        type.getAsString() == "const& std::string" or
        type.getAsString() == "char *" or
        type.getAsString() == "char const *" or
        type.getAsString() == "const char *") {
      fmt::print("first argument is string\n");
      return;
    } else {

      FunctionDecl const* out_func = nullptr;
      RecordDecl const* out_record = nullptr;

      {
        auto NodeList = Result.Context->getParents(*par);
        while (!NodeList.empty()) {
          // Get the first parent.
          auto ParentNode = NodeList[0];

          // Is the parent a FunctionDecl?
          if (const FunctionDecl *Parent = ParentNode.get<FunctionDecl>()) {
            llvm::outs() << "Found ancestor FunctionDecl: "
                         << (void const*)Parent << '\n';
            llvm::outs() << "FunctionDecl name: "
                         << Parent->getNameAsString() << '\n';
            out_func = Parent;
            break;
          }

          // It was not a FunctionDecl.  Keep going up.
          NodeList = Result.Context->getParents(ParentNode);
        }
      }

      if (out_func) {
        auto NodeList = Result.Context->getParents(*out_func->getCanonicalDecl());
        while (!NodeList.empty()) {
          // Get the first parent.
          auto ParentNode = NodeList[0];


          if (const RecordDecl *Parent = ParentNode.get<RecordDecl>()) {
            llvm::outs() << "Found ancestor RecordDecl: "
                         << (void const*)Parent << '\n';
            llvm::outs() << "RecordDecl name: "
                         << Parent->getNameAsString() << '\n';
            out_record = Parent;
            break;
          }
          NodeList = Result.Context->getParents(ParentNode);
        }
      }

      std::string label;
      if (out_record != nullptr) {
        label += out_record->getNameAsString();
      }
      if (out_func != nullptr) {
        label += "::" + out_func->getNameAsString();
      }
      if (out_record == nullptr and out_func == nullptr) {
        label += "PLEASE-ADD";
      }

      rw.InsertTextBefore(getBegin(first_arg), "\"" + label + "\", ");
    }
  }

private:
  Rewriter& rw;
};

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
        //rw.InsertTextBefore(begin, "empire::");
      }
    }

    // Switch to parallel_* blocking
    {
      auto end = getEnd(par->getCallee());
      rw.InsertTextAfterToken(end, "_blocking");
    }

    // Remove the fence line
    if (fence) {
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

struct DeepCopyRewriter  : MatchFinder::MatchCallback {
  explicit DeepCopyRewriter(Rewriter& in_rw)
    : rw(in_rw)
  { }

  virtual void run(const MatchFinder::MatchResult &Result) {
    CallExpr const* ce = Result.Nodes.getNodeAs<clang::CallExpr>("callExpr");

    if (ce) {
      auto file = rw.getSourceMgr().getFilename(getEnd(ce));

      fmt::print("considering filename={}, regex={}\n", file.str(), Matcher);

      std::regex re(Matcher);
      std::cmatch m;
      if (std::regex_match(file.str().c_str(), m, re)) {
        fmt::print("=== Invoking rewriter on matched result in {} ===\n", file.str());
        // we need to process this match
      } else {
        // break out and ignore this file
        return;
      }

      if (processed_files.find(file.str()) != processed_files.end()) {
        fmt::print("already generated for filename={}\n", file.str());
        return;
      }

      new_processed_files.insert(file.str());

      if (locs.find(getBegin(ce)) != locs.end()) {
        return;
      }
      locs.insert(getBegin(ce));

      ce->dumpColor();
      auto rb = std::make_unique<RewriteBlocking>(rw);
      rb->operator()(ce, nullptr);
    }
  }

  Rewriter& rw;
  std::set<SourceLocation> locs;
};

struct ParallelForRewriter : MatchFinder::MatchCallback {
  explicit ParallelForRewriter(Rewriter& in_rw)
    : rw(in_rw)
  { }

  virtual void run(const MatchFinder::MatchResult &Result) {

    CallExpr const *ce_outer = nullptr;
    ce_outer = Result.Nodes.getNodeAs<clang::CallExpr>("fenceExpr");
    if (!ce_outer) {
      ce_outer = Result.Nodes.getNodeAs<clang::CallExpr>("callExpr");
    }

    if (!ce_outer) {
      return;
    }

    auto file = rw.getSourceMgr().getFilename(getEnd(ce_outer));

    fmt::print("considering filename={}, regex={}\n", file.str(), Matcher);

    std::regex re(Matcher);
    std::cmatch m;
    if (std::regex_match(file.str().c_str(), m, re)) {
      fmt::print("=== Invoking rewriter on matched result in {} ===\n", file.str());
      // we need to process this match
    } else {
      // break out and ignore this file
      return;
    }

    if (processed_files.find(file.str()) != processed_files.end()) {
      fmt::print("already generated for filename={}\n", file.str());
      return;
    }

    new_processed_files.insert(file.str());

    if (locs.find(getBegin(ce_outer)) != locs.end()) {
      return;
    }
    locs.insert(getBegin(ce_outer));

    if (CallExpr const *ce = Result.Nodes.getNodeAs<clang::CallExpr>("fenceExpr")) {
      auto begin = getBegin(ce);

      auto str = rw.getRewrittenText(SourceRange(begin, begin.getLocWithOffset(6)));
      fmt::print("str={}\n", str);
      if (str == "Kokkos::") {
        rw.ReplaceText(begin, 6, "empire");
      }
    }

    if (RequireStrings) {
      if (CallExpr const *ce = Result.Nodes.getNodeAs<clang::CallExpr>("callExpr")) {

        auto rr = std::make_unique<RewriteArgumentString>(rw);
        rr->operator()(ce, Result.SourceManager, Result);

      }
    } else {
      if (CallExpr const *ce = Result.Nodes.getNodeAs<clang::CallExpr>("callExpr")) {

        auto& ctx = Result.Context;
        auto p = ctx->getParents(*ce);
        fmt::print("size={}\n", p.size());

        if (p.size() != 1) {
          return;
        }

        ExprWithCleanups const* ewc = nullptr;
        if (isa<ExprWithCleanups>(p[0].get<Stmt>())) {
          fmt::print("isa ExprWithCleanups\n");
          ewc = cast<ExprWithCleanups>(p[0].get<Stmt>());
          p = ctx->getParents(*ewc);
        }

        bool found_fence = false;
        CallExpr const* fence = nullptr;
        for (std::size_t i = 0; i < p.size(); i++) {
          Stmt const* st = p[i].get<Stmt>();
          //st->dumpColor();
          if (isa<CompoundStmt>(st)) {
            auto const& cs = cast<CompoundStmt>(st);
            if (cs->size() == 1) {
              break;
            }
            int x = 0;
            for (auto iter = cs->child_begin(); iter != cs->child_end(); ++iter) {
              x++;
              if (x > cs->size()) {
                goto out;
              }
              if (*iter == ce or *iter == ewc) {
                iter++;
                if (iter != cs->child_end()) {
                  if (isa<WhileStmt>(*iter) or isa<IfStmt>(*iter) or isa<CompoundStmt>(*iter) or isa<ForStmt>(*iter) or isa<CXXForRangeStmt>(*iter)) {
                    iter->dumpColor();
                    fmt::print("Not traversing stmt after\n");
                  } else {
                    //iter->dumpColor();
                    MatchFinder fence_matcher;
                    auto fc = std::make_unique<FenceCallback>();
                    fence_matcher.addMatcher(FenceMatcher, fc.get());
                    fence_matcher.match(**iter, *Result.Context);
                    found_fence = fc->found;
                    iter->dumpColor();
                    if (fc->found) {
                      fence = fc->out_ce;
                      fmt::print("FOUND fence\n");
                    } else {
                      fmt::print("NOT FOUND fence\n");
                    }
                  }
                }
              }
            }
          }

          break;
        }

      out:

        auto rr = std::make_unique<RewriteArgument>(rw);
        rr->operator()(ce, Result.SourceManager, Result);

        if (found_fence) {
          // rewrite to blocking with empire
          auto rb = std::make_unique<RewriteBlocking>(rw);
          rb->operator()(ce, fence);
        } else {
          // rewrite to async with empire
          auto ra = std::make_unique<RewriteAsync>(rw);
          ra->operator()(ce);
        }

      }
    }
  }

  std::set<SourceLocation> locs;
  Rewriter& rw;
};

struct MyASTConsumer : ASTConsumer {
  MyASTConsumer(Rewriter& in_rw) : call_handler_(in_rw), deep_copy_handler_(in_rw) {

    if (DoFencesOnly) {
      matcher_.addMatcher(FenceMatcher, &call_handler_);
    } else {
      if (RequireStrings) {
        matcher_.addMatcher(CallMatcher, &call_handler_);
      } else if (DeepCopy) {
        matcher_.addMatcher(DeepCopyMatcher, &deep_copy_handler_);
      } else {
        matcher_.addMatcher(CallMatcher, &call_handler_);
      }
    }
  }

  void HandleTranslationUnit(ASTContext& Context) override {
    // Run the matchers when we have the whole TU parsed.
    matcher_.matchAST(Context);
  }

private:
  ParallelForRewriter call_handler_;
  DeepCopyRewriter deep_copy_handler_;
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

//===--- ThisUsageCheck.h - SynthAxesVST3 clang-tidy check ----------------===//
//
// Enforces CLAUDE.md §5 "Member Access":
//   * every access to a non-static data member or method from inside a class is
//     qualified with `this->`;
//   * every access to a static member is qualified with `ClassName::`.
//
// No stock clang-tidy check does this (the implicit-`this` bit is not exposed to
// AST matchers), which is why it is a custom check. It offers fix-its, so it can
// insert the missing qualifier automatically.
//
//===----------------------------------------------------------------------===//
#pragma once

#include "clang-tidy/ClangTidyCheck.h"

namespace synthaxes::tidy
{
    class ThisUsageCheck : public clang::tidy::ClangTidyCheck
    {
    public:
        ThisUsageCheck(llvm::StringRef name, clang::tidy::ClangTidyContext* context);

        void registerMatchers(clang::ast_matchers::MatchFinder* finder) override;
        void check(const clang::ast_matchers::MatchFinder::MatchResult& result) override;
    };

} // namespace synthaxes::tidy

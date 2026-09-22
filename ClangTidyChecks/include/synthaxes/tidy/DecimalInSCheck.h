//===--- DecimalInSCheck.h - SynthAxesVST3 clang-tidy check ---------------===//
//
// Enforces CLAUDE.md §6 "Decimal Constants": every floating-point literal is
// wrapped in the S(...) macro -- S(0.5), S(1.0), S(2.0 * x), S(1e-5). Integer
// literals are left alone. No stock check does this; it offers a fix-it that
// wraps the literal.
//
// "Already wrapped" = the literal is the argument of the S macro. A literal that
// is the argument of a DIFFERENT macro (e.g. EXPECT_NEAR(a, b, 1e-5)) still needs
// wrapping and is flagged; a literal from a macro BODY (M_PI, a first-party
// constant macro) is left to its definition and skipped here.
//
//===----------------------------------------------------------------------===//
#pragma once

#include "clang-tidy/ClangTidyCheck.h"

namespace synthaxes::tidy
{
    class DecimalInSCheck : public clang::tidy::ClangTidyCheck
    {
    public:
        DecimalInSCheck(llvm::StringRef name, clang::tidy::ClangTidyContext* context);

        void registerMatchers(clang::ast_matchers::MatchFinder* finder) override;
        void check(const clang::ast_matchers::MatchFinder::MatchResult& result) override;
    };

} // namespace synthaxes::tidy

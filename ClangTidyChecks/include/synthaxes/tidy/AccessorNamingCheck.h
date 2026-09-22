//===--- AccessorNamingCheck.h - SynthAxesVST3 clang-tidy check -----------===//
//
// Enforces the accessor-naming convention: an accessor is named `getX()` / `setX()`,
// never the bare noun `x()`. A member function whose name equals a private or
// protected non-static data member with its `m_` prefix stripped (method `resistance`
// alongside field `m_resistance`) is the bare-noun form and is flagged.
//
// This catches both halves with one rule: a bare getter `resistance()` and a bare
// setter `resistance(Real)` both match the field name, while the correct `getResistance`
// / `setResistance` (and the boolean `isX` / `hasX`) do not.
//
// Only accessor-SHAPED methods qualify -- a getter (no parameters, returns the field's type by
// value, reference or pointer) or a setter (returns void, one parameter of the field's type). A
// same-named method of another shape (an `undo(id, value)` command over an `m_undo` stack, an
// `onParameterEdited(...)` callback) is an action, not an accessor, and is left alone. Virtual
// methods are skipped -- renaming one would have to move a whole override set.
//
// By default the check only WARNS (at the declaration): renaming a method has to move its call
// sites too, so it is not something a per-file `--fix` can do safely. Set the `Fix` option to true
// to opt into rename fix-its that rewrite the declaration, its out-of-line definition and every
// reference (`getX` / `setX` / `isX`); apply them across the whole tree at once with
// `--export-fixes` + clang-apply-replacements so cross-translation-unit references stay consistent.
//
//===----------------------------------------------------------------------===//
#pragma once

#include "clang-tidy/ClangTidyCheck.h"

#include <string>
#include <vector>

namespace synthaxes::tidy
{
    class AccessorNamingCheck : public clang::tidy::ClangTidyCheck
    {
    public:
        AccessorNamingCheck(llvm::StringRef name, clang::tidy::ClangTidyContext* context);

        void registerMatchers(clang::ast_matchers::MatchFinder* finder) override;
        void check(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

    private:
        /// When true, emit rename fix-its (declaration, out-of-line definition and references);
        /// otherwise warn only. Set via the check's `Fix` option; off by default. Drives the
        /// one-off rename campaign.
        bool m_emitFixes;

        /// Method names never flagged even when they match an `m_` field -- idiomatic container
        /// accessors (`size`, `count`, `empty`, ...) that read better bare than as `getSize()`. Seeded
        /// with a fixed base set, extended by the comma-separated `IgnoredNames` option.
        std::vector<std::string> m_ignoredNames;
    };

} // namespace synthaxes::tidy

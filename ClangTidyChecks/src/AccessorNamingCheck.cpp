//===--- AccessorNamingCheck.cpp - SynthAxesVST3 clang-tidy check ---------===//
#include "synthaxes/tidy/AccessorNamingCheck.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "llvm/ADT/SmallVector.h"

#include <cctype>
#include <string>
#include <vector>

using namespace clang;
using namespace clang::ast_matchers;

namespace synthaxes::tidy
{
    namespace
    {
        /// @p name with its first character upper-cased, for building a `getX` / `setX` suggestion.
        std::string capitalize(StringRef name)
        {
            std::string s = name.str();
            if(!s.empty())
                s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
            return s;
        }

        /// The private/protected data member whose name is `m_<method>` -- i.e. the field this
        /// bare-noun method is the accessor for -- or null if the class declares no such member.
        const FieldDecl* matchingMember(const CXXRecordDecl* record, StringRef methodName)
        {
            const std::string wanted = ("m_" + methodName).str();
            for(const FieldDecl* field : record->fields())
            {
                const AccessSpecifier access = field->getAccess();
                if(access != AS_private && access != AS_protected)
                    continue;
                if(field->getName() == wanted)
                    return field;
            }
            return nullptr;
        }

        /// The canonical, cv- and reference-stripped form of @p type, for comparing a return type or
        /// parameter type against a field type regardless of `const`, `&` or typedef sugar.
        QualType bareType(QualType type)
        {
            return type.getNonReferenceType().getCanonicalType().getUnqualifiedType();
        }

        /// True when @p type is the field's own type, or a reference or pointer to it -- the shapes a
        /// value getter, a reference getter (`const T&`), a pointer getter (`T*`) or a setter
        /// parameter may legitimately take. @p fieldBare is the field's bareType().
        bool correspondsToField(QualType type, QualType fieldBare)
        {
            const QualType bare = bareType(type);
            if(bare == fieldBare)
                return true;
            if(bare->isPointerType() && bareType(bare->getPointeeType()) == fieldBare)
                return true;
            return false;
        }

        /// True when @p name already begins with accessor prefix @p prefix followed by an upper-case
        /// letter (`isRamping` has "is", `hasBackground` has "has", `getFoo` has "get") -- i.e. the
        /// name is already in accessor form and must not have the prefix applied a second time.
        bool hasAccessorPrefix(StringRef name, StringRef prefix)
        {
            return name.size() > prefix.size() && name.starts_with(prefix) && std::isupper(static_cast<unsigned char>(name[prefix.size()]));
        }

        /// The `getX` / `setX` / `isX` name a renamable accessor should have, or "" when @p method is
        /// not one. Deterministic in the method's own declaration, so the declaration, its definition
        /// and every reference independently arrive at the same target -- which is what lets the
        /// per-site fix-its merge consistently across translation units.
        std::string accessorNewName(const CXXMethodDecl* method, const std::vector<std::string>& ignoredNames)
        {
            if(method == nullptr || method->isImplicit())
                return {};

            // Only real, named, non-virtual instance member functions. Constructors, destructors and
            // conversion/operator functions have no plain identifier to collide with a field; a
            // virtual method cannot be renamed alone (it would leave its override set behind).
            if(isa<CXXConstructorDecl>(method) || isa<CXXDestructorDecl>(method) || isa<CXXConversionDecl>(method))
                return {};
            if(method->isStatic() || method->isVirtual() || method->isOverloadedOperator() || !method->getDeclName().isIdentifier())
                return {};

            const CXXRecordDecl* record = method->getParent();
            if(record == nullptr)
                return {};

            const StringRef name = method->getName();

            // Idiomatic container accessors (size, count, empty, ...) read better bare and are exempt.
            for(const std::string& ignored : ignoredNames)
                if(name == ignored)
                    return {};

            const FieldDecl* field = matchingMember(record, name);
            if(field == nullptr)
                return {};

            // Only an accessor whose SIGNATURE matches the member's type is a real accessor: a getter
            // takes nothing and returns the field's type (value, reference or pointer); a setter
            // returns void and takes one parameter of the field's type. A same-named method of another
            // shape -- undo(id, value), an onEdited(...) callback -- does not correspond and is left be.
            const QualType fieldBare = bareType(field->getType());
            const bool isGetter = method->getNumParams() == 0 && correspondsToField(method->getReturnType(), fieldBare);
            const bool isSetter = method->getReturnType()->isVoidType() && method->getNumParams() == 1 && correspondsToField(method->getParamDecl(0)->getType(), fieldBare);
            if(!isGetter && !isSetter)
                return {};

            // Skip names already in accessor form -- a bool field literally named `m_isRamping` has the
            // correct getter `isRamping()`, and prepending would give the double `isIsRamping`.
            if(isSetter)
            {
                if(hasAccessorPrefix(name, "set"))
                    return {};
                return "set" + capitalize(name);
            }
            if(fieldBare->isBooleanType())
            {
                if(hasAccessorPrefix(name, "is") || hasAccessorPrefix(name, "has"))
                    return {};
                return "is" + capitalize(name); // a bool getter reads isEnabled(), not getEnabled()
            }
            if(hasAccessorPrefix(name, "get"))
                return {};
            return "get" + capitalize(name);
        }

        /// A fix-it that replaces the identifier token at @p loc with @p newName.
        FixItHint renameToken(SourceLocation loc, StringRef newName)
        {
            return FixItHint::CreateReplacement(CharSourceRange::getTokenRange(loc, loc), newName);
        }
    } // namespace

    AccessorNamingCheck::AccessorNamingCheck(StringRef name, clang::tidy::ClangTidyContext* context): ClangTidyCheck(name, context), m_emitFixes(Options.get("Fix", "false") == "true")
    {
        // Idiomatic container accessors read better bare than as getSize()/getCount(); this base set is
        // unconditional so it survives a bare `--config` that only sets other options. `IgnoredNames`
        // adds to it.
        this->m_ignoredNames = { "size", "count", "length", "empty", "capacity", "data", "begin", "end", "front", "back" };
        SmallVector<StringRef, 8> extra;
        StringRef(Options.get("IgnoredNames", "")).split(extra, ',', -1, false);
        for(StringRef part : extra)
        {
            const StringRef trimmed = part.trim();
            if(!trimmed.empty())
                this->m_ignoredNames.emplace_back(trimmed.str());
        }
    }

    void AccessorNamingCheck::registerMatchers(MatchFinder* finder)
    {
        finder->addMatcher(cxxMethodDecl().bind("method"), this);
        // References are matched only to rewrite them during a rename; the warning lives on the
        // declaration, so the reference matchers do nothing useful unless fixes are being emitted.
        if(this->m_emitFixes)
        {
            finder->addMatcher(memberExpr().bind("member"), this);
            finder->addMatcher(declRefExpr().bind("ref"), this);
        }
    }

    void AccessorNamingCheck::check(const MatchFinder::MatchResult& result)
    {
        if(const auto* method = result.Nodes.getNodeAs<CXXMethodDecl>("method"))
        {
            // Drive the report from the canonical (in-class) declaration only, so a method with a
            // separate out-of-line definition is reported once, not twice.
            if(method != method->getCanonicalDecl())
                return;
            const std::string newName = accessorNewName(method, this->m_ignoredNames);
            if(newName.empty())
                return;

            auto builder = diag(method->getLocation(), "accessor %0 should be named %1 to match its data member") << method->getName() << newName;
            if(this->m_emitFixes)
            {
                builder << renameToken(method->getLocation(), newName);
                if(const FunctionDecl* def = method->getDefinition(); def != nullptr && def != method)
                    builder << renameToken(def->getLocation(), newName);
            }
            return;
        }

        // A reference (member access or a pointer-to-member) to a flagged accessor: rewrite it so the
        // call sites follow the renamed declaration. Only reached when fixes are being emitted.
        if(const auto* member = result.Nodes.getNodeAs<MemberExpr>("member"))
        {
            const std::string newName = accessorNewName(dyn_cast_or_null<CXXMethodDecl>(member->getMemberDecl()), this->m_ignoredNames);
            if(newName.empty())
                return;
            diag(member->getMemberLoc(), "rename accessor reference to %0") << newName << renameToken(member->getMemberLoc(), newName);
            return;
        }
        if(const auto* ref = result.Nodes.getNodeAs<DeclRefExpr>("ref"))
        {
            const std::string newName = accessorNewName(dyn_cast_or_null<CXXMethodDecl>(ref->getDecl()), this->m_ignoredNames);
            if(newName.empty())
                return;
            diag(ref->getLocation(), "rename accessor reference to %0") << newName << renameToken(ref->getLocation(), newName);
        }
    }

} // namespace synthaxes::tidy

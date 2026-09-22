//===--- ThisUsageCheck.cpp - SynthAxesVST3 clang-tidy check --------------===//
#include "synthaxes/tidy/ThisUsageCheck.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

using namespace clang;
using namespace clang::ast_matchers;

namespace synthaxes::tidy
{
    ThisUsageCheck::ThisUsageCheck(StringRef name, clang::tidy::ClangTidyContext* context): ClangTidyCheck(name, context) {}

    void ThisUsageCheck::registerMatchers(MatchFinder* finder)
    {
        // (a) Every member access. The implicit-vs-explicit `this` distinction is not expressible
        //     as a matcher, so it is done in check() via MemberExpr::isImplicitAccess() -- which is
        //     true only for a bare access through an implicit `this`, and an implicit `this` can
        //     only exist inside a non-static member function, so no ancestor scoping is needed.
        finder->addMatcher(memberExpr().bind("member"), this);

        // (b) Every variable reference. Narrowed to an *unqualified reference to a static data
        //     member* in check() (there is no `isStaticDataMember` matcher, so the VarDecl is bound
        //     and filtered in C++).
        finder->addMatcher(declRefExpr(to(varDecl().bind("var"))).bind("ref"), this);
    }

    void ThisUsageCheck::check(const MatchFinder::MatchResult& result)
    {
        // --- this-> rule ---------------------------------------------------------------------
        if(const auto* member = result.Nodes.getNodeAs<MemberExpr>("member"))
        {
            // isImplicitAccess() is true exactly when the base `this->` was omitted -- i.e. the
            // CXXThisExpr the AST synthesised is implicit. `obj.field` and explicit `this->field`
            // both return false, so only a bare member access is flagged.
            //
            // A member access already written with an explicit qualifier (e.g. `Curve::method()`)
            // is left alone: inserting `this->` would produce the invalid `Curve::this->method`, and
            // dropping the qualifier would change dispatch (Curve:: forces the non-virtual call). So
            // only a BARE, unqualified implicit-this access is a violation.
            if(member->isImplicitAccess() && !member->hasQualifier())
            {
                diag(member->getMemberLoc(), "access to member %0 must be qualified with 'this->'") << member->getMemberNameInfo().getName() << FixItHint::CreateInsertion(member->getMemberLoc(), "this->");
            }
            return;
        }

        // --- ClassName:: rule ----------------------------------------------------------------
        if(const auto* ref = result.Nodes.getNodeAs<DeclRefExpr>("ref"))
        {
            const auto* var = result.Nodes.getNodeAs<VarDecl>("var");
            if(var == nullptr || !var->isStaticDataMember())
                return;
            // Already written `Foo::x` (or any qualifier) -> compliant.
            if(ref->hasQualifier())
                return;

            const auto* record = dyn_cast<CXXRecordDecl>(var->getDeclContext());
            if(record == nullptr || !record->getIdentifier())
                return;

            diag(ref->getLocation(), "access to static member %0 must be qualified with '%1::'") << var->getName() << record->getName() << FixItHint::CreateInsertion(ref->getLocation(), (record->getName() + "::").str());
            return;
        }
    }

} // namespace synthaxes::tidy

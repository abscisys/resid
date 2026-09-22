//===--- DecimalInSCheck.cpp - SynthAxesVST3 clang-tidy check -------------===//
#include "synthaxes/tidy/DecimalInSCheck.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Lex/Lexer.h"

using namespace clang;
using namespace clang::ast_matchers;

namespace synthaxes::tidy
{
    namespace
    {
        /// True when @p qt, seen through its typedef sugar, is the `synthaxes::sample` alias.
        ///
        /// Only the `sample` typedef itself qualifies -- writing `double`, `float`, `sample32` or
        /// `sample64` directly does not. Arrays are unwrapped to their element type so that
        /// `sample buf[N]` counts as sample-typed.
        bool isSampleType(QualType qt)
        {
            qt = qt.getNonReferenceType();
            if(const ArrayType* at = qt->getAsArrayTypeUnsafe())
                qt = at->getElementType();
            while(const auto* tt = qt->getAs<TypedefType>())
            {
                if(tt->getDecl()->getName() == "sample")
                    return true;
                qt = tt->desugar();
            }
            return false;
        }

        /// Return type of the function that encloses @p stmt (null QualType if none).
        QualType enclosingReturnType(const Stmt* stmt, ASTContext& ctx)
        {
            DynTypedNodeList parents = ctx.getParents(*stmt);
            while(!parents.empty())
            {
                const DynTypedNode& p = parents[0];
                if(const auto* fd = p.get<FunctionDecl>())
                    return fd->getReturnType();
                if(const Stmt* s = p.get<Stmt>())
                    parents = ctx.getParents(*s);
                else if(const Decl* d = p.get<Decl>())
                    parents = ctx.getParents(*d);
                else
                    break;
            }
            return {};
        }

        /// Walks outward from a float literal to the value it ultimately becomes and reports whether
        /// that recipient is `synthaxes::sample`-typed. Deliberately conservative: an unrecognised
        /// context yields false, so the check can only ever SUPPRESS a flag, never invent one.
        ///
        /// Handled recipients: variable / field initializer, assignment LHS, `return`, call and
        /// constructor arguments, explicit casts, and arithmetic / comparison operands (a
        /// sample-typed sibling settles it; otherwise the walk continues to the enclosing value).
        /// Implicit casts, parentheses and temporaries are transparent.
        bool recipientIsSample(const Expr* lit, ASTContext& ctx)
        {
            const Stmt* cur = lit;
            for(int guard = 0; guard < 64; ++guard)
            {
                DynTypedNodeList parents = ctx.getParents(*cur);
                if(parents.empty())
                    return false;
                const DynTypedNode& p = parents[0];

                if(const auto* d = p.get<Decl>())
                {
                    if(const auto* vd = dyn_cast<VarDecl>(d))
                        return isSampleType(vd->getType());
                    if(const auto* fd = dyn_cast<FieldDecl>(d))
                        return isSampleType(fd->getType());
                    return false;
                }

                const auto* s = p.get<Stmt>();
                if(s == nullptr)
                    return false;
                if(isa<ReturnStmt>(s))
                    return isSampleType(enclosingReturnType(s, ctx));

                const auto* e = dyn_cast<Expr>(s);
                if(e == nullptr)
                    return false;

                if(const auto* bo = dyn_cast<BinaryOperator>(e))
                {
                    if(bo->isAssignmentOp())
                        return isSampleType(bo->getLHS()->getType());
                    const Expr* sibling = (cur == bo->getLHS()) ? bo->getRHS() : bo->getLHS();
                    if(isSampleType(sibling->getType()))
                        return true;
                    cur = bo;
                    continue;
                }
                if(const auto* call = dyn_cast<CallExpr>(e))
                {
                    const FunctionDecl* fn = call->getDirectCallee();
                    for(unsigned i = 0; i < call->getNumArgs(); ++i)
                        if(call->getArg(i) == cur)
                            return fn != nullptr && i < fn->getNumParams() && isSampleType(fn->getParamDecl(i)->getType());
                    return false;
                }
                if(const auto* ctor = dyn_cast<CXXConstructExpr>(e))
                {
                    const CXXConstructorDecl* cd = ctor->getConstructor();
                    for(unsigned i = 0; i < ctor->getNumArgs(); ++i)
                        if(ctor->getArg(i) == cur)
                            return cd != nullptr && i < cd->getNumParams() && isSampleType(cd->getParamDecl(i)->getType());
                    return false;
                }
                if(const auto* cast = dyn_cast<ExplicitCastExpr>(e))
                    return isSampleType(cast->getTypeAsWritten());

                // Transparent nodes (implicit cast, parens, temporaries, ?: , ...): keep walking.
                cur = e;
            }
            return false;
        }
    } // namespace

    DecimalInSCheck::DecimalInSCheck(StringRef name, clang::tidy::ClangTidyContext* context): ClangTidyCheck(name, context) {}

    void DecimalInSCheck::registerMatchers(MatchFinder* finder)
    {
        finder->addMatcher(floatLiteral().bind("lit"), this);
    }

    void DecimalInSCheck::check(const MatchFinder::MatchResult& result)
    {
        const auto* lit = result.Nodes.getNodeAs<FloatingLiteral>("lit");
        if(lit == nullptr)
            return;

        const SourceManager& sm = *result.SourceManager;
        const LangOptions& lo = result.Context->getLangOpts();
        const SourceLocation loc = lit->getBeginLoc();
        if(loc.isInvalid())
            return;

        if(loc.isMacroID())
        {
            // The literal reached us through a macro. If it is the argument of the S macro it is
            // correctly wrapped; if it is the argument of another macro (EXPECT_NEAR, ...) it still
            // needs S and is flagged at the argument text; if it comes from a macro BODY (M_PI, a
            // first-party constant macro) we cannot rewrite it here, so skip.
            if(!sm.isMacroArgExpansion(loc))
                return;
            if(Lexer::getImmediateMacroName(loc, sm, lo) == "S")
                return;
        }

        // §6 only applies to literals that become a synthaxes::sample value; a plain double/float
        // recipient (a UI coordinate, a preset value, a knob position) is left alone.
        if(!recipientIsSample(lit, *result.Context))
            return;

        // Wrap the literal's written text in S(...). getSpellingLoc resolves a macro-argument
        // location back to where the user actually typed the literal, so the fix targets real source.
        const SourceLocation begin = sm.getSpellingLoc(lit->getBeginLoc());
        const SourceLocation end = Lexer::getLocForEndOfToken(sm.getSpellingLoc(lit->getEndLoc()), 0, sm, lo);

        // A literal whose spelling lands outside our sources -- inside gtest's
        // comparison machinery, say -- cannot be rewritten and has no line to
        // report against. Diagnosing it produces a warning with no location,
        // which can be neither fixed nor suppressed. Skip it.
        if(begin.isInvalid() || end.isInvalid() || sm.isInSystemHeader(begin))
            return;

        // And one that has a location but no file behind it -- a literal
        // synthesised inside a template instantiation, say. clang-tidy renders
        // that as a bare warning with no path, which no one can act on.
        if(sm.getFilename(begin).empty())
            return;

        const CharSourceRange range = CharSourceRange::getCharRange(begin, end);

        // The suffix has to go. S(x) is `x##f` under SAMPLE32, so wrapping 0.12f
        // without stripping it yields 0.12ff, which does not compile -- and under
        // SAMPLE64 an f suffix would silently pin the literal to single precision,
        // which is the whole thing the macro exists to prevent. 0.12f becomes
        // S(0.12), exactly as if it had been written that way.
        StringRef text = Lexer::getSourceText(range, sm, lo);
        while(!text.empty() && (text.back() == 'f' || text.back() == 'F' || text.back() == 'l' || text.back() == 'L'))
            text = text.drop_back();

        diag(begin, "floating-point literal must be wrapped in the S(...) macro")
            << FixItHint::CreateReplacement(range, ("S(" + text + ")").str());
    }

} // namespace synthaxes::tidy

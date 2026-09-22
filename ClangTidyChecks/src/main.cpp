//===--- main.cpp - synthaxes-clang-tidy driver ---------------------------===//
//
// A clang-tidy executable built from the installed clang-tidy static libraries
// plus our own check module. No LLVM source build and no loadable plugin: the
// SDK's clangTidy*.lib are linked straight in.
//
// clang-tidy registers each check module through a static initializer that the
// linker discards when nothing references the module's translation unit. Stock
// clang-tidy defeats this with ClangTidyForceLinking.h, which the SDK does not
// install -- so we reconstruct the anchor references here, for the modules our
// .clang-tidy uses (Readability/Bugprone/Performance/Misc), Modernize (not in the
// standing policy, but its fixers -- e.g. modernize-use-override -- are used for
// on-demand `-fix` passes), plus our own.
//
//===----------------------------------------------------------------------===//
#include <clang-tidy/tool/ClangTidyMain.h>

namespace clang::tidy
{
    extern volatile int ReadabilityModuleAnchorSource;
    extern volatile int BugproneModuleAnchorSource;
    extern volatile int PerformanceModuleAnchorSource;
    extern volatile int MiscModuleAnchorSource;
    extern volatile int ModernizeModuleAnchorSource;
} // namespace clang::tidy

namespace synthaxes::tidy
{
    extern volatile int SynthaxesModuleAnchorSource;
}

// Referencing each anchor forces its module's object (and its check registration) to be linked.
static const int kLinkReadability = clang::tidy::ReadabilityModuleAnchorSource;
static const int kLinkBugprone = clang::tidy::BugproneModuleAnchorSource;
static const int kLinkPerformance = clang::tidy::PerformanceModuleAnchorSource;
static const int kLinkMisc = clang::tidy::MiscModuleAnchorSource;
static const int kLinkModernize = clang::tidy::ModernizeModuleAnchorSource;
static const int kLinkSynthaxes = synthaxes::tidy::SynthaxesModuleAnchorSource;

int main(int argc, const char** argv)
{
    return clang::tidy::clangTidyMain(argc, argv);
}

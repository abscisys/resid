//===--- SynthaxesTidyModule.cpp - registers the SynthAxesVST3 checks -----===//
//
// Registers the in-house check module with clang-tidy. When compiled into
// clang-tidy in-tree (the Windows-capable path -- loadable plugins do not work on
// Windows), the checks are available as `synthaxes-explicit-this`,
// `synthaxes-accessor-naming` and `synthaxes-decimal-in-s`.
//
// Every check with a source file and a fixture must appear in addCheckFactories
// below. A check that is compiled and linked but not registered here is silently
// absent: clang-tidy answers `--checks=synthaxes-<name>` with "no checks enabled"
// rather than an unknown-check error, so it looks like a harness fault.
//
//===----------------------------------------------------------------------===//
#include <synthaxes/tidy/AccessorNamingCheck.h>
#include <synthaxes/tidy/DecimalInSCheck.h>
#include <synthaxes/tidy/ThisUsageCheck.h>

#include <clang-tidy/ClangTidyModule.h>
#include <llvm/Config/llvm-config.h>
// The module-registry symbols moved into ClangTidyModule.h in LLVM 22; before that they live in a
// separate header. (In 22 that header became a deprecated forwarding shim whose #warning even
// breaks MSVC under c++17, so it must NOT be included there.)
#if LLVM_VERSION_MAJOR < 22
#include <clang-tidy/ClangTidyModuleRegistry.h>
#endif

namespace synthaxes::tidy
{
    class SynthaxesModule : public clang::tidy::ClangTidyModule
    {
    public:
        void addCheckFactories(clang::tidy::ClangTidyCheckFactories& factories) override
        {
            factories.registerCheck<ThisUsageCheck>("synthaxes-explicit-this");
            factories.registerCheck<AccessorNamingCheck>("synthaxes-accessor-naming");
            factories.registerCheck<DecimalInSCheck>("synthaxes-decimal-in-s");
        }
    };

    // Register the module so clang-tidy discovers its checks.
    static clang::tidy::ClangTidyModuleRegistry::Add<SynthaxesModule> kSynthaxesModule("synthaxes-module", "SynthAxes in-house checks enforcing coding rules.");

} // namespace synthaxes::tidy

// When statically linked into clang-tidy, the linker discards a translation unit nothing
// references, which would drop the registration above. clang-tidy's ClangTidyForceLinking.h
// references one anchor symbol per module to defeat that; this is ours.
namespace synthaxes::tidy
{
    // NOLINTNEXTLINE(readability-identifier-naming) -- matches clang-tidy's anchor convention.
    volatile int SynthaxesModuleAnchorSource = 0;
} // namespace synthaxes::tidy

# Windows build specifics for synthaxes-clang-tidy.

# A prebuilt LLVM distribution bakes the absolute DIA SDK path of the VS edition it was built on
# (often Enterprise) into LLVMDebugInfoPDB. On a machine with a different edition that path does not
# exist and the link fails. Redirect it to this machine's DIA SDK (VSINSTALLDIR is set inside a VS
# developer environment).
if(MSVC AND DEFINED ENV{VSINSTALLDIR} AND TARGET LLVMDebugInfoPDB)
    file(TO_CMAKE_PATH "$ENV{VSINSTALLDIR}DIA SDK/lib/amd64/diaguids.lib" _synthaxes_local_dia)
    if(EXISTS "${_synthaxes_local_dia}")
        get_target_property(_synthaxes_pdb_libs LLVMDebugInfoPDB INTERFACE_LINK_LIBRARIES)
        string(REGEX REPLACE "[^;]*DIA SDK/lib/amd64/diaguids.lib" "${_synthaxes_local_dia}"
               _synthaxes_pdb_libs "${_synthaxes_pdb_libs}")
        set_target_properties(LLVMDebugInfoPDB PROPERTIES
            INTERFACE_LINK_LIBRARIES "${_synthaxes_pdb_libs}")
        message(STATUS "synthaxes-clang-tidy: DIA SDK -> ${_synthaxes_local_dia}")
    endif()
endif()

# Match LLVM's no-RTTI build, or vtable/type_info handling disagrees across the link boundary.
if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(synthaxes-clang-tidy PRIVATE /GR-)
endif()

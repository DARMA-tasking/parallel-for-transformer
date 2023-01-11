
cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(parallel-for-transformer)

find_program(ccache_binary ccache)
if (ccache_binary)
  message(
    STATUS
    "VT: Found ccache binary: ${ccache_binary}; adding launch rule"
  )
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${ccache_binary}")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  add_compile_options(-fcolor-diagnostics)
endif()

set(CMAKE_CXX_STANDARD 14)

set(CHECKPOINT_MEMBER_ANALYZER_LIBRARY parallel-for-transformer)
set(CHECKPOINT_MEMBER_ANALYZER_LIBRARY_NS vt::lib::parallel-for-transformer)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules/")

find_package(Clang REQUIRED)
find_package(LLVM REQUIRED)

# include fmt in the build
add_subdirectory(lib/fmt)

file(
  GLOB
  HEADER_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

file(
  GLOB
  SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
)

# add_executable(
#   transform
#   ${HEADER_FILES} ${SOURCE_FILES}
# )

add_executable(
  transform
  ${HEADER_FILES} ${SOURCE_FILES}
)

install(FILES ${HEADER_FILES} DESTINATION include)

llvm_map_components_to_libnames(llvm_libs support core irreader option ) # clangASTMatchers clangToolingCore clangLex clangParse clangSema clangTooling )

target_include_directories(
  transform PUBLIC ${CLANG_INCLUDE_DIRS}
)

# # set_target_properties(
# #   transform PROPERTIES COMPILE_FLAGS ${LLVM_CXXFLAGS}
# # )

# # set_target_properties(
# #   transform PROPERTIES LINK_FLAGS "${LLVM_LFLAGS} ${LLVM_SYSLIBS} ${LLVM_LIBS}"
# # )

message(STATUS "CLang libs: ${llvm_libs} INCS: ${CLANG_INCLUDE_DIRS}")

# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

target_link_libraries(
  transform PUBLIC ${llvm_libs} ${LLVM_AVAILABLE_LIBS}
)
target_compile_options(transform PUBLIC -fno-rtti)

target_link_options(transform PUBLIC -fno-rtti)

target_link_libraries(
  transform PUBLIC
  clangDriver
  clangCodeGen
  clangSema
  clangAnalysis
  clangRewriteFrontend
  clangRewrite
  clangAST
  clangParse
  clangLex
  clangBasic
  clangARCMigrate
  clangEdit
  clangFrontendTool
  clangSerialization
  clangTooling
  clangStaticAnalyzerCheckers
  clangStaticAnalyzerCore
  clangStaticAnalyzerFrontend
  clang
  clangFrontend
  libclang
  clangASTMatchers clangToolingCore clangLex clangParse clangSema
  
# clang
# clangAnalysis
# clangApplyReplacements
# clangARCMigrate
# clangAST
# clangASTMatchers
# clangBasic
# clangChangeNamespace
# clangCodeGen
# clang-cpp
# clang-cpp
# clangCrossTU
# clangDaemon
# clangDaemonTweaks
# clangDependencyScanning
# clangDirectoryWatcher
# clangDoc
# clangDriver
# clangDynamicASTMatchers
# clangEdit
# clangFormat
# clangFrontend
# clangFrontendTool
# clangHandleCXX
# clangHandleLLVM
# clangIncludeFixer
# clangIncludeFixerPlugin
# clangIndex
# clangLex
# clangMove
# clangParse
# clangQuery
# clangReorderFields
# clangRewrite
# clangRewriteFrontend
# clangSema
# clangSerialization
# clang
# clang
# clangStaticAnalyzerCheckers
# clangStaticAnalyzerCore
# clangStaticAnalyzerFrontend
# clangTidy
# clangTidyAbseilModule
# clangTidyAndroidModule
# clangTidyBoostModule
# clangTidyBugproneModule
# clangTidyCERTModule
# clangTidyCppCoreGuidelinesModule
# clangTidyDarwinModule
# clangTidyFuchsiaModule
# clangTidyGoogleModule
# clangTidyHICPPModule
# clangTidyLinuxKernelModule
# clangTidyLLVMModule
# clangTidyMiscModule
# clangTidyModernizeModule
# clangTidyMPIModule
# clangTidyObjCModule
# clangTidyOpenMPModule
# clangTidyPerformanceModule
# clangTidyPlugin
# clangTidyPortabilityModule
# clangTidyReadabilityModule
# clangTidyUtils
# clangTidyZirconModule
# clangTooling
# clangToolingASTDiff
# clangToolingCore
# clangToolingInclusions
# clangToolingRefactoring
# clangToolingSyntax
# clangTransformer
)

target_compile_definitions(
  transform PUBLIC FMT_HEADER_ONLY=1 FMT_USE_USER_DEFINED_LITERALS=0
)
target_include_directories(
  transform PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/fmt>
  $<INSTALL_INTERFACE:include/fmt>
)



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

add_executable(
  transform
  ${HEADER_FILES} ${SOURCE_FILES}
)

install(FILES ${HEADER_FILES} DESTINATION include)

target_include_directories(
  transform PUBLIC ${CLANG_INCLUDE_DIRS}
)

set_target_properties(
  transform PROPERTIES COMPILE_FLAGS ${LLVM_CXXFLAGS}
)

set_target_properties(
  transform PROPERTIES LINK_FLAGS "${LLVM_LFLAGS} ${LLVM_SYSLIBS} ${LLVM_LIBS}"
)

target_link_libraries(
  transform PUBLIC ${CLANG_LIBS}
)

target_link_libraries(
  transform PUBLIC clangASTMatchers clangToolingCore clangLex clangParse clangSema
)

target_compile_definitions(
  transform PUBLIC FMT_HEADER_ONLY=1 FMT_USE_USER_DEFINED_LITERALS=0
)
target_include_directories(
  transform PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/fmt>
  $<INSTALL_INTERFACE:include/fmt>
)


cmake_minimum_required(VERSION 3.24)

# Keep generated files out of the source tree.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake -S . -B build")
endif ()

project(sketch VERSION 0.1.0 DESCRIPTION "The first compiler for the Doodle programming language" LANGUAGES C CXX)

# CMake owns optimization/debug flags for each configuration.
if (PROJECT_IS_TOP_LEVEL)
    if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
    endif ()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif ()

option(SKETCH_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(SKETCH_SANITIZERS "Enable AddressSanitizer and UBSan (all configurations)" OFF)
option(SKETCH_LTO "Enable checked LTO in optimized configurations" OFF)
option(SKETCH_CLANG_TIDY "Run clang-tidy while compiling" OFF)
option(SKETCH_CCACHE "Use ccache when available" OFF)

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
include(GNUInstallDirs)

# List source files explicitly: add new .cpp files here as the project grows.
set(SKETCH_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/string_literal.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/semanticanalyzer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/codegen.cpp"


)

add_executable(sketch ${SKETCH_SOURCES})

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

target_include_directories(sketch SYSTEM PRIVATE
        ${LLVM_INCLUDE_DIRS}
)

separate_arguments(LLVM_DEFINITIONS_LIST
        NATIVE_COMMAND "${LLVM_DEFINITIONS}"
)

target_compile_options(sketch PRIVATE
        ${LLVM_DEFINITIONS_LIST}
)

llvm_map_components_to_libnames(SKETCH_LLVM_LIBS
        core
        support
)

target_link_libraries(sketch PRIVATE
        ${SKETCH_LLVM_LIBS}
)

set_target_properties(sketch PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

target_include_directories(sketch PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_definitions(sketch PRIVATE SKETCH_VERSION="${PROJECT_VERSION}")

# Check optional warning flags before using them.
function(sketch_warning flag)
    string(MAKE_C_IDENTIFIER "SKETCH_HAS_${flag}" flag_result)
    check_cxx_compiler_flag("${flag}" "${flag_result}")
    if (${flag_result})
        target_compile_options(sketch PRIVATE "${flag}")
    endif ()
endfunction()

if (MSVC)
    target_compile_options(sketch PRIVATE /W4)
    if (SKETCH_WARNINGS_AS_ERRORS)
        target_compile_options(sketch PRIVATE /WX)
    endif ()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    foreach (flag IN ITEMS
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wformat=2
            -Wundef
            -Wwrite-strings
            -Wvla
    )
        sketch_warning("${flag}")
    endforeach ()
    if (SKETCH_WARNINGS_AS_ERRORS)
        target_compile_options(sketch PRIVATE -Werror)
    endif ()
else ()
    message(WARNING "No warning preset for ${CMAKE_CXX_COMPILER_ID}")
    if (SKETCH_WARNINGS_AS_ERRORS)
        message(FATAL_ERROR "Add a warnings-as-errors flag for this compiler first")
    endif ()
endif ()

# Verify both compilation AND linking against the sanitizer runtimes.
if (SKETCH_SANITIZERS)
    if (MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        message(FATAL_ERROR "This sanitizer preset requires GCC or GNU-style Clang")
    endif ()
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address,undefined")
    check_cxx_source_compiles("int main() { return 0; }" SKETCH_HAS_SANITIZERS)
    cmake_pop_check_state()
    if (NOT SKETCH_HAS_SANITIZERS)
        message(FATAL_ERROR "Compiler/linker cannot build with ASan and UBSan")
    endif ()
    target_compile_options(sketch PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(sketch PRIVATE -fsanitize=address,undefined)
endif ()

if (SKETCH_LTO)
    if (SKETCH_SANITIZERS)
        message(FATAL_ERROR "Use separate build directories for LTO and sanitizers")
    endif ()
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES CXX)
    if (NOT ipo_supported)
        message(FATAL_ERROR "Requested LTO is unavailable: ${ipo_error}")
    endif ()
    set_target_properties(sketch PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE YES
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO YES
            INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL YES
    )
endif ()

if (SKETCH_CCACHE)
    find_program(SKETCH_CCACHE_EXECUTABLE NAMES ccache REQUIRED)
    set_property(TARGET sketch PROPERTY CXX_COMPILER_LAUNCHER "${SKETCH_CCACHE_EXECUTABLE}")
endif ()

if (SKETCH_CLANG_TIDY)
    find_program(SKETCH_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
    set_property(TARGET sketch PROPERTY CXX_CLANG_TIDY "${SKETCH_CLANG_TIDY_EXECUTABLE}")
endif ()

# Explicitly requested format targets; building never reformats your code.
# Add your headers to this list once they exist.
set(SKETCH_FORMAT_FILES
    ${SKETCH_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lexer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/parser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/semanticanalyzer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/string_literal.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/codegen.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/token.hpp"
)
find_program(SKETCH_CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if (SKETCH_CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format
            COMMAND "${SKETCH_CLANG_FORMAT_EXECUTABLE}" -i ${SKETCH_FORMAT_FILES}
            COMMENT "Formatting sketch sources"
            VERBATIM
    )
    add_custom_target(format-check
            COMMAND "${SKETCH_CLANG_FORMAT_EXECUTABLE}" --dry-run --Werror ${SKETCH_FORMAT_FILES}
            COMMENT "Checking sketch source formatting"
            VERBATIM
    )
endif ()

install(TARGETS sketch RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

message(STATUS "sketch ${PROJECT_VERSION}: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Warnings-as-errors=${SKETCH_WARNINGS_AS_ERRORS}; sanitizers=${SKETCH_SANITIZERS}; LTO=${SKETCH_LTO}")

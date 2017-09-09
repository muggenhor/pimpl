cmake_minimum_required(VERSION 3.2)

enable_language(CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "[Cc]lang|GNU")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra ${CMAKE_CXX_FLAGS}")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "-DNOMINMAX ${CMAKE_CXX_FLAGS}") # exclude M$ min/max macros
    set(CMAKE_CXX_FLAGS "/wd4996 ${CMAKE_CXX_FLAGS}") # don't warn about use of plain C functions without (non-portable) "_s" suffix
endif()

# Don't warn about [[noreturn]] for older GCC/Clang that only know [[gnu::noreturn]].
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-Wno-error=unknown-attributes" NO_ERROR_UNKNOWN_ATTRIBUTES)
check_cxx_compiler_flag("-Wno-error=attributes" NO_ERROR_ATTRIBUTES)
if(NO_ERROR_UNKNOWN_ATTRIBUTES)
    set(CMAKE_CXX_FLAGS "-Wno-unknown-attributes ${CMAKE_CXX_FLAGS}")
elseif(NO_ERROR_ATTRIBUTES)
    set(CMAKE_CXX_FLAGS "-Wno-attributes ${CMAKE_CXX_FLAGS}")
endif()

if(UNIX AND (
       (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9))
    AND CMAKE_GENERATOR STREQUAL "Ninja")
  # These compilers generate coloured output, but by default only when their output channel is a
  # terminal (TTY/PTY). Ninja however captures all output in a pipe (per-subprocess), disabling
  # coloured compiler diagnostics. We forcefully enable it because Ninja, since 1.0.0
  # (ninja-build/ninja#198) takes care to strip VT100 CSI control sequences from the output if Ninja
  # itself is writing to a pipe instead of a terminal. As a result this should give us the best of
  # both worlds: coloured output when we're running in a terminal, plain output for editors, IDEs,
  # etc.
  set(CMAKE_CXX_FLAGS "-fdiagnostics-color=always ${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)
endif()

# Prepend CMake's auto-detected required C++ flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_INIT} ${CMAKE_CXX_FLAGS}")

###############################################
# Introduction to Hunter
#
# Hunter is a dependency manager for CMake. It automatically downloads, configures and compiles
# libraries that your program depends on.
# Hunter is clever enough to understand the toolchain file you have specified on the command line, and
# build dependencies (including quite demanding ones like boost, openssl etc) with the correct command line arguments.
# Tt then caches the build by target, toolchain and configuration so that you never have to build that
# configuration again.
# see https://github.com/ruslo/hunter
#
# If you don't want to use Hunter then simply define the variable HUNTER_ENABLED=OFF on the CMAKE command line
include(cmake/HunterGate.cmake)
HunterGate(
        URL "https://github.com/ruslo/hunter/archive/v0.19.96.tar.gz"
        SHA1 "8316d0491ee03a918d2e0973aaf5878d9bcfe472"
)

project(impl_ptr CXX)

enable_testing()

if (HUNTER_ENABLED)
    hunter_add_package(Sugar)
    include(${SUGAR_ROOT}/cmake/Sugar)
    include(sugar_files)
    include(sugar_include)
    hunter_add_package(Boost)
    find_package(Boost 1.57 CONFIG REQUIRED)
else ()
    find_package(Boost 1.57 REQUIRED)
endif ()

# Workaround for bug in FindBoost.cmake
if(NOT TARGET Boost::boost AND CMAKE_VERSION VERSION_LESS "3.6")
    add_library(Boost::boost INTERFACE IMPORTED)
    set_target_properties(Boost::boost PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}")
endif()

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>
)
target_compile_features(${PROJECT_NAME}
    INTERFACE
        cxx_alias_templates
        cxx_auto_type
        cxx_constexpr
        cxx_decltype
        cxx_default_function_template_args
        cxx_defaulted_functions
        cxx_deleted_functions
        cxx_explicit_conversions
        cxx_final
        cxx_inheriting_constructors
        cxx_nonstatic_member_init
        cxx_nullptr
        cxx_override
        cxx_right_angle_brackets
        cxx_rvalue_references
        cxx_static_assert
        cxx_template_template_parameters
        cxx_variadic_templates
)
target_link_libraries(${PROJECT_NAME}
    INTERFACE
        Boost::boost
)

if (HUNTER_ENABLED)
    sugar_include(test)
else ()
    add_executable(test_impl_ptr
        test/allocator.hpp
        test/impl.cpp
        test/impl_always_inplace.cpp
        test/impl_copied.cpp
        test/impl_inplace.cpp
        test/impl_poly.cpp
        test/impl_shared.cpp
        test/impl_unique.cpp
        test/main.cpp
        test/test.hpp
    )
    target_link_libraries(test_impl_ptr PRIVATE ${PROJECT_NAME})

    add_test(NAME impl_ptr COMMAND test_impl_ptr)
endif ()

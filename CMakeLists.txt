cmake_minimum_required(VERSION 3.16)
project(subgraph_filter_suite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler version check ────────────────────────────────────────────────────
# Enforce minimums that guarantee C++17 support and a stable ABI.
# Mixing a compiler below these versions with a Boost built by a newer one
# causes GLIBCXX / MSVC CRT linker errors at link time.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "7.0")
        message(FATAL_ERROR
            "GCC 7+ required (found ${CMAKE_CXX_COMPILER_VERSION}). "
            "Using an older GCC against a newer Boost causes libstdc++ ABI mismatches.")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0")
        message(FATAL_ERROR
            "Clang 5+ required (found ${CMAKE_CXX_COMPILER_VERSION}). "
            "Older Clang lacks full C++17 support and may produce ABI-incompatible objects.")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # 19.14 == Visual Studio 2017 15.7, the first MSVC with complete C++17
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.14")
        message(FATAL_ERROR
            "MSVC 19.14+ (VS 2017 15.7+) required (found ${CMAKE_CXX_COMPILER_VERSION}). "
            "Earlier versions lack full C++17 and use an incompatible CRT ABI with Boost.")
    endif()
endif()

# Export compile_commands.json for clang-tidy and IDE tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Boost ─────────────────────────────────────────────────────────────────────
find_package(Boost 1.76 REQUIRED COMPONENTS graph log log_setup json)

# ── Sources ───────────────────────────────────────────────────────────────────
# GLOB_RECURSE descends into all subdirectories, so a single *.cpp pattern
# covers src/graph/, src/preprocessing/enumeration/, src/io/cache/, etc.
# CLI sources are excluded — they live in their own executables below.
# GPU .cu files are excluded here and added conditionally in the CUDA block.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*.cpp")
list(FILTER SOURCES EXCLUDE REGEX ".*/cli/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/gpu/.*")
add_library(subgraph_filter_suite ${SOURCES})

# ── Include paths (library) ───────────────────────────────────────────────────
# Discover all subdirectories under include/ that belong to the library.
# cli/ is excluded — headers there are private to CLI executables.
# gpu/ is excluded — added conditionally in the CUDA block below.
file(GLOB_RECURSE _sgf_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
set(_sgf_lib_include_dirs "")
foreach(_header IN LISTS _sgf_headers)
    get_filename_component(_dir "${_header}" DIRECTORY)
    list(APPEND _sgf_lib_include_dirs "${_dir}")
endforeach()
list(REMOVE_DUPLICATES _sgf_lib_include_dirs)
list(FILTER _sgf_lib_include_dirs EXCLUDE REGEX ".*/include/cli(/.+)?$")
list(FILTER _sgf_lib_include_dirs EXCLUDE REGEX ".*/include/gpu(/.+)?$")

target_include_directories(subgraph_filter_suite PUBLIC $<INSTALL_INTERFACE:include>)
foreach(_dir IN LISTS _sgf_lib_include_dirs)
    target_include_directories(subgraph_filter_suite PUBLIC $<BUILD_INTERFACE:${_dir}>)
endforeach()

target_link_libraries(subgraph_filter_suite PUBLIC
    Boost::log
    Boost::log_setup
    Boost::graph
    Boost::json
)

# Mark Boost headers as system includes so compilers suppress third-party warnings.
# This removes the need for the GCC-only -Wno-maybe-uninitialized workaround.
foreach(_boost_target Boost::graph Boost::log Boost::log_setup Boost::json Boost::program_options)
    if(TARGET ${_boost_target})
        get_target_property(_boost_incs ${_boost_target} INTERFACE_INCLUDE_DIRECTORIES)
        if(_boost_incs)
            set_target_properties(${_boost_target} PROPERTIES
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_boost_incs}")
        endif()
    endif()
endforeach()

# MotifMap.cpp is a large generated file; full DWARF makes debug builds hang.
# Strip the default -g from Debug flags and replace with -g1 project-wide.
string(REPLACE "-g" "-g1" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

# ── Compiler warnings ─────────────────────────────────────────────────────────
# -Wno-maybe-uninitialized: GCC 14 emits false-positive maybe-uninitialized
# warnings inside Boost 1.85 adjacency_list copy_impl / add_edge inlined paths.
# This is a known GCC Bug #105329; the project code is correct.
# clang-tidy uses -Wno-unknown-warning-option (set in .clang-tidy ExtraArgsBefore)
# to silently ignore this GCC-only flag when Clang parses compile_commands.json.
target_compile_options(subgraph_filter_suite PRIVATE
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU>>:-Wall -Wextra -Wpedantic -Werror -Wno-maybe-uninitialized>
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wpedantic -Werror>
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/W4 /WX /EHsc>
)

# ── CUDA (optional GPU acceleration) ─────────────────────────────────────────
# Enable with: cmake -DSGF_ENABLE_CUDA=ON
# Requires the CUDA Toolkit (nvcc). CPU-only build is the default.
option(SGF_ENABLE_CUDA "Enable CUDA GPU acceleration" OFF)

if(SGF_ENABLE_CUDA)
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)

    # ── cuCollections (cuco) — GPU hash map, header-only ──────────────────────
    # Use the bundled source tree instead of an installed package.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cuCollections/include/cuco")
        set(_cccl_root "${CMAKE_CURRENT_SOURCE_DIR}/cuCollections/build/_deps/cccl-src")
        add_library(cuco INTERFACE)
        target_include_directories(cuco INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cuCollections/include>
            $<BUILD_INTERFACE:${_cccl_root}/libcudacxx/include>
            $<BUILD_INTERFACE:${_cccl_root}/thrust>
            $<BUILD_INTERFACE:${_cccl_root}/cub>
            $<INSTALL_INTERFACE:include/cuco>
        )
        install(TARGETS cuco EXPORT SgfTargets)
        install(DIRECTORY cuCollections/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cuco)
    else()
        find_package(cuco REQUIRED)
    endif()

    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    # Keys are uint64_t (8 bytes), supported on all architectures from sm_70+.
    # "native" targets the GPU installed on the build machine (e.g. sm_89 for L40S).
    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
        set(CMAKE_CUDA_ARCHITECTURES native)
    endif()
    set_target_properties(subgraph_filter_suite PROPERTIES CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}")

    file(GLOB_RECURSE SGF_CUDA_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/*.cu")
    target_sources(subgraph_filter_suite PRIVATE ${SGF_CUDA_SOURCES})
    target_link_libraries(subgraph_filter_suite PRIVATE CUDA::cudart cuco)
    # SGF_CUDA_ENABLED propagates to all downstream targets that link this library.
    target_compile_definitions(subgraph_filter_suite PUBLIC SGF_CUDA_ENABLED)
    set_target_properties(subgraph_filter_suite PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
    target_compile_options(subgraph_filter_suite PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr>
    )
    # GPU headers are only meaningful when CUDA is enabled.
    target_include_directories(subgraph_filter_suite PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/gpu>
        $<INSTALL_INTERFACE:include/gpu>
    )
endif()

# ── CLI tools ─────────────────────────────────────────────────────────────────
# Set SGF_BUILD_CLI=OFF at configure time to build only the library.
option(SGF_BUILD_CLI "Build the sgf-graph-enumerator CLI tool" ON)
if(SGF_BUILD_CLI)
    find_package(Boost 1.76 REQUIRED COMPONENTS program_options)
    # CLI-private include directories (not propagated through the library).
    set(_sgf_cli_includes
        "${CMAKE_CURRENT_SOURCE_DIR}/include/cli"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/cli/argument_parsers"
    )
    set(_sgf_cli_warnings
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU>>:-Wall -Wextra -Wpedantic -Werror -Wno-maybe-uninitialized>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wpedantic -Werror>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/W4 /WX /EHsc>
    )

    add_executable(sgf-graph-enumerator
        src/cli/SgfGraphEnumerator.cpp
        src/cli/argument_parsers/SgfGraphEnumeratorArgumentParser.cpp
    )
    target_include_directories(sgf-graph-enumerator PRIVATE ${_sgf_cli_includes})
    target_link_libraries(sgf-graph-enumerator PRIVATE subgraph_filter_suite Boost::program_options)
    target_compile_options(sgf-graph-enumerator PRIVATE ${_sgf_cli_warnings})

    add_executable(sgf-graph-searcher
        src/cli/SgfGraphSearcher.cpp
        src/cli/argument_parsers/SgfGraphSearcherArgumentParser.cpp
    )
    target_include_directories(sgf-graph-searcher PRIVATE ${_sgf_cli_includes})
    target_link_libraries(sgf-graph-searcher PRIVATE subgraph_filter_suite Boost::program_options)
    target_compile_options(sgf-graph-searcher PRIVATE ${_sgf_cli_warnings})

    add_executable(sgf-pattern-finder
        src/cli/SgfPatternFinder.cpp
        src/cli/argument_parsers/SgfPatternFinderArgumentParser.cpp
    )
    target_include_directories(sgf-pattern-finder PRIVATE ${_sgf_cli_includes})
    target_link_libraries(sgf-pattern-finder PRIVATE subgraph_filter_suite Boost::program_options)
    target_compile_options(sgf-pattern-finder PRIVATE ${_sgf_cli_warnings})
endif()

# ── Install / find_package support ───────────────────────────────────────────
# cmake --install build --prefix /path/to/prefix
# Downstream: find_package(sgf REQUIRED); target_link_libraries(app sgf::subgraph_filter_suite)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS subgraph_filter_suite
    EXPORT SgfTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT SgfTargets
    FILE SgfTargets.cmake
    NAMESPACE sgf::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sgf
)
configure_package_config_file(
    cmake/SgfConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SgfConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sgf
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SgfConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sgf
)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)

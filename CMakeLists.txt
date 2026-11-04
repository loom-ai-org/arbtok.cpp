cmake_minimum_required(VERSION 3.16)

project(arbtok VERSION 0.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(_arbtok_top ON)
else()
    set(_arbtok_top OFF)
endif()
option(ARBTOK_BUILD_TESTS "Build the parity tests" ${_arbtok_top})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ── Dependencies ─────────────────────────────────────────────────────────────
# orthography2ipa builds from its pinned submodule. dialects.cpp also reads the
# registry tables that the submodule generates from the Python reference, so a
# separately installed orthography2ipa cannot stand in for it.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/orthography2ipa.cpp/CMakeLists.txt")
    message(FATAL_ERROR "third_party/orthography2ipa.cpp is empty: run `git submodule update --init`")
endif()
# orthography2ipa registers its own tests through include(CTest); they need
# the Python reference beside it, so they stay out of this build.
set(_arbtok_build_testing "${BUILD_TESTING}")
set(BUILD_TESTING OFF)
add_subdirectory(third_party/orthography2ipa.cpp)
set(BUILD_TESTING "${_arbtok_build_testing}")
if(NOT TARGET orthography2ipa::orthography2ipa)
    add_library(orthography2ipa::orthography2ipa ALIAS orthography2ipa)
endif()
include(cmake/onnxruntime.cmake)

# ── Library ──────────────────────────────────────────────────────────────────
# The data files are installed here, and the library looks here unless
# set_data_dir() or ARBTOK_DATA_DIR names another directory.
set(ARBTOK_DATA_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/arbtok")
if(IS_ABSOLUTE "${ARBTOK_DATA_INSTALL_DIR}")
    set(_arbtok_default_data "${ARBTOK_DATA_INSTALL_DIR}")
else()
    set(_arbtok_default_data "${CMAKE_INSTALL_PREFIX}/${ARBTOK_DATA_INSTALL_DIR}")
endif()
# The library looks for its data relative to the installed library, or to the
# executable it is linked into, before the absolute path, so an install made
# with `cmake --install --prefix` into another prefix finds its data.
file(RELATIVE_PATH _arbtok_data_from_lib "${CMAKE_INSTALL_FULL_LIBDIR}" "${CMAKE_INSTALL_FULL_DATADIR}/arbtok")
file(RELATIVE_PATH _arbtok_data_from_bin "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_DATADIR}/arbtok")

add_library(arbtok
    src/unicode.cpp
    src/unicode_norm.cpp
    src/pyrepr.cpp
    src/json.cpp
    src/data.cpp
    src/orthography.cpp
    src/nisba.cpp
    src/waqf.cpp
    src/arabizi.cpp
    src/unicode_symbol2label.cpp
    src/textnorm.cpp
    src/dialects.cpp
    src/stress.cpp
    src/sandhi.cpp
    src/gemination.cpp
    src/lattice.cpp
    src/tokenizer.cpp
    src/ensemble.cpp
    src/diacritize.cpp
)
add_library(arbtok::arbtok ALIAS arbtok)
target_include_directories(arbtok
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
# dialects.cpp reads orthography2ipa's registry tables (src/registry_tables.inc).
target_include_directories(arbtok PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/orthography2ipa.cpp/src)
# dialects.hpp and lattice.hpp hand out orthography2ipa types, so
# orthography2ipa is part of the public interface.
target_link_libraries(arbtok PUBLIC orthography2ipa::orthography2ipa PRIVATE arbtok::onnxruntime ${CMAKE_DL_LIBS})
target_compile_definitions(arbtok PRIVATE ARBTOK_DEFAULT_DATA_DIR="${_arbtok_default_data}"
    ARBTOK_DATA_FROM_LIBDIR="${_arbtok_data_from_lib}" ARBTOK_DATA_FROM_BINDIR="${_arbtok_data_from_bin}")
set_target_properties(arbtok PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(arbtok PRIVATE -Wall -Wextra -Wpedantic)
    # Python rounds each product and each sum. A fused multiply-add rounds once,
    # so a lattice cost differs in the last bit on CPUs with FMA.
    target_compile_options(arbtok PRIVATE -ffp-contract=off)
endif()

install(TARGETS arbtok EXPORT arbtokTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# data/ holds the bundled model under models/.
install(DIRECTORY data/ DESTINATION ${ARBTOK_DATA_INSTALL_DIR})
arbtok_install_onnxruntime()
install(EXPORT arbtokTargets
    NAMESPACE arbtok::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arbtok)
configure_package_config_file(cmake/arbtokConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/arbtokConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arbtok)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/arbtokConfigVersion.cmake
    COMPATIBILITY SameMinorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/arbtokConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/arbtokConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arbtok)

# ── Tests ────────────────────────────────────────────────────────────────────
if(ARBTOK_BUILD_TESTS)
    enable_testing()
    find_package(ZLIB REQUIRED)
    find_package(Threads REQUIRED)

    add_executable(arbtok_parity
        tests/parity.cpp
        tests/parity/constants.cpp
        tests/parity/orthography.cpp
        tests/parity/tashkeel.cpp
        tests/parity/nisba.cpp
        tests/parity/waqf.cpp
        tests/parity/arabizi.cpp
        tests/parity/unicode_symbol2label.cpp
        tests/parity/textnorm.cpp
        tests/parity/dialects.cpp
        tests/parity/stress.cpp
        tests/parity/sandhi.cpp
        tests/parity/lattice.cpp
        tests/parity/tokenizer.cpp
        tests/parity/version.cpp
    )
    target_include_directories(arbtok_parity PRIVATE tests)
    # The headers under src/ expose the private functions the goldens call.
    target_include_directories(arbtok_parity PRIVATE src)
    target_compile_definitions(arbtok_parity PRIVATE ARBTOK_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    target_link_libraries(arbtok_parity PRIVATE arbtok orthography2ipa::orthography2ipa ZLIB::ZLIB Threads::Threads)
    # One parity test per module, so that ctest -j runs the modules in parallel
    # and the timeout applies to each module.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS tests/parity/modules.hpp)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/modules.hpp _parity_header)
    string(REGEX MATCHALL "X\\([a-z0-9_]+\\)" _parity_modules "${_parity_header}")
    list(TRANSFORM _parity_modules REPLACE "X\\(([a-z0-9_]+)\\)" "\\1")
    # The dialects records resolve tags that orthography2ipa matches against
    # its whole registry, so eight shard tests share them.
    # A stride above 1 checks only every n-th record, for the slow sanitizer
    # builds; the dialects records then run as one test at eight times the stride.
    set(ARBTOK_PARITY_STRIDE 1 CACHE STRING "Check every n-th golden record")
    set(_divergences --divergences ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/expected_divergences.tsv)
    set(_parity_tests)
    foreach(_module IN LISTS _parity_modules)
        if(_module STREQUAL "dialects")
            continue()
        endif()
        add_test(NAME parity_${_module} COMMAND arbtok_parity ${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens
                 ${_divergences} --stride ${ARBTOK_PARITY_STRIDE} ${_module})
        list(APPEND _parity_tests parity_${_module})
    endforeach()
    if(ARBTOK_PARITY_STRIDE GREATER 1)
        math(EXPR _dialects_stride "${ARBTOK_PARITY_STRIDE} * 8")
        add_test(NAME parity_dialects COMMAND arbtok_parity ${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens
                 ${_divergences} --stride ${_dialects_stride} dialects)
        list(APPEND _parity_tests parity_dialects)
    else()
        foreach(_k RANGE 7)
            add_test(NAME parity_dialects_${_k} COMMAND arbtok_parity ${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens
                     ${_divergences} --shard ${_k}/8 dialects)
            list(APPEND _parity_tests parity_dialects_${_k})
        endforeach()
    endif()

    foreach(_t ensemble_parity ensemble_api)
        add_executable(${_t} tests/${_t}.cpp)
        target_include_directories(${_t} PRIVATE src)
        target_link_libraries(${_t} PRIVATE arbtok ZLIB::ZLIB)
    endforeach()
    add_test(NAME ensemble_api COMMAND ensemble_api)
    add_test(NAME ensemble_unicode_parity
        COMMAND ensemble_parity ${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens/ensemble_unicode.jsonl.gz)
    add_test(NAME ensemble_parity
        COMMAND ensemble_parity ${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens/ensemble.jsonl.gz)
    set_tests_properties(ensemble_parity PROPERTIES TIMEOUT 1800)
    list(APPEND _parity_tests ensemble_api ensemble_unicode_parity ensemble_parity)

    foreach(_test IN LISTS _parity_tests)
        set_tests_properties(${_test} PROPERTIES ENVIRONMENT "ARBTOK_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data")
    endforeach()
endif()

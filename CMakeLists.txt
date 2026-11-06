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

# The test registry is enabled before src/numbers, so that its parity tests
# are registered with the others. A stride above 1 checks only every n-th
# golden record, for the slow sanitizer builds.
if(ARBTOK_BUILD_TESTS)
    enable_testing()
    set(ARBTOK_PARITY_STRIDE 1 CACHE STRING "Check every n-th golden record")
endif()

# The Arabic parts of ovos-number-parser, ovos-date-parser and unicode-rbnf
# that textnorm and util call. Their tables are compiled into the library.
add_subdirectory(src/numbers)

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
# executable it is linked into, then in the data directory of the package that
# find_package(arbtok) found, before the absolute path. So an install made with
# `cmake --install --prefix` into another prefix finds its data, also from a
# program outside that prefix.
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
    src/textnorm_detail.cpp
    src/textnorm_asr.cpp
    src/textnorm_tts.cpp
    src/pyre.cpp
    src/util.cpp
    src/dialects.cpp
    src/stress.cpp
    src/sandhi.cpp
    src/gemination.cpp
    src/lattice.cpp
    src/tokenizer.cpp
    src/ensemble.cpp
    src/diacritize.cpp
    src/tashkeel.cpp
    src/pyio.cpp
    src/fetch.cpp
    src/lexicon.cpp
    src/dialect_lexicon.cpp
    src/donor_lexicon.cpp
    src/fusion.cpp
    src/translit.cpp
    src/plugin.cpp
    src/o2i_plugins.cpp
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
target_link_libraries(arbtok PUBLIC orthography2ipa::orthography2ipa arbtok::numbers PRIVATE arbtok::onnxruntime ${CMAKE_DL_LIBS})
target_compile_definitions(arbtok PRIVATE ARBTOK_DEFAULT_DATA_DIR="${_arbtok_default_data}"
    ARBTOK_DATA_FROM_LIBDIR="${_arbtok_data_from_lib}" ARBTOK_DATA_FROM_BINDIR="${_arbtok_data_from_bin}")
set_target_properties(arbtok PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(arbtok PRIVATE -Wall -Wextra -Wpedantic)
    # Python rounds each product and each sum. A fused multiply-add rounds once,
    # so a lattice cost differs in the last bit on CPUs with FMA. The rescorers
    # and the lattice costs of orthography2ipa follow the same rule.
    target_compile_options(arbtok PRIVATE -ffp-contract=off)
    target_compile_options(orthography2ipa PRIVATE -ffp-contract=off)
endif()

install(TARGETS arbtok arbtok_numbers EXPORT arbtokTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# data/ holds the bundled model under models/.
install(DIRECTORY data/ DESTINATION ${ARBTOK_DATA_INSTALL_DIR} PATTERN numbers EXCLUDE)
arbtok_install_onnxruntime()
install(EXPORT arbtokTargets
    NAMESPACE arbtok::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arbtok)
configure_package_config_file(cmake/arbtokConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/arbtokConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arbtok
    PATH_VARS ARBTOK_DATA_INSTALL_DIR)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/arbtokConfigVersion.cmake
    COMPATIBILITY SameMinorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/arbtokConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/arbtokConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arbtok)

# ── Tests ────────────────────────────────────────────────────────────────────
if(ARBTOK_BUILD_TESTS)
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
        tests/parity/util.cpp
        tests/parity/pyre.cpp
        tests/parity/dialects.cpp
        tests/parity/stress.cpp
        tests/parity/sandhi.cpp
        tests/parity/lattice.cpp
        tests/parity/tokenizer.cpp
        tests/parity/version.cpp
        tests/parity/lexicon.cpp
        tests/parity/dialect_lexicon.cpp
        tests/parity/donor_lexicon.cpp
        tests/parity/diacritize.cpp
        tests/parity/fusion.cpp
        tests/parity/translit.cpp
        tests/parity/plugin.cpp
        tests/parity/o2i_plugins.cpp
        tests/parity/phoonnx.cpp
    )
    target_include_directories(arbtok_parity PRIVATE tests)
    # The headers under src/ expose the private functions the goldens call.
    target_include_directories(arbtok_parity PRIVATE src)
    target_compile_definitions(arbtok_parity PRIVATE ARBTOK_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
        ARBTOK_GOLDENS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens")
    target_link_libraries(arbtok_parity PRIVATE arbtok orthography2ipa::orthography2ipa ZLIB::ZLIB Threads::Threads)
    # One parity test per module, so that ctest -j runs the modules in parallel
    # and the timeout applies to each module.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS tests/parity/modules.hpp)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/modules.hpp _parity_header)
    string(REGEX MATCHALL "X\\([a-z0-9_]+\\)" _parity_modules "${_parity_header}")
    list(TRANSFORM _parity_modules REPLACE "X\\(([a-z0-9_]+)\\)" "\\1")
    # The dialects records resolve tags that orthography2ipa matches against
    # its whole registry, and the diacritize records run the model, so each of
    # these modules runs as eight shard tests.
    # A stride above 1 checks only every n-th record, for the slow sanitizer
    # builds; a sharded module then runs as one test at eight times the stride.
    # The fusion records run in full and in one test whatever the stride: a
    # text with no word leaves last_nbest as the record before it set it.
    # The goldens of the model hold the output of one CPU class. On a CPU of
    # another class, tools/dump_model_goldens.sh writes a directory that holds
    # them dumped on this CPU, and this variable names it.
    set(ARBTOK_GOLDENS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens CACHE PATH "The directory of the golden files")
    set(_divergences --divergences ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/expected_divergences.tsv)
    set(_sharded dialects diacritize)
    set(_parity_tests)
    foreach(_module IN LISTS _parity_modules)
        if(_module STREQUAL "fusion")
            add_test(NAME parity_fusion COMMAND arbtok_parity ${ARBTOK_GOLDENS_DIR}
                     ${_divergences} fusion)
            list(APPEND _parity_tests parity_fusion)
        elseif(NOT _module IN_LIST _sharded)
            add_test(NAME parity_${_module} COMMAND arbtok_parity ${ARBTOK_GOLDENS_DIR}
                     ${_divergences} --stride ${ARBTOK_PARITY_STRIDE} ${_module})
            list(APPEND _parity_tests parity_${_module})
        elseif(ARBTOK_PARITY_STRIDE GREATER 1)
            math(EXPR _shard_stride "${ARBTOK_PARITY_STRIDE} * 8")
            add_test(NAME parity_${_module} COMMAND arbtok_parity ${ARBTOK_GOLDENS_DIR}
                     ${_divergences} --stride ${_shard_stride} ${_module})
            list(APPEND _parity_tests parity_${_module})
        else()
            foreach(_k RANGE 7)
                add_test(NAME parity_${_module}_${_k} COMMAND arbtok_parity ${ARBTOK_GOLDENS_DIR}
                         ${_divergences} --shard ${_k}/8 ${_module})
                list(APPEND _parity_tests parity_${_module}_${_k})
            endforeach()
        endif()
    endforeach()
    set_tests_properties(parity_fusion PROPERTIES TIMEOUT 3600)
    # A golden whose header gives no records fails: an empty dump is not a pass.
    add_test(NAME parity_zero_count COMMAND arbtok_parity ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/zero_count translit)
    set_tests_properties(parity_zero_count PROPERTIES PASS_REGULAR_EXPRESSION "golden header gives no records")

    foreach(_t ensemble_parity ensemble_api)
        add_executable(${_t} tests/${_t}.cpp)
        target_include_directories(${_t} PRIVATE src)
        target_link_libraries(${_t} PRIVATE arbtok ZLIB::ZLIB)
    endforeach()
    add_test(NAME ensemble_api COMMAND ensemble_api)
    add_test(NAME ensemble_unicode_parity
        COMMAND ensemble_parity ${ARBTOK_GOLDENS_DIR}/ensemble_unicode.jsonl.gz)
    add_test(NAME ensemble_parity
        COMMAND ensemble_parity ${ARBTOK_GOLDENS_DIR}/ensemble.jsonl.gz)
    set_tests_properties(ensemble_parity PROPERTIES TIMEOUT 1800)
    list(APPEND _parity_tests ensemble_api ensemble_unicode_parity ensemble_parity)

    add_executable(arbtok_exceptions tests/exceptions.cpp)
    target_link_libraries(arbtok_exceptions PRIVATE arbtok)
    add_test(NAME exceptions COMMAND arbtok_exceptions)
    list(APPEND _parity_tests exceptions)

    # The lexicon, diacritize and fusion records read the default stem
    # lexicon from the Hugging Face cache. Offline, a missing file fails the
    # test instead of reaching the network.
    foreach(_test IN LISTS _parity_tests)
        set_tests_properties(${_test} PROPERTIES ENVIRONMENT
            "ARBTOK_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data;HF_HUB_OFFLINE=1")
    endforeach()
endif()

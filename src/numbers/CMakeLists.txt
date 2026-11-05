# arbtok_numbers: the parts of ovos-number-parser, ovos-date-parser and
# unicode-rbnf that arbtok calls, for Arabic. The reference tables of
# data/numbers are compiled into the library.

set(_numbers_root "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(_numbers_data
    "${_numbers_root}/data/numbers/numbers_ar.json"
    "${_numbers_root}/data/numbers/dates_ar.json"
    "${_numbers_root}/data/numbers/date_time_ar.json"
    "${_numbers_root}/data/numbers/holidays_ar.json"
    "${_numbers_root}/data/numbers/rbnf_languages.json")
set(_numbers_embed "${CMAKE_CURRENT_BINARY_DIR}/data_embed.cpp")
add_custom_command(
    OUTPUT "${_numbers_embed}"
    COMMAND ${CMAKE_COMMAND} "-DOUT=${_numbers_embed}" "-DFILES=${_numbers_data}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake"
    DEPENDS ${_numbers_data} "${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake"
    COMMENT "Embedding data/numbers"
    VERBATIM)

add_library(arbtok_numbers STATIC
    text.cpp
    pynum.cpp
    json.cpp
    tables.cpp
    numbers_ar.cpp
    ovos_number_parser.cpp
    dates_ar.cpp
    unicode_rbnf.cpp
    "${_numbers_embed}")
add_library(arbtok::numbers ALIAS arbtok_numbers)
target_include_directories(arbtok_numbers
    PUBLIC $<BUILD_INTERFACE:${_numbers_root}/include> $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(arbtok_numbers PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(arbtok_numbers PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
endif()
set_target_properties(arbtok_numbers PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(ARBTOK_BUILD_TESTS)
    find_package(ZLIB REQUIRED)
    add_executable(numbers_parity "${_numbers_root}/tests/numbers_parity.cpp")
    target_include_directories(numbers_parity PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(numbers_parity PRIVATE arbtok_numbers ZLIB::ZLIB)
    add_test(NAME numbers_ar_parity
             COMMAND numbers_parity --stride ${ARBTOK_PARITY_STRIDE}
                     "${_numbers_root}/tests/goldens/numbers_ar.jsonl.gz")
    add_test(NAME numbers_dates_ar_parity
             COMMAND numbers_parity --stride ${ARBTOK_PARITY_STRIDE}
                     "${_numbers_root}/tests/goldens/numbers_dates_ar.jsonl.gz")
    add_test(NAME numbers_dispatch_parity
             COMMAND numbers_parity --stride ${ARBTOK_PARITY_STRIDE}
                     "${_numbers_root}/tests/goldens/numbers_dispatch.jsonl.gz")
endif()

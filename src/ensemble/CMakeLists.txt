# arbtok_ensemble: arbtok._ensemble on onnxruntime, and the skeleton guards of
# arbtok.diacritize.

include(${PROJECT_SOURCE_DIR}/cmake/onnxruntime.cmake)

set(_arbtok_root "${PROJECT_SOURCE_DIR}")

add_library(arbtok_ensemble
    ensemble.cpp
    diacritize_guards.cpp
    unicode.cpp
)
add_library(arbtok::ensemble ALIAS arbtok_ensemble)
target_compile_features(arbtok_ensemble PUBLIC cxx_std_17)
target_include_directories(arbtok_ensemble
    PUBLIC ${_arbtok_root}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(arbtok_ensemble PRIVATE arbtok::onnxruntime)
target_compile_definitions(arbtok_ensemble PRIVATE
    ARBTOK_ENSEMBLE_SOURCE_DATA_DIR="${_arbtok_root}/data")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(arbtok_ensemble PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    find_package(ZLIB REQUIRED)
    foreach(_t ensemble_parity ensemble_api)
        add_executable(${_t} ${_arbtok_root}/tests/${_t}.cpp)
        target_link_libraries(${_t} PRIVATE arbtok_ensemble ZLIB::ZLIB)
        target_include_directories(${_t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${_t} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()
    add_test(NAME ensemble_api COMMAND ensemble_api)
    add_test(NAME ensemble_unicode_parity
        COMMAND ensemble_parity ${_arbtok_root}/tests/goldens/ensemble_unicode.jsonl.gz)
    add_test(NAME ensemble_parity
        COMMAND ensemble_parity ${_arbtok_root}/tests/goldens/ensemble.jsonl.gz)
    set_tests_properties(ensemble_parity PROPERTIES TIMEOUT 1800)
endif()

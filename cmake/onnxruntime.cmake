# onnxruntime for the ensemble: the target arbtok::onnxruntime.
#
# By default the official linux-x64 release tarball is fetched at a pinned
# URL and SHA-256. Its version is the onnxruntime of the Python reference, so
# both run the same kernels. -DARBTOK_ONNXRUNTIME_ROOT=<dir> uses an
# installed onnxruntime instead: <dir>/include or <dir>/include/onnxruntime
# must hold onnxruntime_cxx_api.h, and <dir>/lib or <dir>/lib64 the library.

set(ARBTOK_ONNXRUNTIME_VERSION 1.31.1)
set(ARBTOK_ONNXRUNTIME_URL
    "https://github.com/microsoft/onnxruntime/releases/download/v${ARBTOK_ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-${ARBTOK_ONNXRUNTIME_VERSION}.tgz")
set(ARBTOK_ONNXRUNTIME_SHA256 c849e36a1ef3e22d4ff217b9d1ce4a3cf5b9467b975610913ae959ad5b16bdc9)
set(ARBTOK_ONNXRUNTIME_ROOT "" CACHE PATH "Use the onnxruntime installed under this directory")

if(NOT TARGET arbtok::onnxruntime)
    if(ARBTOK_ONNXRUNTIME_ROOT)
        set(_ort_root "${ARBTOK_ONNXRUNTIME_ROOT}")
    else()
        if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$"))
            message(FATAL_ERROR "The pinned onnxruntime tarball is for linux-x64. Set ARBTOK_ONNXRUNTIME_ROOT.")
        endif()
        include(FetchContent)
        FetchContent_Declare(arbtok_onnxruntime
            URL "${ARBTOK_ONNXRUNTIME_URL}"
            URL_HASH SHA256=${ARBTOK_ONNXRUNTIME_SHA256}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        # The tarball has no CMakeLists.txt, so this only downloads and unpacks it.
        FetchContent_MakeAvailable(arbtok_onnxruntime)
        set(_ort_root "${arbtok_onnxruntime_SOURCE_DIR}")
    endif()

    find_path(ARBTOK_ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
        PATHS "${_ort_root}/include" "${_ort_root}/include/onnxruntime"
        NO_DEFAULT_PATH REQUIRED)
    find_library(ARBTOK_ONNXRUNTIME_LIBRARY onnxruntime
        PATHS "${_ort_root}/lib" "${_ort_root}/lib64"
        NO_DEFAULT_PATH REQUIRED)

    add_library(arbtok::onnxruntime SHARED IMPORTED GLOBAL)
    set_target_properties(arbtok::onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ARBTOK_ONNXRUNTIME_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ARBTOK_ONNXRUNTIME_INCLUDE_DIR}")
    message(STATUS "arbtok: onnxruntime ${ARBTOK_ONNXRUNTIME_LIBRARY}")
endif()

# The installed package must name an onnxruntime for arbtok::onnxruntime. The
# fetched tarball is installed beside arbtok; an ARBTOK_ONNXRUNTIME_ROOT
# library is referenced where it is.
function(arbtok_install_onnxruntime)
    get_filename_component(_ort_name "${ARBTOK_ONNXRUNTIME_LIBRARY}" NAME)
    if(ARBTOK_ONNXRUNTIME_ROOT)
        set(ARBTOK_ONNXRUNTIME_CONFIG_LOCATION "${ARBTOK_ONNXRUNTIME_LIBRARY}" PARENT_SCOPE)
    else()
        get_filename_component(_ort_dir "${ARBTOK_ONNXRUNTIME_LIBRARY}" DIRECTORY)
        file(GLOB _ort_files "${_ort_dir}/libonnxruntime.so*")
        install(FILES ${_ort_files} DESTINATION ${CMAKE_INSTALL_LIBDIR})
        set(ARBTOK_ONNXRUNTIME_CONFIG_LOCATION "\${PACKAGE_PREFIX_DIR}/${CMAKE_INSTALL_LIBDIR}/${_ort_name}" PARENT_SCOPE)
    endif()
endfunction()

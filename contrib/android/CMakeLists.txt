cmake_minimum_required(VERSION 3.24)
project(FluidSynthAndroidSuperbuild NONE)

include(ExternalProject)

set(ANDROID_NDK "$ENV{ANDROID_NDK_HOME}" CACHE PATH "Android NDK directory")
set(ANDROID_ABI "arm64-v8a" CACHE STRING "Android ABI")
set(ANDROID_PLATFORM "24" CACHE STRING "Minimum Android API level")
set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build configuration")
set(ANDROID_BUILD_JOBS "2" CACHE STRING "Parallel jobs per child build")
option(ANDROID_BUILD_DEPENDENCIES "Build dependencies instead of using a restored prefix" ON)
set(ANDROID_PREFIX "${CMAKE_BINARY_DIR}/prefix")
get_filename_component(FLUIDSYNTH_SOURCE_DIR "../.." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")

if(NOT EXISTS "${ANDROID_NDK}/build/cmake/android.toolchain.cmake")
    message(FATAL_ERROR "Set ANDROID_NDK to an installed NDK directory")
endif()
if(NOT ANDROID_ABI MATCHES "^(armeabi-v7a|arm64-v8a|x86|x86_64)$")
    message(FATAL_ERROR "Unsupported Android ABI: ${ANDROID_ABI}")
endif()
if(ANDROID_PLATFORM LESS 24)
    message(FATAL_ERROR "Android API 24 or newer is required by FLAC")
endif()

# Pass target search paths to every child; pkg-config must not use host libraries.
set(android_cmake_args
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${ANDROID_NDK}/build/cmake/android.toolchain.cmake
    -DANDROID_ABI:STRING=${ANDROID_ABI}
    -DANDROID_PLATFORM:STRING=android-${ANDROID_PLATFORM}
    -DANDROID_STL:STRING=c++_shared
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${ANDROID_PREFIX}
    -DCMAKE_INSTALL_LIBDIR:STRING=lib
    -DCMAKE_PREFIX_PATH:PATH=${ANDROID_PREFIX}
    -DCMAKE_FIND_ROOT_PATH:PATH=${ANDROID_PREFIX}
    -DCMAKE_SHARED_LINKER_FLAGS:STRING=-Wl,-z,max-page-size=16384
    -DCMAKE_EXE_LINKER_FLAGS:STRING=-Wl,-z,max-page-size=16384
    -DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5
    -DBUILD_SHARED_LIBS:BOOL=ON
    -DBUILD_TESTING:BOOL=OFF)
set(android_environment ${CMAKE_COMMAND} -E env
    "PKG_CONFIG_PATH=" "PKG_CONFIG_LIBDIR=${ANDROID_PREFIX}/lib/pkgconfig")

function(android_dependency name url sha256)
    cmake_parse_arguments(DEP "" "" "DEPENDS;ARGS;INSTALL_COMMAND" ${ARGN})
    if(NOT DEP_INSTALL_COMMAND)
        set(DEP_INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>)
    endif()
    ExternalProject_Add(${name}
        URL "${url}" URL_HASH "SHA256=${sha256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/downloads/${name}"
        BINARY_DIR "${CMAKE_BINARY_DIR}/build/${name}"
        CONFIGURE_COMMAND ${android_environment} ${CMAKE_COMMAND}
            -S <SOURCE_DIR> -B <BINARY_DIR> -G "${CMAKE_GENERATOR}"
            ${android_cmake_args} ${DEP_ARGS}
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --parallel ${ANDROID_BUILD_JOBS}
        INSTALL_COMMAND ${DEP_INSTALL_COMMAND}
        DEPENDS ${DEP_DEPENDS})
endfunction()

include(dependencies.cmake)

# Install Oboe metadata for FluidSynth's pkg-config discovery and consumers.
file(MAKE_DIRECTORY "${ANDROID_PREFIX}/lib/pkgconfig")
configure_file(oboe-1.0.pc.in "${ANDROID_PREFIX}/lib/pkgconfig/oboe-1.0.pc" @ONLY)

ExternalProject_Add(fluidsynth
    SOURCE_DIR "${FLUIDSYNTH_SOURCE_DIR}"
    BINARY_DIR "${CMAKE_BINARY_DIR}/build/fluidsynth"
    DOWNLOAD_COMMAND "" UPDATE_COMMAND ""
    CONFIGURE_COMMAND ${android_environment} ${CMAKE_COMMAND}
        -S <SOURCE_DIR> -B <BINARY_DIR> -G "${CMAKE_GENERATOR}"
        ${android_cmake_args}
        -Denable-opensles=ON -Denable-floats=ON -Denable-oboe=ON
        -Denable-dbus=OFF -Denable-oss=OFF -Denable-openmp=OFF
    BUILD_ALWAYS TRUE
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --parallel ${ANDROID_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target check --parallel ${ANDROID_BUILD_JOBS}
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
    DEPENDS android-dependencies)

ExternalProject_Add(assetloader
    SOURCE_DIR "${FLUIDSYNTH_SOURCE_DIR}/doc/android/fluidsynth-assetloader"
    BINARY_DIR "${CMAKE_BINARY_DIR}/build/assetloader"
    DOWNLOAD_COMMAND "" UPDATE_COMMAND ""
    CONFIGURE_COMMAND ${android_environment} ${CMAKE_COMMAND}
        -S <SOURCE_DIR> -B <BINARY_DIR> -G "${CMAKE_GENERATOR}" ${android_cmake_args}
    BUILD_ALWAYS TRUE
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --parallel ${ANDROID_BUILD_JOBS}
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
    DEPENDS fluidsynth)

add_custom_target(android-package ALL
    COMMAND ${CMAKE_COMMAND}
        -DPREFIX=${ANDROID_PREFIX} -DNDK=${ANDROID_NDK} -DABI=${ANDROID_ABI}
        -DOUTPUT=${CMAKE_BINARY_DIR}/package -P ${CMAKE_CURRENT_LIST_DIR}/package.cmake
    DEPENDS assetloader VERBATIM)

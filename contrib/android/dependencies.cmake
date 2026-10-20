# Keep versions, archive hashes and build order together for local and CI builds.
set(OBOE_VERSION 1.10.0)
set(android_dependency_libraries FLAC oboe ogg opus sndfile vorbis vorbisenc vorbisfile)

if(NOT ANDROID_BUILD_DEPENDENCIES)
    foreach(library IN LISTS android_dependency_libraries)
        if(NOT EXISTS "${ANDROID_PREFIX}/lib/lib${library}.so")
            message(FATAL_ERROR "Restored dependency prefix is missing lib${library}.so")
        endif()
    endforeach()
    add_custom_target(android-dependencies)
    return()
endif()

android_dependency(ogg
    https://github.com/xiph/ogg/releases/download/v1.3.6/libogg-1.3.6.tar.gz
    83e6704730683d004d20e21b8f7f55dcb3383cdf84c0daedf30bde175f774638
    ARGS -DINSTALL_DOCS=OFF)
android_dependency(vorbis
    https://github.com/xiph/vorbis/releases/download/v1.3.7/libvorbis-1.3.7.tar.gz
    0e982409a9c3fc82ee06e08205b1355e5c6aa4c36bca58146ef399621b0ce5ab
    DEPENDS ogg)
# Disable FLAC's x86 assembler, which receives unsupported compiler flags.
android_dependency(flac
    https://codeload.github.com/xiph/flac/tar.gz/refs/tags/1.4.3
    0a4bb82a30609b606650d538a804a7b40205366ce8fc98871b0ecf3fbb0611ee
    DEPENDS ogg
    ARGS -DCMAKE_C_STANDARD=99 -DCMAKE_C_STANDARD_REQUIRED=ON -DWITH_ASM=OFF
        -DBUILD_CXXLIBS=OFF -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF
        -DBUILD_DOCS=OFF -DINSTALL_MANPAGES=OFF)
android_dependency(opus
    https://codeload.github.com/xiph/opus/tar.gz/refs/tags/v1.5.2
    9480e329e989f70d69886ded470c7f8cfe6c0667cc4196d4837ac9e668fb7404
    ARGS -DCMAKE_C_STANDARD=99 -DCMAKE_C_STANDARD_REQUIRED=ON -DOPUS_MAY_HAVE_NEON=ON)
android_dependency(sndfile
    https://github.com/libsndfile/libsndfile/releases/download/1.2.2/libsndfile-1.2.2.tar.xz
    3799ca9924d3125038880367bf1468e53a1b7e3686a934f098b7e1d286cdb80e
    DEPENDS ogg vorbis flac opus
    ARGS -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DENABLE_MPEG=OFF)
android_dependency(oboe
    https://codeload.github.com/google/oboe/tar.gz/refs/tags/${OBOE_VERSION}
    0e4245f8860c4287040a5d76501c588490bcc9cb57614c486c0c201a5dde3e9f
    # Oboe hardcodes lib/<ABI> rather than using CMAKE_INSTALL_LIBDIR.
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different <BINARY_DIR>/liboboe.so ${ANDROID_PREFIX}/lib/liboboe.so)

add_custom_target(android-dependencies DEPENDS ogg vorbis flac opus sndfile oboe)

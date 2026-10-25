cmake_minimum_required(VERSION 3.6.0)

project ( fluidsynth-assetloader C )

find_package(FluidSynth CONFIG REQUIRED COMPONENTS shared)

set ( fluidsynth-assetloader_sources fluid_androidasset.c )

add_library ( fluidsynth-assetloader SHARED ${fluidsynth-assetloader_sources} )

target_compile_options ( fluidsynth-assetloader
                        PRIVATE -v
                        PRIVATE -Wall
                        PRIVATE "$<$<CONFIG:DEBUG>:-Werror>") # Only include -Werror when building debug config

target_link_libraries ( fluidsynth-assetloader PRIVATE FluidSynth::libfluidsynth log android )

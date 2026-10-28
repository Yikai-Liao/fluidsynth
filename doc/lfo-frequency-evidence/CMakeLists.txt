cmake_minimum_required(VERSION 3.13)
project(lfo_frequency_evidence LANGUAGES C CXX)

get_filename_component(default_source "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(FLUIDSYNTH_SOURCE_DIR "${default_source}" CACHE PATH "FluidSynth checkout to measure")
add_subdirectory("${FLUIDSYNTH_SOURCE_DIR}" synth EXCLUDE_FROM_ALL)

add_executable(lfo_trace trace_lfo.c)
target_link_libraries(lfo_trace PRIVATE libfluidsynth-OBJ)
target_include_directories(lfo_trace PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/synth/include"
    "${FLUIDSYNTH_SOURCE_DIR}/include"
    $<TARGET_PROPERTY:libfluidsynth-OBJ,INCLUDE_DIRECTORIES>)

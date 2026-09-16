cmake_minimum_required(VERSION 3.25)
project(mmathen VERSION 0.1.0 LANGUAGES CXX)

# Certification builds use explicit C++20, never c++latest, with extensions
# disabled where supported: prove the stated language contract.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build the test suite (fetches pinned Catch2 v3)" ON)
option(MMATHEN_BUILD_EXAMPLES "Build runnable examples" ON)
option(MMATHEN_SANITIZE
  "Build tests/examples with sanitizers (ASan+UBSan; GCC/Clang only)" OFF)

if(MMATHEN_SANITIZE AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR
    "MMATHEN_SANITIZE is supported on GCC/Clang only. Rationale: the MSVC "
    "AddressSanitizer component is not verifiable in this project's "
    "environments (missing runtime, /RTC conflict); MSVC is covered by "
    "/W4 /WX plus the compile-fail probes, sanitizers run on GCC/Clang.")
endif()

# Warning + FP + sanitizer policy, target-scoped. Nothing here touches
# Catch2 targets or downstream consumers.
function(mmathen_dev_settings target)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${target} PRIVATE /W4 /WX /fp:precise)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror
      -fno-fast-math -ffp-contract=off)
    if(MMATHEN_SANITIZE)
      target_compile_options(${target} PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endfunction()

# Zero third-party dependencies: headers only, standard library only.
add_library(mmathen_core INTERFACE)
target_include_directories(mmathen_core INTERFACE include)

# Reference floating-point execution policy (NUMERICS.md): precise
# contraction behavior, no fast-math. Target-scoped; never global.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # /fp:precise is the default and disables contraction in VS2022; stated
  # explicitly so the policy is recorded, not assumed.
  target_compile_options(mmathen_core INTERFACE /fp:precise)
  set(MMATHEN_FP_POLICY "/fp:precise (contraction off by default in VS2022)")
else()
  target_compile_options(mmathen_core INTERFACE -fno-fast-math -ffp-contract=off)
  set(MMATHEN_FP_POLICY "-fno-fast-math -ffp-contract=off")
endif()

# Recorded with every test result (NUMERICS.md).
configure_file(cmake/numerics.json.in "${CMAKE_BINARY_DIR}/numerics.json" @ONLY)

if(BUILD_TESTING)
  include(FetchContent)

  # Test-only dependency: one pinned Catch2 v3. BUILD_TESTING=OFF bypasses
  # all discovery and fetching. CATCH2_SOURCE_DIR supports offline builds
  # with a supplied local source directory. Network is used only here, never
  # when building the core alone.
  set(CATCH2_SOURCE_DIR "" CACHE PATH
      "Optional local Catch2 source directory for offline builds")
  if(CATCH2_SOURCE_DIR)
    add_subdirectory(${CATCH2_SOURCE_DIR} ${CMAKE_BINARY_DIR}/catch2)
    set(_catch2_extras "${CATCH2_SOURCE_DIR}/extras")
  else()
    # Immutable commit hash for v3.9.1 (CMake recommends a hash over a tag).
    FetchContent_Declare(Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG 644821ce28cb25d7992a4d0375b1d83214392592
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
    set(_catch2_extras "${catch2_SOURCE_DIR}/extras")
  endif()

  add_executable(mmathen_tests
    tests/test_math.cpp
    tests/test_units.cpp
    tests/test_time.cpp
    tests/test_signal.cpp
    tests/test_render.cpp
    tests/test_tuning.cpp
    tests/test_pitch.cpp
    tests/test_structures.cpp
    tests/test_timbre.cpp
    tests/test_musical_time.cpp
    tests/test_waves.cpp
    tests/test_d1_consonance.cpp
    tests/test_d2_waves.cpp
    tests/test_pcset.cpp
    tests/test_d4_rhythm.cpp
    tests/test_commas.cpp
    tests/test_mos.cpp
    tests/test_fm.cpp
    tests/test_bandlimited.cpp
    tests/test_spectral.cpp
    tests/test_biquad.cpp
    tests/test_wavetable.cpp
    tests/test_pm.cpp
    tests/test_segment_envelope.cpp
    tests/test_wave1d.cpp
    tests/test_loss.cpp
    tests/test_stiffness.cpp
    tests/headers/th_result.cpp
    tests/headers/th_scalar.cpp
    tests/headers/th_constants.cpp
    tests/headers/th_comparison.cpp
    tests/headers/th_dimension.cpp
    tests/headers/th_quantity.cpp
    tests/headers/th_physical_units.cpp
    tests/headers/th_music_units.cpp
    tests/headers/th_conversions.cpp
    tests/headers/th_time_function.cpp
    tests/headers/th_time_factories.cpp
    tests/headers/th_signal_signal.cpp
    tests/headers/th_signal_phase.cpp
    tests/headers/th_signal_oscillator.cpp
    tests/headers/th_sink_sink.cpp
    tests/headers/th_sink_wav.cpp
    tests/headers/th_render_offline.cpp
    tests/headers/th_music_tuning.cpp
    tests/headers/th_music_pitch.cpp
    tests/headers/th_music_structures.cpp
    tests/headers/th_music_transforms.cpp
    tests/headers/th_signal_harmonics.cpp
    tests/headers/th_signal_envelope.cpp
    tests/headers/th_time_tempo.cpp
    tests/headers/th_music_meter.cpp
    tests/headers/th_physics_waves.cpp
    tests/headers/th_physics_medium.cpp
    tests/headers/th_signal_inharmonic.cpp
    tests/headers/th_analysis_consonance.cpp
    tests/headers/th_physics_modes.cpp
    tests/headers/th_physics_impedance.cpp
    tests/headers/th_physics_absorption.cpp
    tests/headers/th_music_pcset.cpp
    tests/headers/th_music_forte_table.cpp
    tests/headers/th_music_voiceleading.cpp
    tests/headers/th_music_euclid.cpp
    tests/headers/th_music_commas.cpp
    tests/headers/th_music_temperament.cpp
    tests/headers/th_music_mos.cpp
    tests/headers/th_signal_fm.cpp
    tests/headers/th_signal_bandlimited.cpp
    tests/headers/th_signal_spectral.cpp
    tests/headers/th_dsp_biquad.cpp
    tests/headers/th_signal_wavetable.cpp
    tests/headers/th_signal_pm.cpp
    tests/headers/th_signal_segment_envelope.cpp
    tests/headers/th_physics_wave1d.cpp
    tests/headers/th_physics_fdtd1d.cpp
    tests/headers/th_physics_waveguide1d.cpp
  )
  target_link_libraries(mmathen_tests PRIVATE mmathen_core Catch2::Catch2WithMain)
  mmathen_dev_settings(mmathen_tests)

  include(CTest)
  include("${_catch2_extras}/Catch.cmake")
  catch_discover_tests(mmathen_tests)

  # Isolated compile probes: each builds in its own directory with the same
  # headers and C++20 configuration; diagnostics are preserved and reported
  # when an expectation is violated. Valid control proves the rig works; the
  # invalid cases prove rejection comes from the type contract.
  function(mmathen_compile_probe name expect_pass)
    try_compile(probe_ok "${CMAKE_BINARY_DIR}/probes/${name}"
      SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fail/${name}.cpp"
      CMAKE_FLAGS
        "-DINCLUDE_DIRECTORIES=${CMAKE_CURRENT_SOURCE_DIR}/include"
        "-DCMAKE_CXX_STANDARD=20"
        "-DCMAKE_CXX_EXTENSIONS=OFF"
      OUTPUT_VARIABLE probe_log)
    if(expect_pass AND NOT probe_ok)
      message(FATAL_ERROR
        "compile probe '${name}' should compile but failed:\n${probe_log}")
    elseif(NOT expect_pass AND probe_ok)
      message(FATAL_ERROR
        "compile probe '${name}' should FAIL to compile but passed")
    else()
      message(STATUS "compile probe '${name}': as expected")
    endif()
  endfunction()

  mmathen_compile_probe(valid_control TRUE)
  mmathen_compile_probe(invalid_add_freq_duration FALSE)
  mmathen_compile_probe(invalid_add_freq_angular FALSE)
  mmathen_compile_probe(invalid_add_timepoint_timepoint FALSE)
  mmathen_compile_probe(invalid_mul_freq_duration FALSE)
  mmathen_compile_probe(invalid_scale_timepoint FALSE)
  mmathen_compile_probe(invalid_add_framecount_framecount FALSE)
  mmathen_compile_probe(invalid_add_ratio_ratio FALSE)
  mmathen_compile_probe(invalid_convert_samplerate_frequency FALSE)
  mmathen_compile_probe(invalid_convert_quantity_double FALSE)
endif()

if(MMATHEN_BUILD_EXAMPLES)
  foreach(example render_demo tuning_demo progression_demo timbre_demo
                  tempo_demo air_table stretch_demo modes_demo voice_demo
                  comma_demo dsp_demo wavetable_demo pm_demo
                  solver_demo loss_demo stiff_demo)
    add_executable(${example} examples/${example}.cpp)
    target_link_libraries(${example} PRIVATE mmathen_core)
    mmathen_dev_settings(${example})
  endforeach()
endif()

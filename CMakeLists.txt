cmake_minimum_required(VERSION 3.25)
project(mmathen VERSION 0.1.0 LANGUAGES CXX)

# Certification builds use explicit C++20, never c++latest, with extensions
# disabled where supported: prove the stated language contract.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build the test suite (fetches pinned Catch2 v3)" ON)
option(MMATHEN_BUILD_EXAMPLES "Build runnable examples" ON)
option(MMATHEN_BUILD_VALIDATION "Build the offline real-audio measurement tool" OFF)
option(MMATHEN_COMPARE_TUNERS "Build comparison with the copied f3 tuner engine" OFF)
option(MMATHEN_BUILD_APP
  "Build the desktop spectrum/EQ app (fetches pinned SDL3 + Dear ImGui)" OFF)
option(MMATHEN_SANITIZE
  "Build tests/examples with sanitizers (ASan+UBSan; GCC/Clang only)" OFF)

option(MMATHEN_SANITIZE_THREAD
  "Build tests/examples with ThreadSanitizer (GCC/Clang; not with ASan)" OFF)
if(MMATHEN_SANITIZE_THREAD AND (MMATHEN_SANITIZE OR
   CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"))
  message(FATAL_ERROR
    "MMATHEN_SANITIZE_THREAD needs GCC/Clang and excludes MMATHEN_SANITIZE "
    "(TSan and ASan cannot share a binary).")
endif()

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
        -fsanitize=address,undefined -fno-omit-frame-pointer
        # UBSan otherwise prints a report and continues with exit code 0
        # (verified), which a passing ctest would hide: every finding aborts.
        -fno-sanitize-recover=all)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
    if(MMATHEN_SANITIZE_THREAD)
      # D29: the lock-free hand-offs (SpscRing, TripleBuffer) are checked by
      # a data-race detector, not only by stress tests.
      target_compile_options(${target} PRIVATE -fsanitize=thread
        -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=thread)
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
    tests/test_voice_render.cpp
    tests/test_plucked_string.cpp
    tests/test_contact_voice.cpp
    tests/test_band_voices.cpp
    tests/test_cymbal_vk.cpp
    tests/test_struck_bar.cpp
    tests/test_wind_tone.cpp
    tests/test_soundboard.cpp
    tests/test_cavity_drum.cpp
    tests/test_bore_network.cpp
    tests/test_bessel_horn.cpp
    tests/test_published_defaults.cpp
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
    tests/test_fft.cpp
    tests/test_pitch_error.cpp
    tests/test_materials.cpp
    tests/test_stiff_ends.cpp
    tests/test_tuned_waveguide.cpp
    tests/test_fdtd_box.cpp
    tests/test_fem1d.cpp
    tests/test_coupled_string.cpp
    tests/test_instruments.cpp
    tests/test_hearing.cpp
    tests/test_spectrum.cpp
    tests/test_svg.cpp
    tests/test_equalizer.cpp
    tests/test_filters.cpp
    tests/test_dynamics.cpp
    tests/test_match_eq.cpp
    tests/test_crossfade.cpp
    tests/test_pitch_detect.cpp
    tests/test_time_pitch.cpp
    tests/test_triple_buffer.cpp
    tests/test_bs1770.cpp
    tests/test_wav_writer.cpp
    tests/test_loudness_normalize.cpp
    tests/test_denoise.cpp
    tests/test_pyin.cpp
    tests/test_tsm.cpp
    tests/test_multiband.cpp
    tests/test_acge.cpp
    tests/test_mfcc.cpp
    tests/test_gmm_hmm.cpp
    tests/test_markov_ngram.cpp
    tests/test_chords.cpp
    tests/test_realtime_io.cpp
    tests/test_desktop_tuner.cpp
    tests/test_guitar_pluck.cpp
    tests/test_sleigh_bell.cpp
    tests/test_player_profile.cpp
    tests/test_player_profile_more.cpp
    tests/test_player_builder.cpp
    tests/test_players_file.cpp
    tests/test_player_library.cpp
    tests/test_wind_cracks.cpp
    tests/test_wind_scoops.cpp
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
    tests/headers/th_sink_wav_path.cpp
    tests/headers/th_render_offline.cpp
    tests/headers/th_render_voice.cpp
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
    tests/headers/th_physics_circular_plate.cpp
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
    tests/headers/th_analysis_fft.cpp
    tests/headers/th_physics_materials.cpp
    tests/headers/th_physics_tuned_waveguide.cpp
    tests/headers/th_physics_fdtd_box.cpp
    tests/headers/th_physics_fem1d.cpp
    tests/headers/th_physics_coupled_string.cpp
    tests/headers/th_instruments_plucked_string.cpp
    tests/headers/th_instruments_contact.cpp
    tests/headers/th_instruments_instrument.cpp
    tests/headers/th_instruments_violin.cpp
    tests/headers/th_instruments_viola.cpp
    tests/headers/th_instruments_cello.cpp
    tests/headers/th_instruments_double_bass.cpp
    tests/headers/th_instruments_bow.cpp
    tests/headers/th_instruments_wound.cpp
    tests/headers/th_instruments_body.cpp
    tests/headers/th_instruments_hammer.cpp
    tests/headers/th_instruments_reed.cpp
    tests/headers/th_instruments_clarinet.cpp
    tests/headers/th_instruments_bore_network.cpp
    tests/headers/th_instruments_bessel_horn.cpp
    tests/headers/th_instruments_flute.cpp
    tests/headers/th_instruments_trumpet.cpp
    tests/headers/th_instruments_saxophone.cpp
    tests/headers/th_instruments_trombone.cpp
    tests/headers/th_instruments_guitar.cpp
    tests/headers/th_instruments_electric_guitar.cpp
    tests/headers/th_instruments_piano.cpp
    tests/headers/th_instruments_harp.cpp
    tests/headers/th_instruments_harpsichord.cpp
    tests/headers/th_instruments_drum.cpp
    tests/headers/th_instruments_cymbal.cpp
    tests/headers/th_instruments_cymbal_vk.cpp
    tests/headers/th_instruments_player_profile.cpp
    tests/headers/th_instruments_bowed_note.cpp
    tests/headers/th_hearing_loudness.cpp
    tests/headers/th_hearing_scales.cpp
    tests/headers/th_hearing_masking.cpp
    tests/headers/th_analysis_spectrum.cpp
    tests/headers/th_plot_svg.cpp
    tests/headers/th_dsp_equalizer.cpp
    tests/headers/th_dsp_filters.cpp
    tests/headers/th_dsp_dynamics.cpp
    tests/headers/th_dsp_match_eq.cpp
    tests/headers/th_dsp_crossfade.cpp
    tests/headers/th_dsp_overlap_add.cpp
    tests/headers/th_analysis_pitch.cpp
    tests/headers/th_dsp_time_pitch.cpp
    tests/headers/th_core_triple_buffer.cpp
    tests/headers/th_analysis_bs1770.cpp
    tests/headers/th_sink_wav_writer.cpp
    tests/headers/th_dsp_loudness_normalize.cpp
    tests/headers/th_dsp_denoise.cpp
    tests/headers/th_analysis_pyin.cpp
    tests/headers/th_dsp_tsm.cpp
    tests/headers/th_dsp_multiband.cpp
    tests/headers/th_dsp_linear_phase.cpp
    tests/headers/th_dsp_acge.cpp
    tests/headers/th_analysis_mfcc.cpp
    tests/headers/th_stats_random.cpp
    tests/headers/th_stats_gmm.cpp
    tests/headers/th_stats_hmm.cpp
    tests/headers/th_stats_markov.cpp
    tests/headers/th_stats_ngram.cpp
    tests/headers/th_analysis_chords.cpp
    tests/headers/th_sink_wav_reader.cpp
    tests/headers/th_core_spsc_ring.cpp
    tests/headers/th_analysis_analyzer.cpp
  )
  # std::thread in the SPSC stress test (D22).
  find_package(Threads REQUIRED)
  target_link_libraries(mmathen_tests PRIVATE mmathen_core Catch2::Catch2WithMain
                        Threads::Threads)
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

if(MMATHEN_BUILD_VALIDATION)
  add_executable(native_chord_probe tools/real_audio/native_chord_probe.cpp)
  target_link_libraries(native_chord_probe PRIVATE mmathen_core)
  mmathen_dev_settings(native_chord_probe)
  add_executable(recognition_probe tools/real_audio/recognition_probe.cpp)
  target_link_libraries(recognition_probe PRIVATE mmathen_core)
  mmathen_dev_settings(recognition_probe)
  add_executable(chroma_math_probe tools/real_audio/chroma_math_probe.cpp)
  target_link_libraries(chroma_math_probe PRIVATE mmathen_core)
  mmathen_dev_settings(chroma_math_probe)
  add_executable(accuracy_probe tools/real_audio/accuracy_probe.cpp)
  target_link_libraries(accuracy_probe PRIVATE mmathen_core)
  mmathen_dev_settings(accuracy_probe)
  add_executable(real_audio_probe tools/real_audio/probe.cpp)
  target_link_libraries(real_audio_probe PRIVATE mmathen_core)
  mmathen_dev_settings(real_audio_probe)
  add_executable(reference_data_check tools/real_audio/check_native.cpp)
  target_link_libraries(reference_data_check PRIVATE mmathen_core)
  mmathen_dev_settings(reference_data_check)
endif()

if(MMATHEN_COMPARE_TUNERS)
  add_subdirectory(tools/real_audio/vendor/tuner-engine/cpp tuner-engine SYSTEM)
  add_executable(tuner_probe tools/real_audio/tuner_probe.cpp)
  target_link_libraries(tuner_probe PRIVATE mmathen_core tuner_engine_core)
  mmathen_dev_settings(tuner_probe)
endif()

if(MMATHEN_BUILD_EXAMPLES)
  foreach(example render_demo tuning_demo progression_demo timbre_demo
                  tempo_demo air_table stretch_demo modes_demo voice_demo
                  comma_demo dsp_demo wavetable_demo pm_demo
                  instrument_voice_demo
                  instrument_pluck_demo
                  instrument_contact_demo
                   solver_demo loss_demo stiff_demo pitch_error_demo
                   hearing_demo objects_demo spectrum_demo)
    add_executable(${example} examples/${example}.cpp)
    target_link_libraries(${example} PRIVATE mmathen_core)
    mmathen_dev_settings(${example})
  endforeach()
endif()

# D22 desktop app: an optional consumer of the header-only core. OFF by
# default, so the core, the tests and CI never fetch or build SDL/ImGui.
# Both dependencies are pinned to immutable commits.
if(MMATHEN_BUILD_APP)
  include(FetchContent)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  # The app does not use the X11 XTEST extension; without this SDL's
  # configure fails on Linux hosts lacking libxtst-dev.
  set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
  # SDL 3.4.16 (tag release-3.4.16).
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG fa2c02bb6e21974a89ea9824bc53c9932abe5f9c
    GIT_SHALLOW TRUE)
  # Dear ImGui v1.92.9b (tag v1.92.9b). No CMake project: sources below.
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG f1cc2ae15e53a861a874c3034aae6798fde194ab
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3 imgui)

  add_library(mmathen_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
  # SYSTEM: third-party headers are outside this project's warning policy.
  target_include_directories(mmathen_imgui SYSTEM PUBLIC
    ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(mmathen_imgui PUBLIC SDL3::SDL3-static)

  add_executable(mmathen_app WIN32 app/spectrum_app.cpp)
  target_link_libraries(mmathen_app PRIVATE mmathen_core mmathen_imgui)
  mmathen_dev_settings(mmathen_app)
  if(MMATHEN_BUILD_VALIDATION)
    add_executable(chordino_check tools/real_audio/check_chordino.cpp)
    target_link_libraries(chordino_check PRIVATE mmathen_core SDL3::SDL3-static)
    mmathen_dev_settings(chordino_check)
    add_executable(playback_check tools/real_audio/check_playback.cpp)
    target_link_libraries(playback_check PRIVATE mmathen_core SDL3::SDL3-static)
    mmathen_dev_settings(playback_check)
  endif()
endif()

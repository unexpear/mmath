# Third-party notices

## Vendored in this repository
| Component | Path | Licence | Notes |
|---|---|---|---|
| StandardTune chord shapes / tuner presentation (adapted) | `app/third_party/standardtune/`, `app/chord_shapes.h`, `app/tuner_controls.h` | MIT, © 2026 unexpear and StandardTune contributors | `source-manifest.json` lists original files by SHA-256 |
| react-native-tuner-engine 1.1.3 (C++ core, comparison only) | `tools/real_audio/vendor/tuner-engine/` | MIT, © 2026 denizyesilirmak | https://github.com/denizyesilirmak/react-native-tuner-engine; linked only by the optional comparison tool |

## Fetched at build time (not stored in the repo; pinned in CMakeLists.txt)
| Component | When | Licence |
|---|---|---|
| Catch2 v3 (`644821ce`) | `BUILD_TESTING=ON` | Boost Software License 1.0 |
| SDL3 (`fa2c02bb`) | `MMATHEN_BUILD_APP=ON` | zlib |
| Dear ImGui (`f1cc2ae1`) | `MMATHEN_BUILD_APP=ON` | MIT, © Omar Cornut |

Anyone distributing a built desktop app binary must include the SDL3 and Dear ImGui licence texts (and Catch2's if test binaries are distributed).

## Downloaded on demand by research tools (`tools/real_audio/community-downloads.json`; not redistributed)
| Component | Licence | Use |
|---|---|---|
| Sonic Annotator 1.7 | GPL-2.0-or-later | separate process, comparison only |
| NNLS Chroma / Chordino (win64 build) | GPL-2.0-or-later | separate process, comparison only |
| onnxcrepe tiny/full models (CREPE) | MIT | pitch comparison |

## Data used to build shipped code
| Data | Licence | Where credited |
|---|---|---|
| GuitarSet 1.1.0 (Xi, Bittner, Pauwels, Ye, Bello 2018) | CC BY 4.0 | `app/spectrum_app.cpp`, `docs/41`, `docs/47`; model in `app/guitar_chord_model.h` |
| LibriSpeech test-clean (Panayotov et al. 2015) | CC BY 4.0 | `app/spectrum_app.cpp`, `docs/41` |
| ISO 226:2023 table via `hearingthreshold` | MIT (implementation) | `docs/33` |
| Forte set-class list via Wikipedia "List of set classes" | CC BY-SA (facts) | `include/mmathen/music/forte_table.h` |
| Plomp–Levelt quintic fit as published in Essentia | coefficients reimplemented; no AGPL code included | `include/mmathen/analysis/consonance.h` |

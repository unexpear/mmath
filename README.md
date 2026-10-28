# mmath

Verified header-only C++20 music mathematics engine: units, signals, tuning,
acoustics solvers, physical-model instruments, and a desktop spectrum/EQ app
with a Player Builder. Documentation is in [`docs/`](docs/README.md).

## Build
```
cmake -S . -B build -DMMATHEN_BUILD_APP=ON
cmake --build build --config Release
ctest --test-dir build -C Release
```

## License
MIT — see [LICENSE](LICENSE). Third-party components are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); music sources, reference
recordings, and the few CC BY-SA data files are listed in [SOURCES.md](SOURCES.md).

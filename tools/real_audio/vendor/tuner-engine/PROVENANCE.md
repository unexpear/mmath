# Comparison-only snapshot

Copied read-only from `guitar-tutor/node_modules/react-native-tuner-engine` (the f3 project),
version 1.1.3, on 2026-09-19. Original files are byte-for-byte copies;
`copy-manifest.json` records their SHA-256 hashes. The included MIT license applies.

Upstream: https://github.com/denizyesilirmak/react-native-tuner-engine

The upstream main branch now uses a different detector fusion design. This
snapshot deliberately tests the installed version used by the user's project.
Only the optional comparison executable links this library; the desktop app and
mmathen_core do not acquire this dependency. Mobile bridges are not compiled.

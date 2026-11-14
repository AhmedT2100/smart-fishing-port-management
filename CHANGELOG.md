# Changelog

## 0.2.0 — 2026-11-14

### Changed

- Organize application code under `src/app`, `src/database`, `src/models`, and `src/hardware`.
- Group Designer forms in `ui/`, assets in `resources/`, and independent Arduino sketches in `firmware/`.
- Rename the original personal-name Arduino directory to `firmware/dock_barrier/dock_barrier.ino`, matching its LCD/servo/LED function.
- Update qmake source paths and include directories while retaining the executable name and runtime resource aliases.
- Refresh public-repository setup instructions and add hardware and contribution guidance.

### Added

- Dependency-free Python structural validation for qmake paths, includes, XML resources, and Arduino naming.

### Validation status

Structural and source-preservation checks pass. Qt/Arduino compilation, database, GUI, messaging, and physical hardware remain unverified. This version is a source prerelease, with no binaries.

## 0.1.0 — 2026-11-14

- Initial complete C++/Qt and Arduino source import.
- Environment-based integration configuration and English setup documentation.
- Static project and resource validation; compilation and runtime integrations unverified.

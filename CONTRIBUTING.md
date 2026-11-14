# Contributing

Use focused branches and pull requests with a clear description of the behavior changed and the checks performed. Keep contributor credit factual and agreed with the team. No license has been selected; discuss reuse and licensing with the project owners.

## Code layout

- `src/app`: application startup and main-window orchestration.
- `src/database`: database connection lifecycle.
- `src/models`: existing business modules and SQL access.
- `src/hardware`: Qt serial communication.
- `ui`: Qt Designer forms.
- `resources`: QRC collection, SVG icons, and QSS styles.
- `firmware`: independent Arduino sketches, named for their purpose.
- `docs`: setup details and validation records.
- `scripts`: repository maintenance checks; Python 3.9+.

When adding a source, header, or form, update `gestion_bateaux.pro`. Keep QRC runtime aliases stable when relocating assets. Do not commit generated `ui_*`, `moc_*`, build directories, executables, provider credentials, or local `.env` files.

## Validation

```sh
python3 scripts/check_project.py
git diff --check
```

Then build with a compatible Qt 6 kit and test the workflows affected by the change against your own configured database and hardware. The structural script does not compile code or validate SQL. Record unavailable dependencies and any untested behavior honestly.

## Releases

Update `CHANGELOG.md` and validation documentation, review the exact source commit, and tag that commit. Use prereleases while compilation and integrations remain unverified. GitHub source archives are not executable packages; upload binaries only after building and testing them. Never include local secrets or real user data in release assets.

# Contributing

ChromaDesk is currently maintained as an owner-directed development project.

Before proposing a change:

1. Open an issue describing the problem, expected behaviour, and platform.
2. Keep edits focused and preserve the non-destructive editing model.
3. Add or update tests for processing, serialization, or UI-state changes.
4. Build with warnings enabled and run `ctest --test-dir build --output-on-failure`.
5. Do not commit photographs, API keys, signing material, build output, or catalogs.

Pull requests should explain the user-facing effect, validation performed, and any
macOS- or Windows-specific behaviour.

Submitting a contribution does not grant a licence to the ChromaDesk source and
does not change the ownership terms in [NOTICE.txt](NOTICE.txt).

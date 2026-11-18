# Notes for Claude in this repo

## Commits

- Do not run `/verify` before a commit. This repo declares no checks, so `/verify` has nothing to run.
- Before committing code changes, run `make test` in `src` and the full build (`make -j8` in `src`). Commit only when both pass.

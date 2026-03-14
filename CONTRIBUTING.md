# Contributing

Thanks for helping improve Sure-Smartie-linux.

## Development setup

Recommended local workflow:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

To disable the Qt GUI during development:

```bash
cmake -S . -B build -DSURE_SMARTIE_BUILD_GUI=OFF
```

## Before opening a pull request

- keep changes focused on one problem or feature;
- update docs or example configs when behavior changes;
- run the build and tests locally;
- mention hardware assumptions if your change depends on a real display.

## Code style

- C++20
- CMake-based build
- prefer small, readable changes
- keep comments concise and explain only non-obvious logic

## Commit guidance

Clear commit messages make review easier. A short imperative summary is enough, for example:

- `Improve serial display refresh caching`
- `Add GUI privileged save helper tests`

## Testing notes

Most automated tests run against `stdout` or headless GUI mode. Hardware-specific changes
should still include a brief manual test note when you open the PR.

## Maintainer

Repository owner: NDRco <ndrco@yahoo.com>

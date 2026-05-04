# Current Baseline

Date: 2026-05-04

Baseline commit before next-stage automation work:

```text
9877209 Tighten trigger policy cleanup boundaries
```

Environment used for local reproduction:

```text
OS: Ubuntu 24.04 environment
C++ compiler: g++ 13.3.0
CMake: available via /usr/bin/cmake
CTest: available via /usr/bin/ctest
clang-format: available via /usr/bin/clang-format
```

Commands reproduced locally:

```bash
git diff --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Observed result:

```text
22/22 CTest tests passed.
```

Known baseline limitations:

- CI evidence did not exist before this baseline-protection pass.
- GCC was the only local compiler used for the recorded baseline; CI is configured to reproduce GCC and Clang builds.

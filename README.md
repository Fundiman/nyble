# Nyble — a web browser built from scratch
Nyble is a web browser built from scratch designed for modern websites — think of Discord or Youtube.

Most browsers need literal super computers to compile; Nyble does not.

# Dependency tree

This is a dependency view, not the repository directory layout:

```text
Nyble
├── NybleJS                       CLI + embedding front end (C++)
│   └── AntJS                     Ant Silver engine (C, git submodule)
│       ├── libuv                 event loop, timers, streams
│       ├── tlsuv                 TLS/stream wrapper
│       │   └── mbedTLS           TLS + crypto backend (no BoringSSL)
│       ├── mimalloc              allocator
│       ├── pcre2                 regular expressions
│       ├── sqlite3               SQLite host module
│       ├── zlib-ng               compression
│       ├── wasm-micro-runtime    WebAssembly
│       ├── skim                  TypeScript stripping
│       └── upm                   Ant package manager
└── NybleR                        native renderer (C/C++)
    ├── Lexbor                    HTML/DOM/CSSOM (local source submodule)
    ├── Tint                      WGSL compiler (local source submodule)
    │   ├── glslang               local glslangValidator build for Mesa
    │   └── SPIR-V                portable shader IR
    └── Mesa                      OpenGL/EGL + Vulkan backend (local source submodule)
        ├── llvmpipe              software OpenGL fallback
        └── lavapipe              software Vulkan fallback
```

Build systems and tools (also a dependency view, not a directory layout):

```text
Nyble build
├── git + submodules              source checkout, Ant master line, NybleR local deps
├── GNU make                      NybleJS driver (njs, embed examples, ant-posix)
├── Meson + Ninja                 Ant configure/build, including the embed dist
│   ├── CMake                     vendored mbedTLS subproject
│   ├── pkg-config                libant.pc include/link flags for hosts
│   ├── Node.js 22 + npm          Ant generated built-in JS headers
│   └── Python 3 + pip            Meson installation on POSIX CI
├── CMake + Ninja                 NybleR local dependency builds (Lexbor, Tint)
├── Meson                         NybleR local Mesa build
├── depot_tools + gclient         pinned Tint dependency sync
├── winflexbison                  pinned Windows Flex/Bison for Mesa
├── LLVM tooling                  Mesa llvmpipe/lavapipe shader compilation
├── clang / clang++               native POSIX and mingw-w64 Windows compiler
├── Zig 0.16.0                    optional mingw-w64 driver (USE_ZIG=1)
├── lld                           Windows static link driver
├── binutils / LLVM tools         ar -M, nm -P, llvm-nm, llvm-libtool-darwin
└── GitHub Actions                CI orchestration
    ├── ubuntu-latest, macos-latest   native POSIX builds + JS tests
    └── windows-latest + MSYS2 UCRT64 libant-mingw64 artifact + njs smoke
```

No MSVC, BoringSSL, WSL, or [Rust](# "An extremely bloated (at compile-time), overhyped, programming language mimicking the feel of low-level programming languages while providing a JavaScript-like node_modules storage-heavy experience for masochists.") bootstrap is required. NybleR builds Lexbor, Tint, and Mesa locally from source instead of consuming system renderer libraries. Node compatibility is opt-in; the web/WinterTC surface is the baseline.

# Frequently Asked Questions (FAQ)

<details>
  <summary><b>1. How do I build this project?</b></summary>
  <br>

  Each component SHOULD have a Makefile — run `make` to build it. 
</details>

<details>
  <summary><b>2. When will this be ready for like, normal daily usage?</b></summary>
  <br>

  Probably in a few months if not years.
</details>

# Licensing
This project is licensed under [MPL-2.0](https://www.mozilla.org/en-US/MPL/2.0), click [here for the repository's snapshot of the LICENSE](LICENSE).

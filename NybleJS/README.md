# NybleJS — Lightweight JavaScript Engine v1.0 (Biryani)

NybleJS is the JavaScript engine, CLI, and REPL of the [Nyble](https://github.com/Fundiman/nyble) browser project. As of v1.0 it runs on **Ant Silver** — an independent, MIT-licensed JavaScript engine written in C — instead of the original from-scratch C++ interpreter/VM.

The NybleJS front end is intentionally unchanged: same `njs` binary name, same Java-style flags (`-Xmx`, `-engine`, `-D`), same `--meow` cat, same colored REPL and keybindings, same "keep going after an eval blows up" behavior. Only the engine underneath was swapped.

> **v1.0 "Biryani"** — engine migration release. The v0.x engine was retired non-destructively at tag `nyblejs-v0.7-final` (commit `339bfbe`), so nothing was deleted from history.
>
> **Web-first:** Ant's browser/WinterTC APIs are always available. Node.js compatibility is opt-out at the CLI (`--no-node-compat`) and **off by default for embedders**; see [Node.js compatibility](#nodejs-compatibility).

## Table of contents

- [What NybleJS is today](#what-nyblejs-is-today)
- [v1.0 at a glance](#v10-at-a-glance)
- [Architecture](#architecture)
- [Where the old engine went](#where-the-old-engine-went)
- [Repository layout and the Ant submodule](#repository-layout-and-the-ant-submodule)
- [Features](#features)
  - [Language and runtime (Ant Silver)](#language-and-runtime-ant-silver)
  - [Web / WinterTC built-ins](#web--wintertc-built-ins)
  - [Node.js compatibility](#nodejs-compatibility)
  - [TLS, crypto, and WebSockets](#tls-crypto-and-websockets)
  - [Memory and garbage collection](#memory-and-garbage-collection)
  - [Console, errors, and completion values](#console-errors-and-completion-values)
- [Building](#building)
  - [Prerequisites](#prerequisites)
  - [Get the Ant submodule](#get-the-ant-submodule)
  - [Build the Ant embed dist (POSIX / Windows)](#build-the-ant-embed-dist-posix--windows)
  - [Build njs / njs.exe](#build-njs--njsexe)
  - [Windows shims](#windows-shims)
  - [Cache and temp locations](#cache-and-temp-locations)
  - [Verify the build](#verify-the-build)
  - [CI scope](#ci-scope)
- [Usage](#usage)
  - [REPL](#repl)
  - [Run a file](#run-a-file)
  - [Run inline code](#run-inline-code)
  - [Command-line options](#command-line-options)
  - [`-engine` semantics](#-engine-semantics)
  - [`-Xmx` memory budget](#-xmx-memory-budget)
  - [`-D` properties (`Nyble.props`)](#-d-properties-nybleprops)
  - [Node compat flags](#node-compat-flags)
  - [`--meow`](#--meow)
  - [REPL keybindings](#repl-keybindings)
  - [Piped / non-interactive REPL](#piped--non-interactive-repl)
  - [Errors do not kill the REPL](#errors-do-not-kill-the-repl)
- [Embedding NybleJS](#embedding-nyblejs)
  - [Header layering](#header-layering)
  - [Quick start (C++)](#quick-start-c)
  - [`nyble::Engine` API reference](#nybleengine-api-reference)
  - [C ABI reference](#c-abi-reference)
  - [Isolate model and lifetime rules](#isolate-model-and-lifetime-rules)
  - [Host bindings and `rawIsolate()`](#host-bindings-and-rawisolate)
  - [Node compat from a host](#node-compat-from-a-host)
  - [SpiderMonkey-style API reference (implemented)](#spidermonkey-style-api-reference-implemented)
  - [Migrating from the v0.x `Context` API](#migrating-from-the-v0x-context-api)
  - [Host link checklist](#host-link-checklist)
- [Compatibility and known gaps](#compatibility-and-known-gaps)
- [Migrating from v0.7](#migrating-from-v07)
- [Performance notes](#performance-notes)
- [Examples](#examples)
- [Versioning](#versioning)
- [Development and contributing](#development-and-contributing)
- [FAQ](#faq)
- [License](#license)

---

## What NybleJS is today

NybleJS used to be a self-contained JavaScript implementation: a lexer, recursive-descent parser, tree-walking interpreter, stack bytecode VM, mark-sweep GC, and value system, all in C++17. That engine still exists in git history but is no longer the engine behind `njs`.

Today `njs` is a thin C++ host around **Ant Silver**:

- The CLI/REPL lives in `src/main.cpp`.
- A small C++ façade, `nyble::Engine`, lives in `src/engine/ant_engine.h` / `.cpp`.
- A one-file C bridge, `src/engine/ant_engine.c` / `ant_engine_c.h`, is the **only** place that includes Ant's C-only headers.
- Everything else — parser, compiler, VM, JIT, GC, builtins, web APIs — comes from `libant.a`, built from the `third_party/ant` submodule.

That split exists because Ant's headers are **C-only** (they use C constructs that do not compile as C++). Never include `ant.h` or `ant/*.h` from a `.cpp` file; go through the bridge.

The browser-side goal is a small, embeddable, web-first JavaScript runtime. Node compatibility is kept for compatibility with existing scripts, but it is disabled by default in the runtime and in embedded hosts, so the web surface is the baseline.

## v1.0 at a glance

| Area | v0.7 (retired) | v1.0 "Biryani" |
|---|---|---|
| Engine | Hybrid tree-walker + bytecode VM, C++17 | Ant Silver, C, with its own parser/compiler/VM/JIT |
| Standard library | Hand-written builtins, partial `Date`/`JSON` | Ant's ES/Web builtins plus WinterTC APIs |
| Modules | `import`/`export` not implemented | ESM loader, `node:`/bare builtin resolution gated by node compat |
| Async | Timer stubs only | Event loop, microtasks, promises, timers, streams, fetch/WebSocket |
| TLS | None | mbedTLS/tlsuv backend, verification fail-closed; no BoringSSL |
| GC | Process-wide mark-sweep heap, one context per process | Per-isolate Ant GC via `Ant_Create()` |
| Embedding API | V8-flavored `nyble::Context`, `Value`, `gHeap.rootTracer` | SpiderMonkey-style `NyRuntime`/`NyContext`/rooted `NyValue` C API + C++ RAII wrappers; `nyble::Engine` façade remains for simple hosts |
| Build | `g++ -std=c++17 src/*.cpp`, MSYS2/WSL | clang/Zig targeting mingw-w64, Meson/Ninja for Ant, no MSYS2 dependency |
| Node compat | Not applicable | Off by default in runtime/embed; on by default in the CLI |
| Version | v0.7 (AttentionIsAllYouNeed) | v1.0 (Biryani) |

## Architecture

```
NybleJS/
├── src/
│   ├── main.cpp                  CLI, REPL, syntax highlighting, flags
│   ├── cats.h                    --meow wisdom/cat-fact database
│   ├── nyblejs.h                 public embedding header (all APIs)
│   └── engine/
│       ├── nyble_sm.h/.c         SpiderMonkey-style C API
│       ├── nyble_sm.hpp          C++ RAII wrappers (Runtime/Context/RootedValue)
│       ├── ant_engine.h/.cpp     C++ façade: nyble::Engine
│       ├── ant_engine_c.h        C-ABI declarations (safe from C++)
│       └── ant_engine.c          ONLY file that sees Ant's C headers
├── build/
│   ├── pathcch_shim.c            mingw-w64 shim for PathCchCombine
│   └── sync_shim.c               mingw-w64 shim for WaitOnAddress/WakeByAddressSingle
├── Makefile                      links njs against a prebuilt libant dist
└── README.md                     this file

third_party/ant/                  submodule (Ant Silver, MIT, C)
├── include/                      C headers
├── src/                          engine, modules, runtime, CLI
├── vendor/                       vendored dependencies (mbedTLS, tlsuv, libuv, ...)
└── build-embed-zig/libant/        locally built embed dist (gitignored)
```

Data flow for one line of script:

```
njs CLI / host
  → nyble::Engine::eval(source, out)
    → nyble_engine_eval(...)          [C ABI]
      → js_eval_bytecode_eval(js, ...) [Ant]
        → completion value → Ant_Value_Describe() → display string
  → Engine::drainEventLoop() → js_run_event_loop(js)
```

Errors take the same path in reverse: an Ant type error becomes a malloc'd string, `Engine::eval()` returns `false`, and `Engine::errors()` holds the text. The CLI prints it with an `Uncaught` prefix and carries on.

## Where the old engine went

Nothing was destroyed:

- The final v0 engine is tagged **`nyblejs-v0.7-final`** at commit **`339bfbe`**.
- The source files (`src/lexer.*`, `parser.*`, `compiler.*`, `vm.*`, `interp.*`, `gc.*`, `context.*`, etc.) remain in the tree as legacy material; they are **not** compiled into v1's `njs`.
- Do not use those files for new work. They describe the tree/VM architecture that v1 no longer uses, and they are the source of most stale claims in old docs.
- `nyblejs.h` now points only at the Ant façade; the old `nyble::Context` / `nyble::VM` / `nyble::Value` API is not part of v1.

## Repository layout and the Ant submodule

Ant is a **git submodule**, not a downloaded release and not a vendored copy of source code pasted into the repo:

```ini
# .gitmodules
[submodule "third_party/ant"]
    path = third_party/ant
    url = https://github.com/Fundiman/nyble.git
    branch = ant
```

- The submodule tracks the **`ant` branch** of the same GitHub repository, which mirrors Ant Silver's **`master`** branch (not a "latest stable" tag) plus Nyble-specific patches.
- The checked-out commit at the time of writing is `f684e91a` — `crypto: restore mbedTLS parity and secure RNG`.
- Nyble-specific Ant changes are committed inside the submodule, then the superproject records the new submodule pointer. The Windows CA-store fix for tlsuv is also preserved as `patches/tlsuv-win-ca-stores.patch` at the superproject root.
- Never fetch engine sources from a release/archive URL. `git submodule update` is the supported path.

## Features

### Language and runtime (Ant Silver)

Ant Silver is a full ES engine, not a toy interpreter. Highlights relevant to NybleJS:

- Modern syntax: `let`/`const`, closures, arrow functions, classes, `for...of`, `for...in`, destructuring, spread/rest, template literals, optional chaining, `async`/`await`, generators, `Proxy`/`Reflect`, `Symbol`, `Map`/`Set`/`WeakMap`/`WeakSet`, typed arrays and `DataView`.
- `try`/`catch`/`finally`, `throw`, proper error objects and stacks.
- Promise jobs and an event loop driven by `js_run_event_loop()`; `Engine::drainEventLoop()` is called by the CLI after every script so pending work runs before exit.
- ES modules via Ant's loader. `node:` specifiers and bare builtin names are resolved only when node compat is enabled; with node compat off, browser-style specifiers remain available and Node builtins are blocked.
- Temporal/BigInt/Intl/RegExp/WASM support in upstream Ant. **Note:** the local Zig embed build disables `temporal` and `encoding_rs`, so those are not part of the locally linked `njs.exe` yet.
- Upstream Ant reports 100% on compat-table and high test262 coverage; see `third_party/ant/README.md` for the upstream numbers. Those figures describe Ant itself, not necessarily the locally configured NybleJS embed.

### Web / WinterTC built-ins

Bootstrapped by `ant_bootstrap_modules()` (`third_party/ant/src/bootstrap.c`) and available regardless of node compat:

- Globals/internals: `Symbol`, iterator/generator helpers, `Reflect`, `Proxy`, `Math`, `BigInt`, `Date`, `Intl`, `JSON`, `console`, `structuredClone`, `AbortController`, `DOMException`.
- Web platform: `fetch`, `Request`, `Response`, `Headers`, `Blob`, `FormData`, `URL`, `URLSearchParams`, `TextEncoder`/`TextDecoder`, `EventSource`, `WebSocket`, `MessagePort`, `Web Worker`, `Navigator`, `localStorage`, `sessionStorage`, `performance`, `Observable`, timers (`setTimeout`, `setInterval`, ...), streams (readable/writable/transform/codec/compression), cron, WASM, and SQLite/host modules where enabled.
- Collections: `Map`, `Set`, `WeakMap`, `WeakSet`, queuing strategies.
- Local embed caveat: `temporal` and `encoding_rs` are disabled in the local build; mbedTLS-backed crypto coverage is described below.

### Node.js compatibility

Ant has a runtime flag, `ANT_RUNTIME_NODE_COMPAT`, backed by `ant_isolate_options_t.node_compat`. When set, bootstrap installs:

- `Buffer`
- `fs`
- `process`
- `tty`
- `events`
- Node-style `node:` / bare builtin module resolution in the ESM loader
- CJS globals and entry/repl/runtime hooks in the Ant CLI layer

Default policy:

| Entry point | Default | How to change |
|---|---|---|
| `nyble::Engine` (embedding) | **off** | Pass `nodeCompat = true` to the constructor |
| Ant runtime / `Ant_Create()` | **off** | Set `ant_isolate_options_t.node_compat = true` |
| NybleJS `njs` CLI | **on** (legacy compat) | `--no-node-compat` to disable, `--node-compat` to re-enable |
| Upstream `ant` CLI | **on** | `--web-compat` to disable |

With node compat off, browser APIs are unchanged; only the Node-flavored globals and module resolution are gated out. See [Node compat flags](#node-compat-flags).

### TLS, crypto, and WebSockets

- **No BoringSSL.** The embed is configured with `-Dtls_backend=mbedtls`, using tlsuv's mbedTLS engine.
- TLS verification **fails closed**: a self-signed host (`self-signed.badssl.com`) is rejected. Valid certificates work (`example.com:443` connects).
- Windows certificate loading patches tlsuv to include the `ROOT`, `CA`, and `AuthRoot` system stores; the patch lives in `patches/tlsuv-win-ca-stores.patch`.
- `rejectUnauthorized: false` is applied per connection via tlsuv stream state; it never mutates the shared TLS context, so one lax connection cannot weaken a later strict one. `getProtocol()` reports the negotiated version and peer certificate/authorization info is exposed by the Node shim. Default verification remains on.
- `src/modules/crypto.c` is mbedTLS-backed and covers Node/WebCrypto secrets (`random*`, `getRandomValues`, `createHash`/`createHmac`, `createCipheriv`/`createDecipheriv`, `pbkdf2`/`scrypt`, `subtle.digest`, raw HMAC/AES-GCM keys, `deriveBits`, `sign`/`verify`, `encrypt`/`decrypt`, `timingSafeEqual`) **and** standard asymmetric WebCrypto: RSASSA-PKCS1-v1_5, RSA-PSS, RSA-OAEP, ECDSA, ECDH, and AES-KW. JWK, SPKI, and PKCS#8 import/export, `wrapKey`/`unwrapKey`, and `deriveKey` are available. Public/private usages are split on generated pairs.
- `src/http/websocket.c` has its SHA1 accept-key and frame logic unstubbed for `ANT_MBEDTLS_TLS_BACKEND`.
- `src/cli/registry.c` is **intentionally still a stub**; registry parity is not claimed.

### Memory and garbage collection

- `nyble::Engine`'s constructor takes `heapBytes`; this is forwarded to `ant_isolate_options_t.object_arena_bytes`.
- `0` (the default, including when `-Xmx` is not passed) lets Ant pick its own arena size.
- `-Xmx<size>` accepts `k`, `m`, and `g` suffixes (case-insensitive) and is forwarded to the isolate. It is a sizing hint for the object arena, not a hard process limit.
- Each `Engine` owns one Ant isolate created with `Ant_Create()` and torn down with `Ant_Destroy()`; the GC is per-isolate, not a single process-wide heap as in v0.x.
- `js_setstackbase()` is called from the C bridge so Ant can scan the host stack from the right address; hosts that create their own isolates via `rawIsolate()` should preserve that pattern.

### Console, errors, and completion values

- `console.log` and friends write through Ant's console module.
- `Engine::eval()` renders the completion value with `Ant_Value_Describe()` so REPL echo stays useful for objects/arrays. `undefined` and `null` render as an empty string and produce no echo line — the same behavior as the v0.x REPL.
- A JavaScript exception makes `eval()` return `false`; the exception text is stored in `Engine::errors()` and cleared at the start of the next eval.
- Native/host errors are surfaced as C++ `std::exception` from the façade and are caught by the CLI's REPL loop.

---

## Building

### Prerequisites

- **clang** (`clang`/`clang++`) on POSIX, or **clang targeting mingw-w64** / **Zig 0.16.0** (`zig cc`, `zig c++` with `--target=x86_64-windows-gnu`) on Windows.
- **Meson + Ninja**, CMake, pkg-config, `binutils`, and Node.js 22 + npm for configuring and building the Ant embed.
- `git` with submodule support and GNU `make`.
- **No MSYS2, WSL, or MSVC dependency.** Windows builds target mingw-w64; POSIX builds are native. "we CANNOT STAND MSVC!!"
- **No Rust porting or Rust bootstrap.** The POSIX recipe uses `-Dsandbox=disabled`; do not translate anything to Rust.
- **No BoringSSL.** Use the mbedTLS TLS backend.

Supported local targets: **Windows x86_64 (mingw-w64 ABI)** and **native Linux/macOS**. No POSIX `libant.a` is shipped; build it locally with `make ant-posix`.

### Get the Ant submodule

```bash
git submodule update --init --recursive
# The `ant` branch is our Ant-master-line tracking branch (it carries the
# mbedTLS/TLS/crypto/WS and node-compat patches). Do not use the submodule's
# local `master` branch here; that is legacy NybleJS history.
git -C third_party/ant switch -C ant --track nyble/ant
git submodule status
```

`.gitmodules` records `branch = ant`, so `git submodule update --remote` follows `nyble/ant` — the Ant master line with our patches — not a release tag or the stale local `master`. The checked-out submodule commit is recorded explicitly by the superproject.

Do not download release artifacts. If the submodule is empty, the first command is the fix.

### Build the Ant embed dist (POSIX / Windows)

#### POSIX (native, no WSL/MSYS2)

From `NybleJS/`:

```bash
make ant-posix
```

That configures `third_party/ant/build-embed-posix` with:

```bash
-Db_lto=false -Dc_std=gnu2x -Dtls_backend=mbedtls \
-Dtemporal=disabled -Dencoding_rs=disabled \
-Dsandbox=disabled -Dpgo=disabled -Dcodesign=false
```

`sandbox=disabled` avoids the Rust bootstrap; `b_lto=false` keeps native objects in the archive; `codesign=false` makes macOS CI deterministic. On macOS, install LLVM (`brew install llvm`) so `llvm-nm` is available to the `embed` target. No BoringSSL is downloaded or built.

Result: `third_party/ant/build-embed-posix/libant/` with the same layout as the Windows dist. Equivalent raw commands:

```bash
cd third_party/ant
meson setup build-embed-posix \
  -Db_lto=false -Dc_std=gnu2x -Dtls_backend=mbedtls \
  -Dtemporal=disabled -Dencoding_rs=disabled \
  -Dsandbox=disabled -Dpgo=disabled -Dcodesign=false
meson compile -C build-embed-posix embed
```

#### Windows / mingw-w64 (Meson + Zig)

Use a separate build tree with LTO off so the archive contains native objects instead of LLVM bitcode. The exact options used locally are below; they are also recorded in `third_party/ant/build-embed-zig/meson-info/`.

```powershell
$env:CC  = "zig cc -target x86_64-windows-gnu"
$env:CXX = "zig c++ -target x86_64-windows-gnu"

meson setup third_party/ant/build-embed-zig third_party/ant `
  -Db_lto=false `
  -Dc_std=gnu2x `
  -Dtls_backend=mbedtls `
  -Dtemporal=disabled `
  -Dencoding_rs=disabled

meson compile -C third_party/ant/build-embed-zig embed
```

Result:

```
third_party/ant/build-embed-zig/libant/
├── include/ant.h              umbrella C header
├── include/ant/...            namespaced headers
└── lib/libant.a               engine + vendored deps in one static archive
```

The `embed` target also produces a `lib/pkgconfig/libant.pc` describing include paths, defines, and system libraries; `PKG_CONFIG_PATH` can point at `build-embed-zig/libant/lib/pkgconfig`.

Why these options:

| Option | Reason |
|---|---|
| `-Db_lto=false` | `libant.a` must hold native objects for the NybleJS link |
| `-Dc_std=gnu2x` | Ant's C sources need GNU C extensions |
| `-Dtls_backend=mbedtls` | No BoringSSL in this build |
| `-Dtemporal=disabled` | Avoids the Temporal Rust dependency for now |
| `-Dencoding_rs=disabled` | Avoids the encoding_rs Rust dependency for now |

### Build njs / njs.exe

The Makefile auto-detects the host with `uname`. POSIX hosts use native `clang`/`clang++` and default `ANT_DIR` to the `make ant-posix` output; Windows hosts keep the mingw-w64 path and default `ANT_DIR` to `../tools/ant`. Both expect `lib/libant.a` and `include/`. Override `ANT_DIR` to point at a different dist:

```bash
# POSIX (after `make ant-posix`)
cd NybleJS
make
make embed   # also builds build/embed_sm and build/embed_engine
```

```powershell
# Windows / mingw-w64
cd NybleJS
make ANT_DIR=../third_party/ant/build-embed-zig/libant USE_ZIG=1
```

Equivalent when using Zig as the driver directly (paths adjusted for your checkout):

```powershell
zig c++ -target x86_64-windows-gnu -std=c++17 -O2 -Isrc `
  -I../third_party/ant/build-embed-zig/libant/include `
  -c src/main.cpp -o build/main.o
zig c++ -target x86_64-windows-gnu -std=c++17 -O2 -Isrc `
  -I../third_party/ant/build-embed-zig/libant/include `
  -c src/engine/ant_engine.cpp -o build/engine/ant_engine.o
zig cc  -target x86_64-windows-gnu -std=gnu2x -O2 -Isrc `
  -I../third_party/ant/build-embed-zig/libant/include `
  -c src/engine/ant_engine.c -o build/engine/ant_engine.c.o
zig cc  -target x86_64-windows-gnu -O2 -c build/pathcch_shim.c -o build/pathcch_shim.o
zig cc  -target x86_64-windows-gnu -O2 -c build/sync_shim.c   -o build/sync_shim.o

zig c++ -target x86_64-windows-gnu -o njs.exe `
  build/main.o build/engine/ant_engine.o build/engine/ant_engine.c.o `
  build/pathcch_shim.o build/sync_shim.o `
  ../third_party/ant/build-embed-zig/libant/lib/libant.a `
  -lws2_32 -lcrypt32 -lsecur32 -luserenv -lntdll -liphlpapi -ldbghelp `
  -lole32 -loleaut32 -luuid -ltaskschd -lshell32 -ladvapi32 -lbcrypt `
  -lwinmm -lpathcch -lnormaliz -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
```

The system-library list mirrors `NybleJS/Makefile` (`ANT_SYS_LIBS`) and is what static `libant.a` needs on Windows (Ant + tlsuv + mbedTLS). The output is `NybleJS/njs.exe`.

### Windows shims

Two small C files in `NybleJS/build/` patch gaps in the mingw-w64/Zig sysroot:

- `pathcch_shim.c` — implements `PathCchCombine` with a bounded wide-string join and returns `STRSAFE_E_INSUFFICIENT_BUFFER` when the output does not fit. Ant uses it for path handling.
- `sync_shim.c` — implements `WaitOnAddress` / `WakeByAddressSingle` by resolving the real kernel32 functions at runtime and falling back to `Sleep`/no-op when unavailable (older Windows).

Keep these in host builds; they are not part of Ant and are not in `libant.a`.

### Cache and temp locations

On this machine the Zig caches and temp dirs are deliberately pinned (they are large and slow on the default drives):

```powershell
$env:ZIG_GLOBAL_CACHE_DIR = "E:\.zig-global-cache"
$env:ZIG_LOCAL_CACHE_DIR  = "E:\.zig-local-cache"
$env:TMP = "M:\tmp"
$env:TEMP = "M:\tmp"
```

Use the same values when rebuilding Ant or `njs.exe`, or builds will recompile everything into a new cache.

### Verify the build

```powershell
.\njs.exe --version        # NybleJS v1.0 (Biryani)
.\njs.exe -c "1 + 1"       # 2
echo 1+1 | .\njs.exe       # 2  (piped REPL)
.\njs.exe --meow           # ASCII cat + wisdom/cat fact
.\njs.exe -engine tree -c "console.log(42)"   # 42 (compat alias)
.\njs.exe -Xmx64m -c "Nyble.props"            # object with CLI props
.\njs.exe --no-node-compat -c "typeof process" # undefined
```

TLS smoke checks (network required):

```powershell
.\njs.exe --no-node-compat -c "fetch('https://example.com').then(r => console.log(r.status))"
# self-signed hosts must fail verification, not silently connect
```

### CI scope

There are two CI paths:

- `libant.yml` builds the Windows `libant-mingw64` artifact with clang/mingw-w64 and smoke-links `njs`.
- `nyblejs-posix.yml` builds native Linux/macOS `libant.a`, `njs`, and both embed examples, then runs the focused WebCrypto JS tests (`test_webcrypto_generate_export.cjs`, `test_webcrypto_asymmetric.cjs`, `examples/spec/crypto.js`).

Interactive REPL behavior (TTY keybindings/history) is still validated locally/manually; piped REPL and CLI smokes run in CI.

---

## Usage

### REPL

```powershell
.\njs.exe
```

You get a colored banner, `> ` prompt, and as-you-type syntax highlighting:

- strings — green
- numbers — cyan
- keywords (`let`, `function`, `if`, ...) — magenta
- literals (`true`, `false`, `null`, `undefined`) — bold yellow
- operators — red
- punctuation — gray
- identifiers — white

Incomplete input is detected heuristically (unbalanced `{`, `(`, `[`, or a trailing `+ - * / , .`), and the prompt switches to `... ` until the construct closes. Type `exit` on an empty line to quit, or press Ctrl+D. Up/Down recalls previously submitted lines from the in-memory history buffer (no history file is written).

### Run a file

```powershell
.\njs.exe script.js
```

The file is read as UTF-8 text and evaluated in one isolate. After eval finishes, `drainEventLoop()` runs pending promise jobs/timers before the process exits.

### Run inline code

```powershell
.\njs.exe -c "console.log(42)"
```

`-c` and a filename can be combined; `-c` wins and the filename is ignored for source selection.

### Command-line options

| Option | Description |
|---|---|
| `-help`, `--help` | Show the colored help message and exit |
| `-version`, `--version` | Show `NybleJS v1.0 (Biryani)` and exit |
| `-c <code>` | Execute inline JavaScript code instead of a file |
| `-Xmx<size>` | Ant object-arena sizing hint; suffixes `k`, `m`, `g` (e.g. `256m`, `1g`) |
| `-engine <type>`, `--engine <type>` | Engine selector: `ant` (default), `tree`, `vm`, `jit` (compat aliases; see below) |
| `-D<key>=<value>` | Set `Nyble.props.<key>` to `<value>` (strings); repeatable |
| `--node-compat` | Force Node.js compatibility on (CLI default) |
| `--no-node-compat` | Disable Node.js compatibility; browser-like globals/modules only |
| `-meow`, `--meow` | Print an ASCII cat with a random encouraging quote or cat fact |
| `<file>` | Script to run (last non-option argument) |

Unknown options are an error: the CLI prints `Unknown option '<arg>'. Use -help for usage.` and exits with status 1. Options needing a value (`-engine`, `-c`) report a specific error if missing.

### `-engine` semantics

`-engine`/`--engine` exists for CLI compatibility with v0.x scripts:

| Value | Behavior in v1 |
|---|---|
| `ant` | Explicit Ant Silver (same as the default) |
| `tree` | Accepted as a compat alias; runs the Ant engine |
| `vm` | Accepted as a compat alias; runs the Ant engine |
| `jit` | Accepted by the parser but **errors out** (`JIT compiler is not yet implemented.`) — retained as a historical stub |

There is no longer a per-script tree/VM threshold and no `jit` engine in the CLI.

### `-Xmx` memory budget

```powershell
.\njs.exe -Xmx512m script.js
.\njs.exe -Xmx1g -engine ant script.js
.\njs.exe -Xmx64m --no-node-compat script.js
```

Parsing accepts a bare number (bytes) or a `k`/`m`/`g` suffix, case-insensitive. The value becomes `object_arena_bytes` for the Ant isolate. It is a tuning hint, not an OS memory cap.

### `-D` properties (`Nyble.props`)

```powershell
.\njs.exe -Dname=myapp -Ddebug=true -Dregion=eu script.js
```

```javascript
console.log(Nyble.props.name);   // "myapp"
console.log(Nyble.props.debug);  // "true"  (string, not boolean)
console.log(Nyble.props.region); // "eu"
```

`nyble_engine_set_prop()` quotes keys and values safely and assigns them as strings on `globalThis.Nyble.props`. All props survive across evals in the same engine.

### Node compat flags

```powershell
# CLI: Node compat on by default, so this is a no-op in practice
.\njs.exe --node-compat -c "console.log(typeof Buffer)"

# Browser-like mode: no Buffer/fs/process/tty/events, no node: builtins
.\njs.exe --no-node-compat -c "console.log(typeof Buffer, typeof process)"
# undefined undefined
```

From embedded code, pass `false` (the default) or `true` as the fourth constructor argument. See [Node compat from a host](#node-compat-from-a-host).

### `--meow`

```powershell
.\njs.exe --meow
```

Always lands: an ASCII cat plus a randomly picked entry from `src/cats.h` — an encouraging quote, a whimsical line, a playful joke, or a cat fact. It is the official test that the binary is awake.

### REPL keybindings

| Key | Action |
|---|---|
| Left / Right | Move cursor within the current line |
| Home / End | Jump to start / end of line |
| Ctrl+A | Start of line |
| Ctrl+E | End of line |
| Ctrl+W | Delete word backward |
| Ctrl+U | Delete from cursor to start of line |
| Ctrl+K | Delete from cursor to end of line |
| Ctrl+C | Cancel/clear the current line (does not exit) |
| Ctrl+D | Exit the REPL |
| Backspace | Delete character before cursor |
| Delete | Delete character at cursor |
| Enter / Return | Submit the current input |
| Up / Down | Recall previous / next submitted line from the in-memory history buffer |

The Windows implementation uses `ReadConsoleInput` with `ENABLE_LINE_INPUT`/`ENABLE_ECHO_INPUT` turned off while the prompt is active, then restores the original console mode on exit.

### Piped / non-interactive REPL

```powershell
echo 1 + 1 | .\njs.exe
Get-Content script.js | .\njs.exe
```

When stdin is not a TTY:

- no banner, no prompts, no highlighting pass;
- one line is read at a time;
- bare expressions echo their value (with or without a trailing `;`);
- declarations (`let`, `function`) do not echo;
- unbalanced blocks/trailing operators keep reading until closed;
- `exit` ends the session;
- EOF (Ctrl+D equivalent) ends the session.

### Errors do not kill the REPL

This is deliberate v0.x behavior and is preserved:

```text
> throw new Error("boom")
Uncaught Error: boom
> 1 + 1
2
```

`eval()` exceptions, `std::exception`s, and even unknown C++ exceptions are caught in the REPL loop. The partial input buffer is cleared and the next prompt appears. The bridge consumes Ant's pending throw after reporting it, so `engine.drainEventLoop()` cannot turn a recoverable eval error into process exit; it is still called after every eval (inside the protected region) so async work runs. The process exits only via `exit`, Ctrl+D, EOF, or a CLI-level error before the REPL starts.

---

## Embedding NybleJS

### Header layering

```cpp
#include "src/nyblejs.h"
```

`nyblejs.h` is the single public header. It exposes:

- `engine/nyble_sm.h` — the **preferred** SpiderMonkey-style C API (`NyRuntime`, `NyContext`, rooted `NyValue` handles, explicit operations).
- `engine/nyble_sm.hpp` — C++ RAII wrappers (`nyble::sm::Runtime`, `Context`, `RootedValue`).
- `engine/ant_engine.h` — the small `nyble::Engine` façade still used by the CLI and simple string-in/string-out hosts.

Ant's C-only headers are never visible in these headers. `engine/ant_engine.c` is the sole bridge that sees them; the SpiderMonkey-style API goes through that bridge with integer value handles.

### Quick start (SpiderMonkey-style C++)

```cpp
#include <iostream>
#include "src/nyblejs.h"

int main() {
    nyble::sm::Runtime rt(0, /*nodeCompat=*/false);  // web-first default
    nyble::sm::Context cx(rt);
    if (!rt.valid() || !cx.valid()) return 1;

    nyble::sm::RootedValue two = cx.evaluate("1 + 1");
    std::cout << two.toString() << '\n';  // 2

    nyble::sm::RootedValue fn = cx.evaluate("(s) => s.toUpperCase()");
    nyble::sm::RootedValue arg = cx.string("hi");  // keep it rooted for the call
    nyble::sm::RootedValue result = cx.call(fn, cx.undefined(), {arg.get()});
    std::cout << result.toString() << '\n';  // "HI"

    cx.drainJobQueue();
    return 0;
}
```

A buildable version is `examples/embed_sm.cpp`. For hosts that only want "run this string and give me a string", the `nyble::Engine` façade is still available (`examples/embed_engine.cpp`):

```cpp
nyble::Engine engine(0, nullptr, 0, /*nodeCompat=*/false);
std::string out;
bool ok = engine.eval("1 + 1", out);
engine.drainEventLoop();
```

Compile `src/main.cpp` **not at all** for a host build. Compile the engine sources listed in [Host link checklist](#host-link-checklist) and link `libant.a`.

`test_link.cpp` and `test_context.cpp` at the NybleJS root are legacy v0.x tests against the retired `nyble::VM`/`nyble::Context` API. They are kept for reference but do not build in v1.

### `nyble::Engine` API reference

Declared in `src/engine/ant_engine.h`:

| Member | Meaning |
|---|---|
| `Engine(int argc = 0, char** argv = nullptr, size_t heapBytes = 0, bool nodeCompat = false)` | Create an Ant isolate, initialize its runtime, bootstrap modules, and install `Nyble.props`. Throws `std::runtime_error` if `Ant_Create()` fails. Node compat is **off by default** for embeddings; pass `true` to opt in. |
| `~Engine()` | Calls `Ant_Destroy()` and frees the C bridge object. |
| `bool eval(const std::string& source, std::string& result)` | Evaluate in the isolate's global scope. On success returns `true` and writes a display-ready completion string to `result` (empty for `undefined`/`null`). On a JS error returns `false` and fills `errors()`. |
| `void setProp(const std::string& key, const std::string& value)` | Set `Nyble.props[key] = value` as a string. |
| `void drainEventLoop()` | Run pending promise jobs/timers via `js_run_event_loop()`. |
| `const std::vector<std::string>& errors() const` | Last eval's error strings; cleared at the start of each eval. |
| `void* rawIsolate() const` | Escape hatch: returns the underlying `ant_t*` for host bindings. Null if the engine is gone. |
| Copy constructor / copy assignment | Deleted. One C++ object owns one isolate. |

Defaults are consistent across the runtime and embedders: `Ant_Create()` leaves node compat off, and `nyble::Engine` defaults `nodeCompat = false`. The **CLI** deliberately defaults it on (`--no-node-compat` turns it off) for legacy script compatibility.

### C ABI reference

Declared in `src/engine/ant_engine_c.h`, implemented in `src/engine/ant_engine.c`:

| Function | Contract |
|---|---|
| `nyble_engine* nyble_engine_create(int argc, char** argv, size_t heap_bytes, int node_compat)` | `Ant_Create()` + stack base + runtime init + module bootstrap + `Nyble.props` init. Returns `NULL` on allocation failure. |
| `void nyble_engine_destroy(nyble_engine*)` | `Ant_Destroy()` + free. Safe on `NULL`. |
| `int nyble_engine_eval(nyble_engine*, const char* src, size_t len, char** out)` | 1 on success, 0 on JS error. Always sets `*out` to a malloc'd string; caller frees. Empty string for undefined/null. |
| `void nyble_engine_set_prop(nyble_engine*, const char* key, const char* value)` | Safely quotes and assigns `Nyble.props[key]`. |
| `void nyble_engine_drain(nyble_engine*)` | `js_run_event_loop()`. |
| `void* nyble_engine_isolate(nyble_engine*)` | The raw `ant_t*`. |

If you write a host in plain C, use this ABI directly and never include Ant's headers from C++ translation units.

### Isolate model and lifetime rules

- One `nyble::Engine` is one `ant_t` isolate with its own global scope and GC arena. Unlike v0.x (`gHeap` was a process-wide singleton), multiple engines can coexist in one process.
- Global state installed during creation (`Nyble.props`, bootstrapped modules) lives for the engine's lifetime; re-creating an engine starts fresh.
- Scripts evaluated in the same engine share globals: define a function in one eval, call it in the next.
- `eval()` clears `errors_` and `result` on entry; save what you need before the next call.
- `rawIsolate()` is valid until destruction; do not use it after `~Engine()`.
- The C bridge calls `js_setstackbase()` against a field inside its own struct. Hosts that create additional isolates should set a stack base similarly so conservative stack scanning sees host frames.

### Host bindings and `rawIsolate()`

`rawIsolate()` is the supported path for hosts that need real Ant values (DOM bindings, storage adapters, custom modules). Cast it back to `ant_t*` in C code, not in C++:

```c
/* host_bindings.c — plain C, allowed to include Ant headers */
#include "engine/ant_engine_c.h"
#include "ant.h"

void register_host_binding(nyble_engine* e) {
    ant_t* js = (ant_t*)nyble_engine_isolate(e);
    /* install your native functions/modules on js here */
}
```

Do not include `ant.h` from a `.cpp` file; the bridge split exists for that reason.

### Node compat from a host

```cpp
nyble::Engine nodeish(0, nullptr, 0, /*nodeCompat=*/true);   // Buffer/fs/process/tty/events available
nyble::Engine browser(0, nullptr, 0, /*nodeCompat=*/false);  // web APIs only (recommended)
```

The flag is applied at isolate creation, before module bootstrap. It cannot be flipped mid-life; create a second engine if you need both shapes.

### SpiderMonkey-style API reference (implemented)

The v0 `nyble::Context` was V8-flavored (`HandleScope`, `gHeap.rootTracer`, implicit rooting). v1 replaces it with an actual SpiderMonkey-shaped API — not just a naming direction — implemented over the Ant bridge:

- `NyRuntime` / `nyble::sm::Runtime` — engine configuration, analogous to `JSRuntime`.
- `NyContext` / `nyble::sm::Context` — one isolate/global scope, analogous to `JSContext`.
- `NyValue` / `nyble::sm::RootedValue` — explicit rooted value handles, analogous to `JS::RootedValue`/`JS::HandleValue`. Handles are kept alive on a private roots object in the isolate global.
- `Ny_Evaluate` / `Context::evaluate()` — analogous to `JS_EvaluateScript`/`JS::Evaluate`.
- `Ny_GetProperty`, `Ny_SetProperty`, `Ny_Call` — analogous to `JS_GetProperty`, `JS_SetProperty`, `JS_Call`.
- `Ny_HasPendingException`, `Ny_ClearPendingException` — analogous to `JS_GetPendingException`/`JS_ClearPendingException`.
- `Ny_DrainJobQueue` — drains promise jobs/timers after evaluation.

Declared in `src/engine/nyble_sm.h` (C) and `src/engine/nyble_sm.hpp` (C++), implemented in `src/engine/nyble_sm.c` on top of the C bridge:

| C API | Contract |
|---|---|
| `Ny_NewRuntime(size_t heap_bytes, int node_compat)` / `Ny_DestroyRuntime` | Create/destroy runtime configuration. Node compat off with `0`. |
| `Ny_NewContext(NyRuntime*, int argc, char** argv)` / `Ny_DestroyContext` | Create/destroy one Ant isolate + global scope. |
| `Ny_Undefined`, `Ny_Null`, `Ny_Boolean`, `Ny_Number`, `Ny_String` | Create rooted values; release with `Ny_FreeValue`. |
| `Ny_Evaluate(cx, src, len)` | Evaluate script; `NY_VALUE_INVALID` on error, message via `Ny_GetErrorMessage`/`Ny_HasPendingException`. |
| `Ny_GetProperty`, `Ny_SetProperty`, `Ny_Call` | Property access and function invocation. |
| `Ny_ToString`, `Ny_ToNumber`, `Ny_TypeOf` | Display string (malloc'd), numeric coercion, type tag. |
| `Ny_GetErrorMessage`, `Ny_HasPendingException`, `Ny_ClearPendingException` | Error/pending-exception handling. |
| `Ny_DrainJobQueue`, `Ny_FreeValue`, `Ny_Version` | Job draining, handle release, version string. |

The C++ layer uses RAII: `RootedValue` unroots on destruction, supports move semantics, and is non-copyable. Keep a `RootedValue` alive for as long as you pass its handle to another call, and destroy all rooted values before their `Context`.

### Migrating from the v0.x `Context` API

| Old (v0.x) | New (v1) |
|---|---|
| `nyble::Context ctx;` | `nyble::sm::Runtime rt; nyble::sm::Context cx(rt);` (or `nyble::Engine` for simple string eval) |
| `ctx.evaluate(src, nyble::Value& out)` | `nyble::sm::RootedValue out = cx.evaluate(src);` then `out.toString()` / `out.toNumber()` |
| `ctx.define("f", native)` | Install natives in C via `nyble_engine_isolate()`, or set globals with `Ny_SetProperty` |
| `ctx.call(ctx.get("f"), args)` | `cx.call(f, this_val, {arg.get(), ...})` |
| `ctx.get("name")` | `cx.getProperty(global, "name")` |
| `ctx.parseErrors()` | `cx.errorMessage()` / `cx.pendingException(msg)` |
| `gHeap.rootTracer = ...` | Not needed; `RootedValue` handles are rooted on the isolate |
| `setEngine`/`setVmThreshold`/`lastEngine` | Removed; Ant handles execution strategy internally |
| `nyble::VM` / `nyble::Interpreter` | Removed from the build; historical sources only |
| `throw NybleRuntimeError(...)` from natives | Use Ant's C API to throw on the isolate; errors surface through the C/C++ error accessors |

### Host link checklist

1. Compile C++: `src/main.cpp` (CLI only), `src/engine/ant_engine.cpp`. Use C++17.
2. Compile C: `src/engine/ant_engine.c`, `src/engine/nyble_sm.c`. On Windows also compile `build/pathcch_shim.c` and `build/sync_shim.c`. Use C (gnu2x is what Ant expects) and define `NYBLE_ANT_PACKAGED` for the embed dist headers.
3. Include dirs: `src/`, `src/engine`, `<embed-dist>/include`, `<embed-dist>/include/ant`.
4. Link `libant.a` with a C++ driver (`clang++`, or `zig c++ --target=x86_64-windows-gnu`).
5. Add platform system libs from `ANT_SYS_LIBS` in `NybleJS/Makefile` (pkg-config `libant` on POSIX; mingw-w64 libs plus shims on Windows).
6. The `embed` make target builds both SpiderMonkey-style and `nyble::Engine` examples without `src/main.cpp`.
7. Do not link the legacy `*.o` files at the NybleJS root (`compiler.o`, `vm.o`, `context.o`, ...). They belong to the retired v0 engine.

---

## Compatibility and known gaps

**Still missing / partial in v1:**

- `src/cli/registry.c` in Ant is intentionally stubbed; no registry parity.
- TLS: verification is fail-closed by default, `getProtocol()` reports the negotiated version, and `rejectUnauthorized: false` is per connection. Peer certificate/authorization info is exposed, but broader TLS plumbing is not claimed to be fully Node-equivalent.
- Crypto: mbedTLS-backed Node/WebCrypto covers hashes, HMAC, AES-GCM/Cipher, PBKDF2/scrypt, random helpers, raw and asymmetric keys, JWK/SPKI/PKCS#8 import/export, key wrapping, and `deriveKey`/`deriveBits`. Full Node `crypto` API parity is not claimed.
- WebSocket/registry behavior beyond the unstubbed SHA1/frame paths is not fully verified against Node.
- Node compatibility surface is intentionally partial: `Buffer`, `fs`, `process`, `tty`, `events`, and node/bare builtin resolution are gated; broader Node APIs are not guaranteed.
- Local embed disables `temporal` and `encoding_rs`; those Ant features are not in the shipped `njs.exe` yet.
- POSIX builds: the REPL has `isatty`/`termios` shims for raw mode, arrow/Home/End/Delete keys, and Up/Down history. There is no shipped POSIX `libant.a`; build it locally with `make ant-posix` (or the equivalent Meson commands above).
- `-engine jit` is a stub that errors out.

**No longer problems (v0.x claims that are obsolete):**

- "No classes / no async / no `import` / no `Promise`" — all provided by Ant Silver.
- "One engine per process because `gHeap` is a singleton" — isolates are per-`Engine`.
- "MSYS2 required" — not needed with the Zig/mingw-w64 path.
- "Tree-walker for <300 AST nodes / VM for larger" — that heuristic is gone.

## Migrating from v0.7

1. Rebuild `njs` with the Ant embed dist; do not reuse old object files.
2. Keep your CLI flags: `-Xmx`, `-engine`, `-D`, `-c`, `--meow` all still parse.
3. If you relied on Node globals, they are still on by default in the CLI; use them or switch to `--no-node-compat` and move to web APIs.
4. If you embedded via `nyblejs.h`, replace `nyble::Context`/`nyble::VM` usage with `nyble::Engine` per the migration table above.
5. If you relied on v0 quirks (string `eval` stub, stubbed timers/URI helpers, `JSON.stringify` using raw `toString()`), expect real Ant behavior instead; test your scripts.
6. Old engine source and history remain available at `nyblejs-v0.7-final` / `339bfbe` for reference or rollback experiments.

## Performance notes

- Ant upstream quotes a ~9 MB runtime and ~4 ms cold start; those are upstream numbers for the `ant` binary, not measured `njs.exe` figures.
- The embed is built with `-Db_lto=false` so hosts link native objects; `-O2`/`-O3` are fine, but do not rely on LTO across the archive.
- mbedTLS replaces BoringSSL; TLS verification stays on by default and the Windows CA-store patch is required for valid cert chains on Windows.
- `Engine::eval()` renders results through `Ant_Value_Describe()`; that is display formatting, not a serialization API. Use Ant's JSON/string conversion APIs from C for machine-readable output.
- `drainEventLoop()` is explicit: embed hosts that forget it will leave promise jobs pending.

## Examples

These work in v1 (the classic NybleJS samples, now interpreted by Ant Silver):

```javascript
// Fibonacci
function fib(n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}
console.log(fib(10));

// Array methods
const arr = [1, 2, 3, 4, 5];
console.log(arr.map(x => x * 2));

// Closure
function counter() {
    let count = 0;
    return () => ++count;
}
const c = counter();
console.log(c(), c());

// Try/catch/finally
try {
    throw new Error("error!");
} catch (e) {
    console.log(e.message);
} finally {
    console.log("done");
}

// Constructor + prototype + this binding
function Animal(name) { this.name = name; }
Animal.prototype.speak = function () { return this.name + " says hi"; };
const a = new Animal("Rex");
console.log(a.speak());

// Inheritance
function Dog(name, breed) {
    Animal.call(this, name);
    this.breed = breed;
}
Dog.prototype = Object.create(Animal.prototype);
Dog.prototype.bark = function () { return this.name + " barks!"; };
const d = new Dog("Buddy", "Golden");
console.log(d.speak(), d.bark());

// Function.prototype.call
function greet(greeting) { return greeting + ", " + this.name; }
console.log(greet.call({ name: "World" }, "Hello"));
```

Web APIs (available with node compat off):

```javascript
crypto.randomUUID();                       // e.g. "9b1deb4d-3b7d-4bad-9bdd-2b0d7b3dcb6d"
crypto.getRandomValues(new Uint8Array(4)); // random bytes
await crypto.subtle.digest("SHA-256", new TextEncoder().encode("nyble"));

fetch("https://example.com")
    .then(r => console.log("status:", r.status));

const ws = new WebSocket("wss://example.com/socket");
ws.onmessage = ev => console.log(ev.data);
```

Node compat (CLI default, or `--node-compat`):

```javascript
console.log(Nyble.props.debug);   // string from -Ddebug=true
console.log(typeof Buffer);       // "object" with node compat, "undefined" without
console.log(require("node:events"));  // gated by the runtime flag
```

## Versioning

NybleJS uses the two-level scheme established in `ac3240c`:

- Versions are `major.minor`; there is **no patch tier**.
- Major versions are named. Names alternate between famous CS/AI research papers and foods from somewhere in the world — no sweets or desserts (to avoid sounding like an Android clone).
  - v0: **AttentionIsAllYouNeed** (paper) — retired at v0.7, tagged `nyblejs-v0.7-final`.
  - v1: **Biryani** (food) — Ant Silver migration.
  - v2: [paper] — next planned tier.
  - v3: [food] — and so on.
- Minor versions add features, significant changes, or bug fixes and have no name.
- Every commit that changes engine behavior must bump the minor version.
- The constants live in `src/main.cpp` as `NYBLE_VERSION` and `NYBLE_VERSION_NAME`; update both together with this README's title and tables.

## Development and contributing

- Work on the CLI/REPL/façade in `NybleJS/`; work on engine behavior in `third_party/ant/` (the submodule) and record the new submodule commit in the superproject.
- Ant-side local commits are made on the submodule's `ant` branch and pushed there explicitly; the superproject then gets a `submodule: point ant to ...` commit.
- Preserve the bridge rule: Ant headers only in `src/engine/ant_engine.c`. If a new Ant header must be visible, add a C shim, not a C++ include.
- Keep the REPL crash-resilient: wrap eval/drain in `try`/`catch`, including `catch (...)`.
- When unstubbing Ant modules, prefer mbedTLS/tlsuv paths and verify fail-closed behavior before adding permissive options.
- `patches/tlsuv-win-ca-stores.patch` documents the Windows CA-store fix; keep it in sync with the submodule vendor change.
- Build and smoke locally with the Zig/Meson recipe above; CI covers Ant only.
- **Do not push to production/main unless explicitly asked.** Local commits and tags are fine.

Before finalizing engine-side changes, run Ant's own preflight from `third_party/ant` (`maid preflight`) if available, then the focused test for the module you touched; broad build/spec runs may need approval.

## FAQ

**Why replace a working engine?**
The v0 engine lacked the modern language surface and web APIs the browser needs. Ant Silver already targets WinterTC, has a JIT, streams, fetch/WebSocket, and a much broader spec baseline, with a small binary and fast cold start.

**Is the old engine deleted?**
No. `nyblejs-v0.7-final` / `339bfbe` preserves it, and the legacy sources are still in the tree for reference. They are simply not linked into v1.

**Why a submodule instead of vendoring Ant?**
To track Ant's `master` line with real commits, patches, and provenance — and to avoid release downloads. The tracked branch is `ant` (`nyble/ant` upstream), which is the Ant master line with our mbedTLS/TLS/crypto/WS and node-compat patches. The submodule's local `master` is stale NybleJS history and is intentionally not the tracking branch; keep a local `ant` branch tracking `nyble/ant` as shown in [Get the Ant submodule](#get-the-ant-submodule).

**Why mbedTLS and not BoringSSL?**
BoringSSL is explicitly out of scope. The mbedTLS/tlsuv backend builds with Zig/mingw-w64 and lets us keep verification fail-closed while patching Windows CA-store loading.

**Do I need MSYS2 or MSVC?**
Neither. Use Zig or clang with the `x86_64-windows-gnu`/`x86_64-w64-mingw32` target; Meson/Ninja configure the Ant embed; the two shims in `NybleJS/build/` cover mingw-w64 gaps.

**Is Node.js compat on?**
In the `njs` CLI, yes by default (for script compatibility). In the runtime and in embedded `nyble::Engine`, no — pass `true` explicitly or use `--node-compat` if you need it. Web APIs do not depend on it.

**Why does `--no-node-compat` show `typeof process === "undefined"`?**
Because `bootstrap.c` gates `process`, `tty`, `events`, `fs`, and `Buffer` behind `ANT_RUNTIME_NODE_COMPAT`, and the ESM loader blocks `node:`/bare builtin resolution when it is off.

**Is TLS verification optional?**
Verification is on by default and fails closed. `rejectUnauthorized: false` is scoped to the individual connection and does not change the shared TLS context; do not depend on it for production security decisions.

**Can I embed multiple isolates?**
Yes. Each `nyble::Engine` creates its own Ant isolate with its own global scope and GC arena; copy construction is disabled to keep ownership clear.

**What happened to `nyble::Context` and `gHeap.rootTracer`?**
Retired with v0. The v1 API is the SpiderMonkey-style `NyRuntime`/`NyContext`/`RootedValue` surface (C and C++), backed by the C bridge. `nyble::Engine` remains as the tiny string-in/string-out façade used by the CLI. There are no V8-style handle scopes or `gHeap.rootTracer` globals.

## License

The Nyble project is licensed under [MPL-2.0](https://www.mozilla.org/en-US/MPL/2.0); see the repository [LICENSE](../LICENSE). Ant Silver is MIT-licensed inside `third_party/ant` (see `third_party/ant/LICENSE.txt`), and vendored dependencies keep their own licenses.

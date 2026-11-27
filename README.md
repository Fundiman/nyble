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
```

Node compatibility is opt-in; the web/WinterTC surface is the baseline.

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

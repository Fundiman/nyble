# Minimal standalone Tint checkout for NybleR.
#
# The upstream standalone.gclient pulls Chromium test infrastructure, CTS,
# DXC, libc++, protobuf, googletest, and benchmark trees. NybleR needs only
# the Tint CMake build inputs: abseil, SPIRV-Headers, SPIRV-Tools, and
# glslang from vulkan-deps. Everything else is skipped to keep the local
# dependency tree small.

solutions = [
  {
    "name": ".",
    "url": "https://dawn.googlesource.com/tint",
    "deps_file": "DEPS",
    "managed": False,
    "custom_deps": {
      "build": None,
      "buildtools/clang_format/script": None,
      "buildtools/reclient": None,
      "buildtools/win": None,
      "testing": None,
      "third_party/catapult": None,
      "third_party/dxc": None,
      "third_party/google_benchmark/src": None,
      "third_party/googletest": None,
      "third_party/gpuweb-cts": None,
      "third_party/libc++/src": None,
      "third_party/libc++abi/src": None,
      "third_party/ninja": None,
      "third_party/protobuf": None,
      "third_party/vulkan-deps/spirv-cross/src": None,
      "third_party/vulkan-deps/vulkan-headers/src": None,
      "third_party/vulkan-deps/vulkan-loader/src": None,
      "third_party/vulkan-deps/vulkan-tools/src": None,
      "third_party/vulkan-deps/vulkan-utility-libraries/src": None,
      "third_party/vulkan-deps/vulkan-validation-layers/src": None,
      "tools/clang": None,
    },
  },
]

cmake_minimum_required(VERSION 3.20)

# Build wrapper for Trex and its dependencies using local vendored sources.
# FetchContent source dir overrides are supplied by the NybleR Makefile.

project(nybler-trex-build LANGUAGES C CXX)

add_subdirectory(${TREX_SOURCE_DIR} ${CMAKE_BINARY_DIR}/trex EXCLUDE_FROM_ALL)

# Trex has no install rules; define them here so `cmake --install` produces a
# self-contained prefix matching the other NybleR dependencies.
install(TARGETS trex ARCHIVE DESTINATION lib)
install(DIRECTORY ${TREX_SOURCE_DIR}/include/ DESTINATION include/trex)

if(TARGET freetype)
  install(TARGETS freetype ARCHIVE DESTINATION lib)
endif()
if(TARGET harfbuzz)
  install(TARGETS harfbuzz ARCHIVE DESTINATION lib)
endif()

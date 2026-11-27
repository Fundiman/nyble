cmake_minimum_required(VERSION 3.20)

project(nybler VERSION 0.1.0 LANGUAGES C CXX)

set(NYBLER_SOURCES
  src/document.c
  src/layout.c
  src/instance.c
  src/shader_bridge.cpp
  src/version.c
)

add_library(nybler STATIC ${NYBLER_SOURCES})
add_library(nybler::nybler ALIAS nybler)

target_include_directories(nybler
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(LEXBOR_ROOT)
  target_include_directories(nybler PRIVATE "${LEXBOR_ROOT}/include")
  target_compile_definitions(nybler PRIVATE LEXBOR_STATIC)
endif()

if(NOT TINT_ROOT AND CMAKE_PREFIX_PATH)
  list(GET CMAKE_PREFIX_PATH 0 TINT_ROOT)
endif()
if(TINT_ROOT)
  set(TINT_INCLUDE_DIR "${TINT_ROOT}/include/src/tint")
  if(WIN32)
    file(GLOB TINT_LIBRARIES "${TINT_ROOT}/lib/tint*.lib")
  else()
    file(GLOB TINT_LIBRARIES "${TINT_ROOT}/lib/libtint*.a")
  endif()
  if(NOT EXISTS "${TINT_INCLUDE_DIR}")
    message(FATAL_ERROR "Tint headers are missing from ${TINT_INCLUDE_DIR}")
  endif()
  if(NOT TINT_LIBRARIES)
    message(FATAL_ERROR "Tint libraries are missing from ${TINT_ROOT}/lib")
  endif()

  target_include_directories(nybler PRIVATE "${TINT_INCLUDE_DIR}")
  if(APPLE)
    target_link_libraries(nybler PUBLIC ${TINT_LIBRARIES} ${TINT_LIBRARIES})
  else()
    target_link_libraries(nybler PUBLIC
      "-Wl,--start-group" ${TINT_LIBRARIES} "-Wl,--end-group")
  endif()
endif()

target_compile_features(nybler PUBLIC c_std_11 cxx_std_17)
set_target_properties(nybler PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

target_compile_definitions(nybler PRIVATE
  NYBLER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  NYBLER_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  NYBLER_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

install(TARGETS nybler ARCHIVE DESTINATION lib)
install(DIRECTORY include/nybler DESTINATION include)

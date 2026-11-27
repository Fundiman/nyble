cmake_minimum_required(VERSION 3.20)

project(nybler VERSION 0.1.0 LANGUAGES C CXX)

set(NYBLER_SOURCES
  src/document.c
  src/layout.c
  src/instance.c
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

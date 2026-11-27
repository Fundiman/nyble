file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../CMakeLists.txt" NYBLER_PROJECT_LINE REGEX "^project\\(nybler VERSION ")
string(REGEX REPLACE "^project\\(nybler VERSION ([0-9]+\\.[0-9]+\\.[0-9]+).*$" "\\1" NYBLER_VERSION "${NYBLER_PROJECT_LINE}")
message(STATUS "${NYBLER_VERSION}")

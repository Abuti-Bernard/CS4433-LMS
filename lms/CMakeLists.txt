cmake_minimum_required(VERSION 3.14)
project(SchoolLMS CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Keep the executable in the same place for single- and multi-configuration
# generators (MinGW Makefiles, Ninja and Visual Studio).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}")
endforeach()

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

# Everything except main.cpp goes in a library so the tests (Phase 5) can reuse it.
add_library(lms_core
    src/Database.cpp
    src/Domain.cpp
    src/Security.cpp
    src/Repositories.cpp
    src/Services.cpp
    src/Input.cpp
    src/ConsoleApp.cpp)
target_include_directories(lms_core PUBLIC src)
# CMake 4.x renamed the imported target; support both spellings.
if(TARGET SQLite3::SQLite3)
  set(LMS_SQLITE_TARGET SQLite3::SQLite3)
else()
  set(LMS_SQLITE_TARGET SQLite::SQLite3)
endif()
target_link_libraries(lms_core PUBLIC ${LMS_SQLITE_TARGET} OpenSSL::Crypto)

add_executable(lms src/main.cpp)
target_link_libraries(lms PRIVATE lms_core)

if(MSVC)
  target_compile_options(lms_core PRIVATE /W4)
  target_compile_options(lms PRIVATE /W4)
else()
  target_compile_options(lms_core PRIVATE -Wall -Wextra)
  target_compile_options(lms PRIVATE -Wall -Wextra)
endif()

# End-to-end test: drives the real program and checks the database (needs Python 3).
# Run with:  ctest --test-dir build --output-on-failure
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  enable_testing()
  add_test(NAME end_to_end
           COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke_test.py $<TARGET_FILE:lms>)
endif()

cmake_minimum_required(VERSION 3.20)
project(Pather LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Pather
    src/cli.cpp
    src/environment.cpp
    src/main.cpp
)

target_compile_features(Pather PRIVATE cxx_std_17)
target_include_directories(Pather PRIVATE include)
target_compile_definitions(Pather PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(Pather PRIVATE advapi32 user32)

if(MINGW)
    target_link_options(Pather PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

cmake_minimum_required(VERSION 3.15)
project(snl_compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(snlc
    src/app/main.cpp
    src/lexical/lexical.cpp
    src/syntax/syntax.cpp
    src/semantic/semantic.cpp
    src/codegen/codegen.cpp
    src/driver/driver.cpp
)

target_include_directories(snlc PRIVATE src)

# 设置 CMake 的最低版本要求
cmake_minimum_required(VERSION 3.16)
# 设置项目名称和使用的 C++ 标准
project(colib)

# 指定 C++ 标准
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-march=native -ffast-math)
set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_CXX_FLAGS "-Werror -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")

option(COLIB-UNITTEST "build unit-test" ON)

# gtest
find_package(GTest REQUIRED)
include_directories(${GTest_INCLUDE_DIRS})

include_directories(src)

# 添加源文件
file(GLOB_RECURSE SRC
      src/thread/*.cc
      src/thread/*.h 
      src/main.cc
)

file(GLOB_RECURSE SRC_WITHOUT_MAIN
      src/thread/*.cc
      src/thread/*.h
)

# 添加可执行文件
add_executable(colib ${SRC})

if (COLIB-UNITTEST)
    enable_testing()
    file(GLOB_RECURSE SRC_TEST
            tests/*.cpp)
    add_executable(colib-unitest ${SRC_WITHOUT_MAIN} ${SRC_TEST})
    target_link_libraries(colib-unitest PRIVATE gtest pthread nlohmann_json::nlohmann_json)
    target_link_libraries(colib-unitest PRIVATE libabsl_crc32c.a libabsl_crc_internal.a libabsl_raw_logging_internal.a)
endif (COLIB-UNITTEST)
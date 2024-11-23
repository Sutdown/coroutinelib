# 设置 CMake 的最低版本要求
cmake_minimum_required(VERSION 3.16)

# 设置项目名称和使用的 C++ 标准
project(coroutine)

# 指定 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(src)

# 添加源文件
set(GLOB_RECURSE SRC
    
)

file(GLOB_RECURSE SRC_WITHOUT_MAIN

)

# 添加可执行文件
add_executable(coroutinelib ${SOURCES})

# 如果使用线程库，链接 pthread（CMake 自动适配不同平台）
find_package(Threads REQUIRED)
target_link_libraries(coroutinelib PRIVATE Threads::Threads)
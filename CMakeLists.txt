cmake_minimum_required(VERSION 3.20)
project(threadpool_project CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall -Wextra -Wpedantic -Wshadow -Wconversion")

find_package(Threads REQUIRED)

add_library(thread_pool STATIC
    src/thread_pool.cpp
)
target_include_directories(thread_pool PUBLIC include)
target_link_libraries(thread_pool PUBLIC Threads::Threads)

add_executable(stress_test
    tests/stress_test.cpp
)
target_link_libraries(stress_test PRIVATE thread_pool)

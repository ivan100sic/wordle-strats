cmake_minimum_required(VERSION 3.0)
project(WordleStrats)

set(CMAKE_CXX_STANDARD 20)
find_package(Threads REQUIRED)

add_executable(wordle wordle.cpp)
target_link_libraries(wordle PRIVATE Threads::Threads)

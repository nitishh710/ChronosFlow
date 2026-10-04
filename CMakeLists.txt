cmake_minimum_required(VERSION 3.20)
project(ChronosFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pthread")

include_directories(include)

add_executable(producer_vision src/producer_vision.cpp)
target_link_libraries(producer_vision rt pthread)

add_executable(consumer_llm src/consumer_llm.cpp)
target_link_libraries(consumer_llm rt pthread)
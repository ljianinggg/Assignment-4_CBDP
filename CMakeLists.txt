cmake_minimum_required(VERSION 3.16)
project(cbdp)

set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Werror -fsanitize=address")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -fsanitize=address")

find_package(CURL REQUIRED)

add_executable(coordinator
        coordinator.cpp
        CurlEasyPtr.cpp
        )
target_link_libraries(coordinator PUBLIC CURL::libcurl)

add_executable(worker
        worker.cpp
        CurlEasyPtr.cpp
        )
target_link_libraries(worker PUBLIC CURL::libcurl)


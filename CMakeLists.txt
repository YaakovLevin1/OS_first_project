cmake_minimum_required(VERSION 3.10)
project(OS_first_project)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wpedantic")

include_directories(.)

add_executable(program
        uthreads.cpp
        tests/test_2_thread_id_management.cpp
        ../../../../../../wsl.localhost/Ubuntu/home/tlp/projects/OS_first_project/tests/test_3_5.cpp
        ../../../../../../wsl.localhost/Ubuntu/home/tlp/projects/OS_first_project/tests/test_3_5.h
        ../../../../../../wsl.localhost/Ubuntu/home/tlp/projects/OS_first_project/tests/test_4.cpp
        ../../../../../../wsl.localhost/Ubuntu/home/tlp/projects/OS_first_project/tests/test_4.h
)
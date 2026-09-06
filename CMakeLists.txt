cmake_minimum_required(VERSION 3.16)
project(dma_timing LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dma_timing
    src/main.cpp
    src/config.cpp
    src/page_pool.cpp
    src/reads.cpp
    src/sweep.cpp
)
target_include_directories(dma_timing PRIVATE src)

if(MSVC)
    target_compile_definitions(dma_timing PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(dma_timing PRIVATE /W3 /O2)
endif()

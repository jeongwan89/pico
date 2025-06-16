cmake_minimum_required(VERSION 3.13)
project(dht22_lib C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_library(dht22_lib STATIC DHT22.cpp)

target_include_directories(dht22_lib 
    PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(dht22_lib
    PUBLIC
    pico_stdlib
    hardware_gpio
    hardware_timer
    hardware_clocks
    hardware_dma
    hardware_pio 
)
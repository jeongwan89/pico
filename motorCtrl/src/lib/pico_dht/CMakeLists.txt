add_library(dht STATIC dht.c)

pico_generate_pio_header(dht ${CMAKE_CURRENT_LIST_DIR}/dht.pio)

target_include_directories(dht 
    PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(dht
    PUBLIC
    hardware_clocks
    hardware_dma
    hardware_pio 
    pico_stdlib)
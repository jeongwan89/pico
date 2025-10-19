add_library(esp01
    esp01.c
)

target_include_directories(esp01 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(esp01 PUBLIC
    pico_stdlib
    hardware_uart
)

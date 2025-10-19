add_library(tm1637
    tm1637.c
)

target_include_directories(tm1637 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(tm1637 PUBLIC
    pico_stdlib
)

add_library(tm1637 STATIC tm1637.cpp)
target_include_directories(tm1637 PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(tm1637
    pico_stdlib)
    
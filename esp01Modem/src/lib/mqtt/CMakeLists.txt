add_library(mqtt_client
    mqtt_client.c
)

target_include_directories(mqtt_client PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(mqtt_client PUBLIC
    pico_stdlib
)

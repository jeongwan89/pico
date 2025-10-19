# Targets for esp01Modem project using src/ layout

# Executable
add_executable(esp01Modem
    esp01Modem.cpp
    pio/blink.pio
)

pico_set_program_name(esp01Modem "esp01Modem")
pico_set_program_version(esp01Modem "0.1")

# Generate PIO header for blink
pico_generate_pio_header(esp01Modem ${CMAKE_CURRENT_LIST_DIR}/pio/blink.pio)

# StdIO config
pico_enable_stdio_uart(esp01Modem 0)
pico_enable_stdio_usb(esp01Modem 1)

# Library subdirs
add_subdirectory(lib/esp01)
add_subdirectory(lib/mqtt)
add_subdirectory(lib/tm1637)

# Include directories
target_include_directories(esp01Modem PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/common
)

# Link libraries
target_link_libraries(esp01Modem
    pico_stdlib
    hardware_pio
    hardware_uart
    esp01
    mqtt_client
    tm1637
)

pico_add_extra_outputs(esp01Modem)

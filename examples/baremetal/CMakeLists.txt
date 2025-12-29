cmake_minimum_required(VERSION 3.24)

add_executable(rp2350_geek_baremetal
    src/main.c
)

# Ensure the ELF file has a .elf suffix so picotool can infer the format.
set_property(TARGET rp2350_geek_baremetal PROPERTY SUFFIX ".elf")

target_include_directories(rp2350_geek_baremetal PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(rp2350_geek_baremetal
    pico_stdlib
    hardware_adc
    hardware_i2c
    hardware_spi
)

pico_enable_stdio_usb(rp2350_geek_baremetal 1)
pico_enable_stdio_uart(rp2350_geek_baremetal 1)

pico_add_extra_outputs(rp2350_geek_baremetal)

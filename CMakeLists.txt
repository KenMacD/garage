cmake_minimum_required(VERSION 3.20.0)

set(BOARD particle_xenon)

list(APPEND OVERLAY_CONFIG "${CMAKE_CURRENT_LIST_DIR}/network.conf")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(garage)

target_compile_options(app PRIVATE -Wall -Werror -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-ignored-qualifiers -Wno-type-limits)

target_sources(app PRIVATE
	src/main.c
	src/srp-init.c
	src/switch.c
)

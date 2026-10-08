cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(stm32f7_dashboard)

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE
    src/main.c
    src/system_overview.c
    src/task_management.c
    src/pc_profiler.c
    src/generated/symtab_data.c
    src/events_logs.c
    src/live_variables.c
    src/ball_game.c
    src/launcher.c
    src/splash.c
    src/images/img_image1.c
    src/images/img_image2.c
    src/temperature.c
    src/mpu6050_input.c
    src/async_printk.c
    src/watchdog.c
)

set(PC_PROFILER_SYMTAB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/symtab_data.c)
if(NOT EXISTS ${PC_PROFILER_SYMTAB_SRC})
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/generated)
    file(WRITE ${PC_PROFILER_SYMTAB_SRC}
        "#include \"pc_profiler.h\"\n"
        "const struct symtab_entry pc_profiler_symtab[] = { { 0x0u, 0x0u, \"(build twice)\" } };\n"
        "const unsigned int pc_profiler_symtab_count = 0;\n")
endif()

add_custom_target(pc_profiler_gen_symtab ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_symtab.py
            --nm  ${CMAKE_NM}
            --elf $<TARGET_FILE:zephyr_final>
            --out ${PC_PROFILER_SYMTAB_SRC}
    COMMENT "pc_profiler: regenerating symtab_data.c from zephyr.elf (used by next build)"
    VERBATIM
)
add_dependencies(pc_profiler_gen_symtab zephyr_final)

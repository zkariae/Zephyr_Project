cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(stm32f7_dashboard)

# So src/generated/symtab_data.c can #include "pc_profiler.h" (quote-includes
# only search the including file's own directory by default, and
# src/generated/ isn't src/).
target_include_directories(app PRIVATE src)

target_sources(app PRIVATE
    src/main.c
    src/system_overview.c
    # src/rtos_tasks.c is disabled (kept, not built): replaced by
    # task_management.c + pc_profiler.c below.
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

# --- PC profiler symbol table (two-pass build) ------------------------------
# Bootstrap: on a fresh checkout, src/generated/ is gitignored (it's build
# output) so it doesn't exist yet. Seed an empty stub so target_sources()
# above has a real file to compile on the very first build.
set(PC_PROFILER_SYMTAB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/symtab_data.c)
if(NOT EXISTS ${PC_PROFILER_SYMTAB_SRC})
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/generated)
    file(WRITE ${PC_PROFILER_SYMTAB_SRC}
        "#include \"pc_profiler.h\"\n"
        "const struct symtab_entry pc_profiler_symtab[] = { { 0x0u, 0x0u, \"(build twice)\" } };\n"
        "const unsigned int pc_profiler_symtab_count = 0;\n")
endif()

# src/generated/symtab_data.c is generated from THIS build's own zephyr.elf,
# for use by the NEXT build (it's data-only, so relinking it alone doesn't
# move any other object's .text addresses). After any source change, build
# twice: pass 1 links normally and regenerates symtab_data.c from the fresh
# .text layout; pass 2 recompiles only that one file so the flashed binary's
# baked-in symbol table matches itself exactly. See docs/architecture.md.
#
# This can't be an add_custom_command(TARGET zephyr_final POST_BUILD ...):
# that attachment is only allowed from the directory that created the
# target, and zephyr_final is created deep inside find_package(Zephyr)'s
# own CMakeLists.txt, not this one. It also can't be an OUTPUT-based
# add_custom_command() naming symtab_data.c: since that same path is a
# compiled source of `app` (above), CMake would mark it GENERATED and wire
# an automatic dependency back onto the command that (transitively)
# depends on zephyr_final -- which itself depends on `app` -- a cycle.
# A plain ALL custom target ordered after zephyr_final via
# add_dependencies() avoids both problems: no directory restriction, and
# no GENERATED-file dependency loop, at the cost of always being
# considered out-of-date (it just reruns, cheaply, every build).
add_custom_target(pc_profiler_gen_symtab ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_symtab.py
            --nm  ${CMAKE_NM}
            --elf $<TARGET_FILE:zephyr_final>
            --out ${PC_PROFILER_SYMTAB_SRC}
    COMMENT "pc_profiler: regenerating symtab_data.c from zephyr.elf (used by next build)"
    VERBATIM
)
add_dependencies(pc_profiler_gen_symtab zephyr_final)
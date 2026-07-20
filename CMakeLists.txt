cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Projet1)

target_sources(app PRIVATE
    src/main.c
    src/system_overview.c
    src/rtos_tasks.c
    src/events_logs.c
    src/live_variables.c
    src/launcher.c
    src/splash.c
    src/images/img_image1.c
    src/images/img_image2.c
)
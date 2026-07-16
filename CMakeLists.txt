cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Projet1)

target_sources(app PRIVATE
    src/main.c
    src/app_calculator.c
    src/app_alarm.c
    src/launcher.c
)
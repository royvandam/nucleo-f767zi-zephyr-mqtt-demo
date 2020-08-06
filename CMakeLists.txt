cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pcu)

FILE(GLOB app_sources src/*.c src/*.cpp src/*.h)
target_sources(app PRIVATE ${app_sources})

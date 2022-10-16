cmake_minimum_required(VERSION 3.10)

project(emulator)
set (CMAKE_CXX_STANDARD 23)

add_executable(emulator main.cpp e_asm.h e_base.h)
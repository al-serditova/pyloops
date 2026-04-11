cmake_minimum_required(VERSION 3.12)
project(pyloops_module C)

find_package(Python3 REQUIRED COMPONENTS Development)

add_library(pyloops MODULE pyloops.c)

target_include_directories(pyloops PRIVATE ${Python3_INCLUDE_DIRS})
target_link_libraries(pyloops PRIVATE ${Python3_LIBRARIES})

# Убираем префикс lib
set_target_properties(pyloops PROPERTIES PREFIX "")

# Чтобы было pyloops.so (без version suffix)
set_target_properties(pyloops PROPERTIES SUFFIX ".so")
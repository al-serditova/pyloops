cmake_minimum_required(VERSION 3.12)
project(pyloops_module)

set(CMAKE_CXX_STANDARD 17)

find_package(Python3 REQUIRED COMPONENTS Development)

# DUBUG: hardcoded link
include_directories("/home/vtdrs/work/loops/include/")

add_library(pyloops MODULE pyloops.cpp)

target_include_directories(pyloops PRIVATE ${Python3_INCLUDE_DIRS})
target_link_libraries(pyloops PRIVATE ${Python3_LIBRARIES})
# DUBUG: hardcoded link
target_link_libraries(pyloops PRIVATE "/home/vtdrs/work/loops/build/libloops.a")

# Убираем префикс lib
set_target_properties(pyloops PROPERTIES PREFIX "")

# Чтобы было pyloops.so (без version suffix)
set_target_properties(pyloops PROPERTIES SUFFIX ".so")
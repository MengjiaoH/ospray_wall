cmake_minimum_required(VERSION 2.8)

set(SHARED_LIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(SHARED_LIB_INCLUDE_DIR ${SHARED_LIB_INCLUDE_DIR} PARENT_SCOPE)

include_directories(${SHARED_LIB_INCLUDE_DIR})

set(IMPORTER ./importer.cpp)
set(XML ./XML.cpp)
set(LODEPNG ./lodepng.cpp)
set(OBJLOADER ./meshloader.cpp)

add_library(shared_libs STATIC ${IMPORTER} ${XML} ${LODEPNG} ${OBJLOADER})

target_link_libraries(shared_libs ospray)
set_target_properties(shared_libs
  PROPERTIES
  CXX_STANDARD 11)

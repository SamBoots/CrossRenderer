#Cmake for all the third_party libraries, they will be compiled or collected here.
cmake_minimum_required (VERSION 3.8)

#header only so 
add_library(VMA INTERFACE)
target_include_directories(VMA INTERFACE VMA)
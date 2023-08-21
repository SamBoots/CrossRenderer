##########################################################
#  This cmakelist handles the base BB library framework  #
##########################################################
cmake_minimum_required (VERSION 3.8)

#compile Definitions.
if (WIN32)
set(PLATFORM_NAME "_WIN")
elseif(UNIX)
set(PLATFORM_NAME "_LINUX")
include_directories(/usr/include/X11)
endif ()

#Add source to this project's executable.
add_library (BBFramework STATIC
"src/Allocators/Allocators.cpp" 
"src/Allocators/BackingAllocator.cpp" 
"src/Allocators/TemporaryAllocator.cpp"
"src/Allocators/RingAllocator.cpp"
"src/OS/Program${PLATFORM_NAME}.cpp"
"src/Utils/Logger.cpp"
"src/Utils/Utils.cpp"
"src/BBThreadScheduler.cpp"
"src/BBjson.cpp"
"src/BBMain.cpp")

#Include library
target_include_directories(BBFramework PUBLIC
"include"
"include/Allocators"
"include/Allocators/BackingAllocator"
"include/Storage"
"include/Utils"
"include/OS"
)
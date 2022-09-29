# CMakeList.txt : CMake project for Allocator, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

#compile Definitions.
if (WIN32)
set(PLATFORM_NAME "_WIN")
elseif(UNIX)
set(PLATFORM_NAME "_LINUX")
include_directories(/usr/include/X11)
endif ()

#Add source to this project's executable.
add_library (BBFramework 
"include/Allocators/BackingAllocator/BackingAllocator.h" 
"include/Allocators/Allocators.h" 
 
"include/Allocators/TemporaryAllocator.h"
"include/Storage/Array.h" 
"include/Storage/Pool.h" 
"include/Storage/Hashmap.h"
"include/Storage/Slotmap.h"
"include/Storage/BBString.h"
"include/OS/OSDevice.h"
"include/Utils/Utils.h"
"include/Utils/Hash.h"
"include/Utils/Slice.h"
"include/Utils/Logger.h"
"include/Common.h"
"include/BBMemory.h" 

"src/Allocators/Allocators.cpp" 
"src/Allocators/BackingAllocator/BackingAllocator${PLATFORM_NAME}.cpp" 
"src/Allocators/TemporaryAllocator.cpp"
"src/OS/OSDevice${PLATFORM_NAME}.cpp"
"src/Utils/Logger.cpp"
"src/Utils/Utils.cpp"
"src/BBMemory.cpp" )

#Include library
target_include_directories(BBFramework PUBLIC
"include"
"include/Allocators"
"include/Allocators/BackingAllocator"
"include/Storage"
"include/Utils"
"include/OS"
)
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

# Export all Symbols by default (on Windows creates '.lib'+ '.dll').
set ( CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON )


set (BB_Public
"include/Allocators/BackingAllocator.h" 
"include/Allocators/Allocators.h" 
"include/Allocators/TemporaryAllocator.h"
"include/Storage/Array.h" 
"include/Storage/Pool.h" 
"include/Storage/GrowPool.h"
"include/Storage/Hashmap.h"
"include/Storage/Slotmap.h"
"include/Storage/BBString.h"
"include/Utils/Utils.h"
"include/Utils/Hash.h"
"include/Utils/Slice.h"
"include/Utils/Logger.h"
"include/OS/Program.h" 
"include/Common.h"
"include/BBMemory.h" 
"include/BBMain.h")

set (BB_Source
"include/BBGlobal.h" 
"src/Allocators/Allocators.cpp" 
"src/Allocators/BackingAllocator.cpp" 
"src/Allocators/TemporaryAllocator.cpp"
"src/Allocators/RingAllocator.cpp"
"src/OS/Program${PLATFORM_NAME}.cpp"
"src/Utils/Logger.cpp"
"src/Utils/Utils.cpp"
"src/BBMemory.cpp"
"src/BBMain.cpp")


#Add source to this project's executable.
add_library (BBFramework SHARED ${BB_Source})

#Executable-Object
set_target_properties (BBFramework PROPERTIES
	PUBLIC_HEADER "${BB_Public}
	include/Allocators/RingAllocator.h" #bro I hate cmake, why does this work but not putting it in BB_Public?!
	RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:Renderer>
)

#Include library
target_include_directories(BBFramework PUBLIC
"include"
"include/Allocators"
"include/Allocators/BackingAllocator"
"include/Storage"
"include/Utils"
"include/OS"
)

install(TARGETS BBFramework
DESTINATION $<TARGET_FILE_DIR:Renderer>)
# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)
project ("CrossRenderer")
set (CMAKE_CXX_STANDARD 17)

if (PROCESSOR_ARCHITECTURE EQUAL X86)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CPU_ARCHITECTURE "x64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(CPU_ARCHITECTURE "x86")
    endif()
elseif(PROCESSOR_ARCHITECTURE EQUAL ARM64)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CPU_ARCHITECTURE "arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(CPU_ARCHITECTURE "arm")
    endif()
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_definitions(PUBLIC _64BIT)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_compile_definitions(PUBLIC _32BIT)
endif()

if (MSVC)
    # warning level 4 and all warnings as errors
    add_compile_definitions(PUBLIC /W4)
elseif(CMAKE_COMPILER_IS_GNUCXX)
    # lots of warnings and all warnings as errors
    add_compile_definitions(PUBLIC Wall Wextra)
endif()

#compile Definitions.
if (WIN32)
add_compile_definitions(PUBLIC _WIN32)
elseif(UNIX)
add_compile_definitions(PUBLIC _LINUX)
endif ()

if(WIN32)
elseif(UNIX)
link_libraries(X11)
endif()

add_subdirectory ("BB")
add_subdirectory ("Libs")
add_subdirectory ("Renderer")

message(STATUS "Building binaries at")
message(STATUS ${CMAKE_BINARY_DIR})

#Copy over all the resources to the main directory.
file(COPY "${CMAKE_SOURCE_DIR}/Resources" DESTINATION ${CMAKE_BINARY_DIR})
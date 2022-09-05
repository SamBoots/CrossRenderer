# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)
project ("BB")
set (CMAKE_CXX_STANDARD 17)

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

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_definitions(PUBLIC _64BIT)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_compile_definitions(PUBLIC _32BIT)
endif()

if(WIN32)
elseif(UNIX)
link_libraries(X11)
endif()

# Include sub-projects.
add_subdirectory ("Framework")
add_subdirectory ("UnitTests")
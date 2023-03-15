#######################################################
#  This cmakelist handles the BB framework unit test  #
#######################################################
cmake_minimum_required (VERSION 3.8)

#Google Test unit tests
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Google Test Project
# enable_testing()

add_executable (Unittest_Project
"Main.cpp"
"TestValues.h"
"Framework/Allocators_UTEST.h"
"Framework/Array_UTEST.h"
"Framework/Pool_UTEST.h" 
"Framework/Hashmap_UTEST.h"
"Framework/MemoryArena_UTEST.h"
"Framework/Slice_UTEST.h"
"Framework/Slotmap_UTEST.h"
"Framework/String_UTEST.h" 
"Framework/MemoryOperations_UTEST.h" 
"Framework/FileReadWrite_UTEST.h")

include_directories(
"../Framework/include"
)
target_link_libraries(Unittest_Project BBFramework gtest_main)

#Executable-Object
set_target_properties (Unittest_Project PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}>)
#include(googletest)
#gtest_discover_tests(Unit_Test_Proj)
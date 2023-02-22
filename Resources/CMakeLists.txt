# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

#copy shaders when a change happened.
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Resources/Shaders
    ${CMAKE_BINARY_DIR}/Resources/Shaders
    COMMENT "Copying Shaders")

add_dependencies(Renderer copy_shaders)

    #copy models when a change happened.
add_custom_target(copy_models ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Resources/Models
    ${CMAKE_BINARY_DIR}/Resources/Models
    COMMENT "Copying models")

    add_dependencies(Renderer copy_models)
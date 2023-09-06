#####################################################
#  This cmakelist handles the assets of the engine  #
#####################################################
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

#copy models when a change happened.
add_custom_target(copy_textures ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Resources/Textures
    ${CMAKE_BINARY_DIR}/Resources/Textures
    COMMENT "Copying Textures")

add_dependencies(Renderer copy_textures)

#copy json files when a change happened.
add_custom_target(copy_json ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Resources/Json
    ${CMAKE_BINARY_DIR}/Resources/Json
    COMMENT "Copying Json files")

add_dependencies(Unittest_Project copy_json)
add_dependencies(Renderer copy_json)
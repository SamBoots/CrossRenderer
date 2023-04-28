#######################################################
#  This cmakelist handles the IMGUI library building  #
#######################################################

#  The imgui impl for the platform is in Renderer under include/src as imgui_impl_CrossRenderer  #

add_library(IMGUI STATIC
    "imgui.cpp"
    "imgui_draw.cpp"
    "imgui_demo.cpp"
    "imgui_widgets.cpp"
	"imgui_tables.cpp")

target_include_directories(IMGUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
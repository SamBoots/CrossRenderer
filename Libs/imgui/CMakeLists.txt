#######################################################
#  This cmakelist handles the IMGUI library building  #
#######################################################

if (WIN32)
set(PLATFORM_NAME "_win32")
elseif(UNIX)
message(ERROR "Unsupported IMGUI build! We only support Windows currently")
else()
message(ERROR "Unsupported IMGUI build! We only support Windows currently")
endif()


add_library(IMGUI STATIC
    "imgui.cpp"
    "imgui_draw.cpp"
    "imgui_demo.cpp"
    "imgui_widgets.cpp"
	"imgui_tables.cpp"
    "backends/imgui_impl${PLATFORM_NAME}.cpp")

target_include_directories(IMGUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
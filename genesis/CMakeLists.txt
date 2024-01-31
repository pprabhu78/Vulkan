file(GLOB GENESIS_SRC "*.cpp" "*.h" )
source_group("genesis" FILES ${GENESIS_SRC})

file(GLOB IMGUI_SOURCE "../external/imgui/*.cpp")
source_group("imgui" FILES ${IMGUI_SOURCE})

set(KTX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/ktx)
set(KTX_SOURCES
    ${KTX_DIR}/lib/texture.c
    ${KTX_DIR}/lib/hashlist.c
    ${KTX_DIR}/lib/checkheader.c
    ${KTX_DIR}/lib/swap.c
    ${KTX_DIR}/lib/memstream.c
    ${KTX_DIR}/lib/filestream.c)

source_group("ktx" FILES ${KTX_SOURCES})

set(GLFW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/glfw)
set(GLFW_SOURCES
    ${GLFW_DIR}/src/context.c
	${GLFW_DIR}/src/egl_context.c
    ${GLFW_DIR}/src/init.c
	${GLFW_DIR}/src/input.c
	${GLFW_DIR}/src/monitor.c
	${GLFW_DIR}/src/null_init.c
	${GLFW_DIR}/src/null_joystick.c
	${GLFW_DIR}/src/null_monitor.c
	${GLFW_DIR}/src/null_window.c
	${GLFW_DIR}/src/osmesa_context.c
	${GLFW_DIR}/src/vulkan.c
	${GLFW_DIR}/src/wgl_context.c
	${GLFW_DIR}/src/win32_init.c
	${GLFW_DIR}/src/win32_joystick.c
	${GLFW_DIR}/src/win32_module.c
	${GLFW_DIR}/src/win32_monitor.c
	${GLFW_DIR}/src/win32_thread.c
	${GLFW_DIR}/src/win32_time.c
	${GLFW_DIR}/src/win32_window.c
	${GLFW_DIR}/src/window.c
	${GLFW_DIR}/src/platform.c
    )

source_group("glfw" FILES ${GLFW_SOURCES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_GLFW")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLFW_WIN32")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../external/FreeImage/Source)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../external/libtiff/libtiff)

add_library(genesis STATIC ${GENESIS_SRC} ${IMGUI_SOURCE} ${KTX_SOURCES} ${GLFW_SOURCES})
if(WIN32)
    target_link_libraries(genesis ${Vulkan_LIBRARY} ${WINLIBS} 
	${CMAKE_CURRENT_SOURCE_DIR}/../external/FreeImage/Dist/x64/FreeImage.lib 
	${CMAKE_CURRENT_SOURCE_DIR}/../external/libtiff/libtiff/RelWithDebInfo/tiff.lib)
 else(WIN32)
    target_link_libraries(genesis ${Vulkan_LIBRARY} ${XCB_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif(WIN32)
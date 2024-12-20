set(GLSLC_EXECUTABLE "$ENV{VULKAN_SDK}/bin/glslc.exe")
set(GLSLANGVALIDATOR_EXECUTABLE "$ENV{VULKAN_SDK}/bin/glslangValidator.exe")

function(add_shader TARGET SHADER)
	find_program(GLSLC glslc)
	find_program(GLSLANGVALIDATOR glslangValidator)
	
	# All shaders for a sample are found here.
	set(current-shader-path "../data/shaders/glsl/${TARGET}/${SHADER}")

	set(current-output-path  "../data/shaders/glsl/${TARGET}/${SHADER}.spv")

	# Add a custom command to compile GLSL to SPIR-V.
	get_filename_component(current-output-dir ${current-output-path} DIRECTORY)
	file(MAKE_DIRECTORY ${current-output-dir})

	if ( (${SHADER} MATCHES "(.*)(\\.rchit)") OR (${SHADER} MATCHES "(.*)(\\.rmiss)") OR (${SHADER} MATCHES "(.*)(\\.rgen)") OR (${SHADER} MATCHES "(.*)(\\.task)") OR (${SHADER} MATCHES "(.*)(\\.mesh)") )

	add_custom_command(
		OUTPUT ${current-output-path}
		COMMAND ${GLSLANGVALIDATOR_EXECUTABLE} --target-env vulkan1.3 -V -o ${current-output-path} ${current-shader-path}
		DEPENDS ${current-shader-path}
		IMPLICIT_DEPENDS CXX ${current-shader-path}
		VERBATIM)
	else()
  		add_custom_command(
			OUTPUT ${current-output-path}
			COMMAND ${GLSLC_EXECUTABLE} -o ${current-output-path} ${current-shader-path}
			DEPENDS ${current-shader-path}
			IMPLICIT_DEPENDS CXX ${current-shader-path}
			VERBATIM)

  endif()

	# Make sure our native build depends on this output.
	set_source_files_properties(${current-output-path} PROPERTIES GENERATED TRUE)
	target_sources(${TARGET} PRIVATE ${current-output-path})
endfunction(add_shader)

# Function for building single example
function(buildExample EXAMPLE_NAME GLRENDERING)
	SET(EXAMPLE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME})
	message(STATUS "Generating project file for example in ${EXAMPLE_FOLDER}")
	# Main
	file(GLOB SOURCE *.cpp ${BASE_HEADERS} ${EXAMPLE_FOLDER}/*.cpp)
	SET(MAIN_CPP ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.cpp)
	if(EXISTS ${EXAMPLE_FOLDER}/main.cpp)
		SET(MAIN_CPP ${EXAMPLE_FOLDER}/main.cpp)
	ENDIF()
	if(EXISTS ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.h)
		SET(MAIN_HEADER ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.h)
	ENDIF()
	find_package(OpenMP)
	# imgui example requires additional source files
	IF(${EXAMPLE_NAME} STREQUAL "imgui")
		file(GLOB ADD_SOURCE "../external/imgui/*.cpp")
		SET(SOURCE ${SOURCE} ${ADD_SOURCE})
	ENDIF()
	# wayland requires additional source files
	IF(USE_WAYLAND_WSI)
		SET(SOURCE ${SOURCE} ${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.h ${CMAKE_BINARY_DIR}/xdg-shell-protocol.c)
	ENDIF()
	# Add optional readme / tutorial
	file(GLOB README_FILES "${EXAMPLE_FOLDER}/*.md")

	if (GLRENDERING)
		set(GLEW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/glew)
		set(GLEW_SOURCES ${GLEW_DIR}/src/glew.cpp)
		source_group("glew" FILES ${GLEW_SOURCES})
	endif(GLRENDERING)

	if(WIN32)	
		# Add shaders
		set(SHADER_DIR_GLSL "../data/shaders/glsl/${EXAMPLE_NAME}")
		set(SHADER_DIR_GLSL_COMMON "../data/shaders/glsl/common")
		# Find all shaders.

		file(GLOB vertex-shaders ${SHADER_DIR_GLSL}/*.vert)
		file(GLOB fragment-shaders ${SHADER_DIR_GLSL}/*.frag)
		file(GLOB compute-shaders ${SHADER_DIR_GLSL}/*.comp)
		file(GLOB raytracing-shaders ${SHADER_DIR_GLSL}/*.rchit ${SHADER_DIR_GLSL}/*.rgen ${SHADER_DIR_GLSL}/*.rmiss)
		file(GLOB mesh-shaders ${SHADER_DIR_GLSL}/*.task ${SHADER_DIR_GLSL}/*.mesh)
		file(GLOB other-shaders ${SHADER_DIR_GLSL}/*.glsl ${SHADER_DIR_GLSL}/*.h ${SHADER_DIR_GLSL_COMMON}/*.*)
		file(GLOB SHADERS ${SHADER_DIR_GLSL}/*.* ${SHADER_DIR_GLSL_COMMON}/*.*)

		source_group("Shaders" FILES ${vertex-shaders} ${fragment-shaders} ${compute-shaders} ${raytracing-shaders} ${mesh-shaders} ${other-shaders})
		
	if (GLRENDERING)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${GLEW_SOURCES} ${MAIN_HEADER} ${SHADERS})
	else(GLRENDERING)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS})	
	endif(GLRENDERING)
	
		target_include_directories(${EXAMPLE_NAME} PRIVATE "../genesis")

	if (GLRENDERING)
		target_include_directories(${EXAMPLE_NAME} PRIVATE ../external/glew/include)
	endif(GLRENDERING)
	
		target_link_libraries(${EXAMPLE_NAME} genesis)

		if(GLRENDERING)
			target_link_libraries(${EXAMPLE_NAME} opengl32.lib)
		endif()
		
		# Add them to the build.
		foreach(vertex-shader ${vertex-shaders})
			get_filename_component(p ${vertex-shader} NAME)
			add_shader(${EXAMPLE_NAME} ${p})
		endforeach(vertex-shader)

		foreach(fragment-shader ${fragment-shaders})
			get_filename_component(p ${fragment-shader} NAME)
			add_shader(${EXAMPLE_NAME} ${p})
		endforeach(fragment-shader)

		foreach(compute-shader ${compute-shaders})
			get_filename_component(p ${compute-shader} NAME)
			add_shader(${EXAMPLE_NAME} ${p})
		endforeach(compute-shader)
		
		foreach(raytracing-shader ${raytracing-shaders})
			get_filename_component(p ${raytracing-shader} NAME)
			add_shader(${EXAMPLE_NAME} ${p})
		endforeach(raytracing-shader)

		foreach(mesh-shader ${mesh-shaders})
			get_filename_component(p ${mesh-shader} NAME)
			add_shader(${EXAMPLE_NAME} ${p})
		endforeach(mesh-shader)
		
	else(WIN32)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS_GLSL} ${SHADERS_HLSL} ${README_FILES})
	endif(WIN32)

	set_target_properties(${EXAMPLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
	if(OpenMP_CXX_FOUND)
		target_compile_options(${EXAMPLE_NAME} PRIVATE ${OpenMP_CXX_FLAGS})
		IF(${EXAMPLE_NAME} STREQUAL "texture3d")
			if(OpenMP_CXX_FOUND)
    			target_link_libraries(${EXAMPLE_NAME} OpenMP::OpenMP_CXX)
			endif()	
		endif()
	endif()

	if (GLRENDERING)
		target_compile_definitions(${EXAMPLE_NAME} PRIVATE GLRENDERING)
		target_compile_definitions(${EXAMPLE_NAME} PRIVATE GLEW_STATIC)
	endif(GLRENDERING)
	
	if(RESOURCE_INSTALL_DIR)
		install(TARGETS ${EXAMPLE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endfunction(buildExample)

# Build all examples
function(buildExamples)
	buildExample(raytracing false)
	buildExample(meshshaders false)
	buildExample(gl_vk_interop_nv true)
	buildExample(gl_vk_interop_kh true)
endfunction(buildExamples)

buildExamples()

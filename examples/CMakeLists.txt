set(GLSLC_EXECUTABLE "$ENV{VULKAN_SDK}/bin/glslc.exe")
set(GLSLANGVALIDATOR_EXECUTABLE "$ENV{VULKAN_SDK}/bin/glslangValidator.exe")

function(add_shader TARGET SHADER)
	find_program(GLSLC glslc)
	find_program(GLSLANGVALIDATOR glslangValidator)
	
	# All shaders for a sample are found here.
	set(current-shader-path "../../data/shaders/glsl/${TARGET}/${SHADER}")

	set(current-output-path  "../../data/shaders/glsl/${TARGET}/${SHADER}.spv")

	# Add a custom command to compile GLSL to SPIR-V.
	get_filename_component(current-output-dir ${current-output-path} DIRECTORY)
	file(MAKE_DIRECTORY ${current-output-dir})

	if ( (${SHADER} MATCHES "(.*)(\\.rchit)") OR (${SHADER} MATCHES "(.*)(\\.rmiss)") OR (${SHADER} MATCHES "(.*)(\\.rgen)"))

	add_custom_command(
		OUTPUT ${current-output-path}
		COMMAND ${GLSLANGVALIDATOR_EXECUTABLE} --target-env vulkan1.2 -V -o ${current-output-path} ${current-shader-path}
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
function(buildExample EXAMPLE_NAME)
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
	if(WIN32)	
		# Add shaders
		set(SHADER_DIR_GLSL "../data/shaders/glsl/${EXAMPLE_NAME}")
		# Find all shaders.

		file(GLOB vertex-shaders ${SHADER_DIR_GLSL}/*.vert)
		file(GLOB fragment-shaders ${SHADER_DIR_GLSL}/*.frag)
		file(GLOB compute-shaders ${SHADER_DIR_GLSL}/*.comp)
		file(GLOB raytracing-shaders ${SHADER_DIR_GLSL}/*.rchit ${SHADER_DIR_GLSL}/*.rgen ${SHADER_DIR_GLSL}/*.rmiss)
		file(GLOB other-shaders ${SHADER_DIR_GLSL}/*.glsl ${SHADER_DIR_GLSL}/*.h)
		file(GLOB SHADERS ${SHADER_DIR_GLSL}/*.*)

		source_group("Shaders" FILES ${vertex-shaders} ${fragment-shaders} ${compute-shaders} ${raytracing-shaders} ${other-shaders})
		if( (${EXAMPLE_NAME} STREQUAL "tutorial") OR (${EXAMPLE_NAME} STREQUAL "tutorial_raytracing"))
			add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS})
		else()
			add_executable(${EXAMPLE_NAME} WIN32 ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS})
		endif()
		
		target_link_libraries(${EXAMPLE_NAME} base ${Vulkan_LIBRARY} ${WINLIBS})

		if( (${EXAMPLE_NAME} STREQUAL "tutorial") OR (${EXAMPLE_NAME} STREQUAL "tutorial_raytracing"))
			target_link_libraries(${EXAMPLE_NAME} genesis)
		endif()
		
		if( (${EXAMPLE_NAME} STREQUAL "tutorial") OR (${EXAMPLE_NAME} STREQUAL "tutorial_raytracing"))
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
			endforeach(compute-shader)
		endif()
	else(WIN32)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS_GLSL} ${SHADERS_HLSL} ${README_FILES})
		target_link_libraries(${EXAMPLE_NAME} base )
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

	if(RESOURCE_INSTALL_DIR)
		install(TARGETS ${EXAMPLE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endfunction(buildExample)

# Build all examples
function(buildExamples)
	foreach(EXAMPLE ${EXAMPLES})
		buildExample(${EXAMPLE})
	endforeach(EXAMPLE)
endfunction(buildExamples)

set(EXAMPLES
	bloom
	computecloth
	computecullandlod
	computeheadless
	computenbody
	computeparticles
	computeraytracing
	computeshader
	conditionalrender
	conservativeraster
	debugmarker
	deferred
	deferredmultisampling
	deferredshadows
	descriptorindexing
	descriptorsets
	displacement
	distancefieldfonts
	dynamicuniformbuffer
	gears
	geometryshader
	gltfloading
	gltfscenerendering
	gltfskinning
	hdr
	imgui
	indirectdraw
	inlineuniformblocks
	inputattachments
	instancing
	multisampling
	multithreading
	multiview
	negativeviewportheight	
	occlusionquery
	offscreen
	oit
	parallaxmapping
	particlefire
	pbrbasic
	pbribl
	pbrtexture
	pipelines
	pipelinestatistics
	pushconstants
	pushdescriptors
	radialblur
	rayquery
	raytracingbasic
	raytracingcallable
	raytracingreflections
	raytracingshadows	
	renderheadless
	screenshot
	shadowmapping
	shadowmappingomni
	shadowmappingcascade
	specializationconstants
	sphericalenvmapping
	ssao
	stencilbuffer
	subpasses
	terraintessellation
	tessellation
	textoverlay
	texture
	texture3d
	texturearray
	texturecubemap
	texturecubemaparray
	texturemipmapgen
	texturesparseresidency
	triangle
	tutorial
	tutorial_raytracing
	variablerateshading
	viewportarray
	vulkanscene
)

buildExamples()

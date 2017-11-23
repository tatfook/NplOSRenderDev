# Author: chenjinxian
# Date: 2017.11.24
# Desc: 

cmake_minimum_required(VERSION 2.6)
project (NplOSRender)


SET(NplOSRenderPlugin_Source_files "")
file (GLOB src_files ${PROJECT_SOURCE_DIR}/src/*.*)
SOURCE_GROUP("src" FILES ${src_files}) 
list(APPEND NplOSRenderPlugin_Source_files ${src_files})

include_directories("${PROJECT_SOURCE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${NPLRuntime_Absolutely_DIR}/Client/trunk/ParaEngineClient")
include_directories("${NPLRuntime_Absolutely_DIR}/Client/trunk/ParaEngineClient/Core")
include_directories("${NPLRuntime_Absolutely_DIR}/Client/trunk/ParaEngineClient/math")
include_directories("${NPLRuntime_Absolutely_DIR}/Client/trunk/ParaEngineClient/util")
IF (WIN32)
	include_directories("${BoostRoot_DIR}")
	include_directories("${MESA3D_DIR}/include")
	include_directories("${LIBPNG_DIR}/include")
ELSE()
	find_package(PNG REQUIRED)
	add_definitions(-DCMAKE_EXPECTED_PNG_VERSION="${PNG_VERSION_STRING}")
ENDIF()
include_directories("${MESA3D_DIR}/util")

IF(MSVC)
	# statically link MSVC to reduce dependancies
	foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
	 	if(${flag_var} MATCHES "/MD")
			string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
	 	endif(${flag_var} MATCHES "/MD")
	 	if(${flag_var} MATCHES "/MDd")
	 		string(REGEX REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
		endif(${flag_var} MATCHES "/MDd")
	endforeach(flag_var)
	
	# /GR- Remove RTTI to miminize the executable size
	SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GR-")

  # generate program database (PDB symbol files even in release build)
	SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
	SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zi")
ENDIF()



IF (WIN32)
	SET(CMAKE_DEBUG_POSTFIX "_d")
	link_directories(${MESA3D_DIR}/lib)
	link_directories(${LIBPNG_DIR}/lib)
ENDIF()

add_library(NplOSRender SHARED ${NplOSRenderPlugin_Source_files})

IF (WIN32)
	target_link_libraries(NplOSRender osmesa)
	target_link_libraries(NplOSRender libpng)
	target_link_libraries(NplOSRender zlib)
ELSE ()
	target_link_libraries(NplOSRender OSMesa)
	target_link_libraries(NplOSRender ${PNG_LIBRARIES})
ENDIF()

ADD_CUSTOM_COMMAND(
	TARGET NplOSRender
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_BIN_DIR}
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:NplOSRender> ${OUTPUT_BIN_DIR}
	IF (WIN32)
		COMMAND ${CMAKE_COMMAND} -E copy "${MESA3D_DIR}/lib/osmesa.dll" ${OUTPUT_BIN_DIR}
	ENDIF()
)

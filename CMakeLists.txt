# Author: chenjinxian
# Date: 2017.11.24
# Desc: 

cmake_minimum_required(VERSION 2.6)
project (NplOSRender)

add_definitions(-D_INT32)
add_definitions(-D_UINT32)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DNOMINMAX)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if (COMPILER_SUPPORTS_CXX11)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else ()
	
endif ()


set	(NPLRuntime_DIR "../NPLRuntime" CACHE STRING "")
set (NPLRuntime_Absolutely_DIR "${PROJECT_SOURCE_DIR}/${NPLRuntime_DIR}")
set (MESA3D_DIR "${PROJECT_SOURCE_DIR}/osmesa")

if (WIN32)
	set (BoostRoot_DIR "$ENV{BOOST_ROOT}")
	set (LIBPNG_DIR "${PROJECT_SOURCE_DIR}/libpng")
	set	(OUTPUT_BIN_DIR "${NPLRuntime_Absolutely_DIR}/ParaWorld/bin32/osmesa")
else()
	set	(OUTPUT_BIN_DIR "${PROJECT_SOURCE_DIR}/../NplCadOnline/workspace/osmesa")
endif()

# NplOSRender
add_subdirectory ("${PROJECT_SOURCE_DIR}/NplOSRenderPlugin")


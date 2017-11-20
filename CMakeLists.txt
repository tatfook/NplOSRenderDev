# Author: leio
# Date: 2016.11.24
# Desc: 

cmake_minimum_required(VERSION 2.6)
project (NplOSRender)

add_definitions(-D_INT32)
add_definitions(-D_UINT32)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DNOMINMAX)



set (BoostRoot_DIR "$ENV{BOOST_ROOT}")
set (MESA3D_DIR "${PROJECT_SOURCE_DIR}/osmesa")
set	(NPLRuntime_DIR "../NPLRuntime" CACHE STRING "")
set (NPLRuntime_Absolutely_DIR "${PROJECT_SOURCE_DIR}/${NPLRuntime_DIR}")

set	(OUTPUT_BIN_DIR "${NPLRuntime_Absolutely_DIR}/ParaWorld/osmesa")

# NplOSRender
add_subdirectory ("${PROJECT_SOURCE_DIR}/NplOSRenderPlugin")


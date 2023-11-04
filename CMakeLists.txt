cmake_minimum_required(VERSION 3.15)

cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

project("ecat")

set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

execute_process(
	COMMAND node -p "require('node-addon-api').include_dir"
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE NODE_ADDON_API_DIR)
string(REPLACE "\n" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})
set(NODE_ADDON_API_DIR "${CMAKE_SOURCE_DIR}/${NODE_ADDON_API_DIR}")

add_compile_options(-Wall -Wfatal-errors -std=c++20 -O2 -fexceptions -Wpedantic)
add_definitions(-DNAPI_VERSION=6)

if(DEFINED ENV{ECAT_BUILD_DEBUG})
	message(STATUS "DEBUG = $ENV{ECAT_BUILD_DEBUG}")
	add_compile_options(-g)
	add_definitions(-DDEBUG=$ENV{ECAT_BUILD_DEBUG})
endif()

set(ECHELPER_DIR "${SOURCE_DIR}/etherlab-helper")
set(ECHELPER_INC_DIR "${ECHELPER_DIR}/include")
set(ECHELPER_LIB_DIR "${ECHELPER_DIR}/lib")
set(ECHELPER_SRC_DIR "${ECHELPER_DIR}/src")
set(RAPIDJSON_DIR "${ECHELPER_LIB_DIR}/rapidjson/include")

# pthread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# lib ethercat
find_library(
	ETHERCAT_LIB
	NAMES ethercat
	HINTS "/usr/local/lib/" "/usr/lib/")

# config parser
set(CONFIGPARSER_OBJ_NAME "OBJ_CONFIGPARSER")
set(CONFIGPARSER_OBJ_LIB "$<TARGET_OBJECTS:${CONFIGPARSER_OBJ_NAME}>")
file(GLOB CONFIGPARSER_SRC_FILES "${ECHELPER_SRC_DIR}/config-parser.cpp")
add_library("${CONFIGPARSER_OBJ_NAME}" OBJECT "${CONFIGPARSER_SRC_FILES}")
target_include_directories("${CONFIGPARSER_OBJ_NAME}"
						   PRIVATE "${ECHELPER_INC_DIR}" "${RAPIDJSON_DIR}")
set_target_properties("${CONFIGPARSER_OBJ_NAME}"
					  PROPERTIES POSITION_INDEPENDENT_CODE 1)

# etherlab helper
set(ECHELPER_OBJ_NAME "OBJ_ECHELPER")
set(ECHELPER_OBJ_LIB "$<TARGET_OBJECTS:${ECHELPER_OBJ_NAME}>")
file(GLOB ECHELPER_SRC_FILES "${ECHELPER_SRC_DIR}/etherlab-helper.cpp")
add_library("${ECHELPER_OBJ_NAME}" OBJECT "${ECHELPER_SRC_FILES}")
target_include_directories("${ECHELPER_OBJ_NAME}" PRIVATE "/usr/local/include"
														  "${ECHELPER_INC_DIR}")
set_target_properties("${ECHELPER_OBJ_NAME}"
					  PROPERTIES POSITION_INDEPENDENT_CODE 1)

# ecat.node
file(GLOB SOURCE_FILES "${SOURCE_DIR}/api.cc" "${CONFIGPARSER_OBJ_LIB}"
	 "${ECHELPER_OBJ_NAME}")
add_library(
	"${PROJECT_NAME}" SHARED "${SOURCE_FILES}" "${CMAKE_JS_SRC}"
							 "${ECHELPER_OBJ_LIB}" "${CONFIGPARSER_OBJ_LIB}")
set_target_properties("${PROJECT_NAME}" PROPERTIES POSITION_INDEPENDENT_CODE 1)
set_target_properties("${PROJECT_NAME}" PROPERTIES PREFIX "" SUFFIX ".node")
target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE "${CMAKE_JS_INC}" "${NODE_ADDON_API_DIR}" "/usr/local/include"
			"${NAPI_INCLUDE_DIR}" "${ECHELPER_INC_DIR}")
target_link_libraries(
	"${PROJECT_NAME}" PRIVATE Threads::Threads "${CMAKE_JS_LIB}"
							  "${ETHERCAT_LIB}")

project(pilight_protocols_ir)

file(REMOVE "protocol_header.h")
file(REMOVE "protocol_init.h")

if(NOT WIN32)
	file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
	file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h")

	include(CMakeExclude.txt)
	include(CMakeInclude.txt)

	set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")
endif()
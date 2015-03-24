project(pilight_protocols_network)

file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h")

include(CMakeExclude.txt)

if(WIN32)
	list(REMOVE_ITEM ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/arping.c")
	list(REMOVE_ITEM ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/arping.h")
endif()

include(CMakeInclude.txt)

set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")

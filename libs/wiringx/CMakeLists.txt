project(wiringx)

if(WIN32 OR ${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
	file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/wiringX.c")
	file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/wiringX.h")
else()
	file(GLOB_RECURSE ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
	file(GLOB_RECURSE ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h")
endif()

set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")

project(wiringx)

if(WIN32)
	file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/wiringX.c")
	file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/wiringX.h")
else()
	file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
	file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h")

	list(REMOVE_ITEM ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/radxa.c")
endif()

set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")

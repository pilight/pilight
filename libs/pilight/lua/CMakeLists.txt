project(pilight_lua)

set(${PROJECT_NAME}_files "" CACHE INTERNAL "")

add_subdirectory(devices)
add_subdirectory(network)

file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h")

set(${PROJECT_NAME}_files
	${${PROJECT_NAME}_sources}
	${${PROJECT_NAME}_headers}
	${${PROJECT_NAME}_files}
	${${PROJECT_NAME}_devices_files}
	${${PROJECT_NAME}_network_files}
CACHE INTERNAL "")
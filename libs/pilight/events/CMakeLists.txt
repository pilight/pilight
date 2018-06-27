project(pilight_events)

add_subdirectory(actions)
add_subdirectory(functions)
add_subdirectory(operators)

file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h")

set(${PROJECT_NAME}_files 
	${${PROJECT_NAME}_sources}
	${${PROJECT_NAME}_headers}
CACHE INTERNAL "")
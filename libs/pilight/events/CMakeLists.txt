project(pilight_events)

add_subdirectory(actions)
add_subdirectory(operators)
add_subdirectory(functions)

file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h")

set(${PROJECT_NAME}_files 
	${pilight_event_actions_files}
	${pilight_event_functions_files}
	${pilight_event_operators_files}
	${${PROJECT_NAME}_sources}
	${${PROJECT_NAME}_headers}
CACHE INTERNAL "")
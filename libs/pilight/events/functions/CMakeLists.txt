project(pilight_event_functions)

file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h")

if(${MODULESPACK} MATCHES "OFF" OR WIN32)
	include(CMakeInclude.txt)

	set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")
else()
	install(FILES ${functions} DESTINATION pilight/functions/ COMPONENT ${PROJECT_NAME})
endif()

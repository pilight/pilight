project(pilight_hardware)

file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h")

if(${MODULESPACK} MATCHES "OFF" OR WIN32)
	set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")
endif()

file(GLOB hardware "${PROJECT_SOURCE_DIR}/*.lua")

if(WIN32)
	install(FILES ${hardware} DESTINATION hardware COMPONENT pilight)
else()
	install(FILES ${hardware} DESTINATION lib/pilight/hardware COMPONENT pilight)
endif()
project(pilight_config)

file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h")

set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")

SUBDIRLIST(storage ${PROJECT_SOURCE_DIR})

if(WIN32)
	install(DIRECTORY ${storage} DESTINATION storage COMPONENT pilight)
else()
	install(DIRECTORY ${storage} DESTINATION lib/pilight/storage COMPONENT pilight)
endif()
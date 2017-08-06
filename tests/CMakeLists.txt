project(tests)

include(../CMakeConfig.txt)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose)

file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h" "${PROJECT_SOURCE_DIR}/protocols/*.h")

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR WIN32)
	list(REMOVE_ITEM ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/protocols_i2c.c")
	list(REMOVE_ITEM ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/i2c-dev.h")
endif()

list(REMOVE_ITEM ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/libgpio.c")
list(REMOVE_ITEM ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/libdso.c")

set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")
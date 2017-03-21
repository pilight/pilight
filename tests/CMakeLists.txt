project(tests)

include(../CMakeConfig.txt)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose)

file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h" "${PROJECT_SOURCE_DIR}/protocols/*.h")

set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")

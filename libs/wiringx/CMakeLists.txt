project(wiringx)

message("Entering ${PROJECT_SOURCE_DIR}")

file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h")

list(REMOVE_ITEM ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/radxa.c")

include_directories(${CROSS_COMPILE_INCLUDE})
include_directories(/usr/local/include/)

set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")

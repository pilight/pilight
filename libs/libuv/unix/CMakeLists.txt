project(libuv)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PROJECT_SOURCE_DIR}/include/")

file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/*.c")
file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/*.h")

set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")

project(wiringx)

if(WIN32)
	file(GLOB ${PROJECT_NAME}_sources "${PROJECT_SOURCE_DIR}/wiringX.c")
	file(GLOB ${PROJECT_NAME}_headers "${PROJECT_SOURCE_DIR}/wiringX.h")
else()
	file(GLOB ${PROJECT_NAME}_headers
		"${PROJECT_SOURCE_DIR}/*.h"
		"${PROJECT_SOURCE_DIR}/platform/*.h"
		"${PROJECT_SOURCE_DIR}/platform/linksprite/*.h"
		"${PROJECT_SOURCE_DIR}/platform/lemaker/*.h"
		"${PROJECT_SOURCE_DIR}/platform/solidrun/*.h"
		"${PROJECT_SOURCE_DIR}/platform/raspberrypi/*.h"
		"${PROJECT_SOURCE_DIR}/platform/hardkernel/*.h"
		"${PROJECT_SOURCE_DIR}/platform/xunlong/*.h"
		"${PROJECT_SOURCE_DIR}/soc/*.h"
		"${PROJECT_SOURCE_DIR}/soc/allwinner/*.h"
		"${PROJECT_SOURCE_DIR}/soc/nxp/*.h"
		"${PROJECT_SOURCE_DIR}/soc/broadcom/*.h"
		"${PROJECT_SOURCE_DIR}/soc/amlogic/*.h"
		"${PROJECT_SOURCE_DIR}/soc/samsung/*.h"
		"${PROJECT_SOURCE_DIR}/soc/samsung/*.h"
	)
	file(GLOB ${PROJECT_NAME}_sources
		"${PROJECT_SOURCE_DIR}/*.c"
		"${PROJECT_SOURCE_DIR}/platform/*.c"
		"${PROJECT_SOURCE_DIR}/platform/linksprite/*.c"
		"${PROJECT_SOURCE_DIR}/platform/lemaker/*.c"
		"${PROJECT_SOURCE_DIR}/platform/solidrun/*.c"
		"${PROJECT_SOURCE_DIR}/platform/raspberrypi/*.c"
		"${PROJECT_SOURCE_DIR}/platform/hardkernel/*.c"
		"${PROJECT_SOURCE_DIR}/platform/xunlong/*.c"
		"${PROJECT_SOURCE_DIR}/soc/*.c"
		"${PROJECT_SOURCE_DIR}/soc/allwinner/*.c"
		"${PROJECT_SOURCE_DIR}/soc/nxp/*.c"
		"${PROJECT_SOURCE_DIR}/soc/broadcom/*.c"
		"${PROJECT_SOURCE_DIR}/soc/amlogic/*.c"
		"${PROJECT_SOURCE_DIR}/soc/samsung/*.c"
	)
endif()

set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")

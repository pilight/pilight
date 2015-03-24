project(pthreadw32)

set(${PROJECT_NAME}_sources
		"${PROJECT_SOURCE_DIR}/attr.c"
		"${PROJECT_SOURCE_DIR}/barrier.c"
		"${PROJECT_SOURCE_DIR}/cancel.c"
		"${PROJECT_SOURCE_DIR}/cleanup.c"
		"${PROJECT_SOURCE_DIR}/condvar.c"
		"${PROJECT_SOURCE_DIR}/create.c"
		"${PROJECT_SOURCE_DIR}/dll.c"
		"${PROJECT_SOURCE_DIR}/errno.c"
		"${PROJECT_SOURCE_DIR}/exit.c"
		"${PROJECT_SOURCE_DIR}/fork.c"
		"${PROJECT_SOURCE_DIR}/global.c"
		"${PROJECT_SOURCE_DIR}/misc.c"
		"${PROJECT_SOURCE_DIR}/mutex.c"
		"${PROJECT_SOURCE_DIR}/nonportable.c"
		"${PROJECT_SOURCE_DIR}/private.c"
		"${PROJECT_SOURCE_DIR}/sched.c"
		"${PROJECT_SOURCE_DIR}/semaphore.c"
		"${PROJECT_SOURCE_DIR}/sync.c"
		"${PROJECT_SOURCE_DIR}/tsd.c"
)

set(${PROJECT_NAME}_headers
		"${PROJECT_SOURCE_DIR}/config.h"
		"${PROJECT_SOURCE_DIR}/context.h"
		"${PROJECT_SOURCE_DIR}/implement.h"
		"${PROJECT_SOURCE_DIR}/need_errno.h"
		"${PROJECT_SOURCE_DIR}/pthread.h"
		"${PROJECT_SOURCE_DIR}/sched.h"
		"${PROJECT_SOURCE_DIR}/semaphore.h"
)

set(${PROJECT_NAME}_files ${${PROJECT_NAME}_sources} ${${PROJECT_NAME}_headers} CACHE INTERNAL "")

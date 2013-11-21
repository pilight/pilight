cmake_minimum_required(VERSION 2.8.8)

project(pilight C)

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "armv6")
	set(CMAKE_C_FLAGS "-Ofast -mfloat-abi=hard -mfpu=vfp -march=armv6 -ffast-math -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wformat=2 -g -Wall -Wconversion -Wunreachable-code -Wstrict-prototypes")
endif()
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
	set(CMAKE_C_FLAGS "-Ofast -fPIC -march=native -mtune=native -mfpmath=sse -ffast-math -Wconversion -Wunreachable-code -Wstrict-prototypes -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wformat=2 -g -Wall")	
endif()

# Start uninstaller generator
function(WRITE_UNINSTALL_TARGET_SCRIPT)
    # Create uninstall target template file, if it doesn't exist...
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in)
        set(__uninstall_filename ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake.in)
        # BEGIN actual write to file...
        file(WRITE ${__uninstall_filename} "\# - uninstall target template\n\#")
        file(APPEND ${__uninstall_filename} "
if (NOT EXISTS \"\@CMAKE_CURRENT_BINARY_DIR\@/install_manifest.txt\")
    message(FATAL_ERROR \"Cannot find install manifest: \\\"\@CMAKE_CURRENT_BINARY_DIR\@/install_manifest.txt\\\"\")
endif(NOT EXISTS \"\@CMAKE_CURRENT_BINARY_DIR\@/install_manifest.txt\")

file(READ \"\@CMAKE_CURRENT_BINARY_DIR\@/install_manifest.txt\" files)
string(REGEX REPLACE \"\\n\" \";\" files \"\${files}\")

foreach (file \${files})
    message(STATUS \"Uninstalling \\\"\$ENV{DESTDIR}\${file}\\\"\")
    if (EXISTS \"\$ENV{DESTDIR}\${file}\")
        execute_process(
            COMMAND \@CMAKE_COMMAND\@ -E remove \"\$ENV{DESTDIR}\${file}\"
            OUTPUT_VARIABLE rm_out
            RESULT_VARIABLE rm_retval
        )
        if(NOT \${rm_retval} EQUAL 0)
            message(FATAL_ERROR \"Problem when removing \\\"\$ENV{DESTDIR}\${file}\\\"\")
        endif (NOT \${rm_retval} EQUAL 0)
    else (EXISTS \"\$ENV{DESTDIR}\${file}\")
        message(STATUS \"File \\\"\$ENV{DESTDIR}\${file}\\\" does not exist.\")
    endif (EXISTS \"\$ENV{DESTDIR}\${file}\")
endforeach(file)

execute_process(COMMAND update-rc.d pilight remove)
execute_process(COMMAND ln -sf /usr/local/lib/pilight/libpilight.so.2 /usr/local/lib/libpilight.so)
execute_process(COMMAND ldconfig)

") # END of appending to file...
    endif()
endfunction()
# End of the uninstaller generator

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake_postinstall.cmake)
	set(__uninstall_filename ${CMAKE_CURRENT_BINARY_DIR}/cmake_postinstall.cmake)
	# BEGIN actual write to file...
	file(WRITE ${__uninstall_filename} "execute_process(COMMAND update-rc.d pilight defaults)")
endif()

include(${PROJECT_SOURCE_DIR}/CMakeConfig.txt)

execute_process(COMMAND git describe --always
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} 
	RESULT_VARIABLE git_result
	OUTPUT_VARIABLE git_ver)

STRING(REGEX REPLACE "\n" "" git_ver "${git_ver}")
add_definitions(-DHASH="${git_ver}")

include_directories("${PROJECT_SOURCE_DIR}/libs/pilight/")

file(GLOB pilight_headers
	"${PROJECT_SOURCE_DIR}/libs/hardwares/*.h" 
	"${PROJECT_SOURCE_DIR}/libs/pilight/*.h"
	"${PROJECT_SOURCE_DIR}/libs/protocols/*.h" 
	"${PROJECT_SOURCE_DIR}/libs/websockets/*.h")
	
file(GLOB pilight
	"${PROJECT_SOURCE_DIR}/libs/hardwares/*.c" 
	"${PROJECT_SOURCE_DIR}/libs/pilight/*.c"
	"${PROJECT_SOURCE_DIR}/libs/protocols/*.c" 
	"${PROJECT_SOURCE_DIR}/libs/websockets/*.c")

if(${PROTOCOL_ALECTO} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/alecto.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/alecto.c")
endif()

if(${PROTOCOL_COGEX_SWITCH} MATCHES "OFF" 
	AND ${PROTOCOL_KAKU_SWITCH_OLD} MATCHES "OFF" 
	AND ${PROTOCOL_INTERTECHNO_OLD} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/arctech_switch_old.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/arctech_switch_old.c")
endif()

if(${PROTOCOL_KAKU_SCREEN_OLD} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/arctech_screen_old.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/arctech_screen_old.c")
endif()

if(${PROTOCOL_KAKU_DIMMER} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/arctech_dimmer.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/arctech_dimmer.c")
endif()

if(${PROTOCOL_KAKU_SCREEN} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/arctech_screen.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/arctech_screen.c")
endif()

if(${PROTOCOL_COCO_SWITCH} MATCHES "OFF"
	AND ${PROTOCOL_DIO_SWITCH} MATCHES "OFF"
	AND ${PROTOCOL_NEXA_SWITCH} MATCHES "OFF"
	AND ${PROTOCOL_KAKU_SWITCH} MATCHES "OFF"
	AND ${PROTOCOL_INTERTECHNO_SWITCH} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/arctech_switch.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/arctech_switch.c")	
endif()

if(${PROTOCOL_DS18B20} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/ds18b20.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/ds18b20.c")
endif()

if(${PROTOCOL_DHT22} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/dht22.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/dht22.c")
endif()

if(${PROTOCOL_ELRO_SWITCH} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/elro_he.h")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/elro_hc.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/elro_hc.c")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/elro_he.c")
endif()

if(${PROTOCOL_GENERIC_DIMMER} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/generic_dimmer.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/generic_dimmer.c")
endif()

if(${PROTOCOL_GENERIC_SWITCH} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/generic_switch.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/generic_switch.c")
endif()

if(${PROTOCOL_GENERIC_WEATHER} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/generic_weather.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/generic_weather.c")
endif()

if(${PROTOCOL_HOMEEASY_OLD} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/home_easy_old.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/home_easy_old.c")
endif()

if(${PROTOCOL_IMPULS} MATCHES "OFF"
	AND ${PROTOCOL_SELECTREMOTE} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/impuls.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/impuls.c")
endif()

if(${PROTOCOL_RAW} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/raw.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/raw.c")
endif()

if(${PROTOCOL_RELAY} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/relay.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/relay.c")
endif()

if(${PROTOCOL_CHACON} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/chacon.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/chacon.c")
endif()

if(${PROTOCOL_CLARUS} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/protocols/clarus.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/protocols/clarus.c")
endif()

if(${HARDWARE_433_GPIO} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/hardwares/gpio.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/hardwares/gpio.c")
endif()

if(${HARDWARE_433_MODULE} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/hardwares/module.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/hardwares/module.c")
endif()

if(${UPDATE} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/pilight/update.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/pilight/update.c")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/pilight/http_lib.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/pilight/http_lib.c")	
endif()
	
if(${WEBSERVER} MATCHES "ON")
	file(GLOB web
		"${PROJECT_SOURCE_DIR}/web/*"
	)
endif()

file(GLOB webserver_sources "${PROJECT_SOURCE_DIR}/libs/websockets/*.c")

if(${WEBSERVER} MATCHES "OFF")
	list(REMOVE_ITEM pilight_headers "${PROJECT_SOURCE_DIR}/libs/pilight/webserver.h")
	list(REMOVE_ITEM pilight "${PROJECT_SOURCE_DIR}/libs/pilight/webserver.c")
	list(REMOVE_ITEM pilight ${webserver_sources})
	file(GLOB webserver_headers "${PROJECT_SOURCE_DIR}/libs/websockets/*.h")	
	list(REMOVE_ITEM pilight ${webserver_headers})
endif()
	
find_package(Threads REQUIRED)

if(${UPDATE} MATCHES "ON")
	find_package(ZLIB REQUIRED)
	file(GLOB CMAKE_ZLIB_LIBS_INIT "/usr/lib/arm-linux-gnueabihf/libz.so")
endif()

configure_file(pilight.h.in pilight.h)

add_library(pilight OBJECT ${pilight})
add_library(pilight_static STATIC $<TARGET_OBJECTS:pilight>)
add_library(pilight_shared SHARED $<TARGET_OBJECTS:pilight>)
target_link_libraries(pilight_shared ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(pilight_static ${CMAKE_THREAD_LIBS_INIT})

if(${UPDATE} MATCHES "ON")
	target_link_libraries(pilight_shared ${CMAKE_ZLIB_LIBS_INIT})
	target_link_libraries(pilight_static ${CMAKE_ZLIB_LIBS_INIT})
endif()

set_target_properties(pilight_shared pilight_static PROPERTIES OUTPUT_NAME pilight)

set_source_files_properties(${PROJECT_SOURCE_DIR}/libs/pilight/wiringPi.c PROPERTIES COMPILE_FLAGS " -w ")
set_source_files_properties(${PROJECT_SOURCE_DIR}/libs/pilight/json.c PROPERTIES COMPILE_FLAGS " -w ")
set_source_files_properties(${PROJECT_SOURCE_DIR}/libs/pilight/http_lib.c PROPERTIES COMPILE_FLAGS " -w ")
set_source_files_properties(${webserver_sources} PROPERTIES COMPILE_FLAGS " -w ")

install(FILES libpilight.so DESTINATION lib/pilight/ RENAME libpilight.so.2)
install(FILES libpilight.a DESTINATION lib/pilight/ RENAME libpilight.a)

if(${WEBSERVER} MATCHES "ON")
	install(FILES ${web} DESTINATION /usr/local/share/pilight/)
endif()

install(PROGRAMS init/pilight DESTINATION /etc/init.d/)

if(NOT EXISTS "/etc/pilight/settings.json")
	install(FILES settings.json-default DESTINATION /etc/pilight/ RENAME settings.json) 
endif()

add_executable(pilight-daemon daemon.c)
target_link_libraries(pilight-daemon pilight_shared)
target_link_libraries(pilight-daemon ${CMAKE_DL_LIBS})
target_link_libraries(pilight-daemon ${CMAKE_THREAD_LIBS_INIT})

if(${UPDATE} MATCHES "ON")
	target_link_libraries(pilight-daemon ${CMAKE_ZLIB_LIBS_INIT})
endif()

add_executable(pilight-raw raw.c)
target_link_libraries(pilight-raw pilight_shared)
target_link_libraries(pilight-raw ${CMAKE_DL_LIBS})
target_link_libraries(pilight-raw ${CMAKE_THREAD_LIBS_INIT})

add_executable(pilight-send send.c)
target_link_libraries(pilight-send pilight_shared)
target_link_libraries(pilight-send ${CMAKE_DL_LIBS})
target_link_libraries(pilight-send ${CMAKE_THREAD_LIBS_INIT})

add_executable(pilight-control control.c)
target_link_libraries(pilight-control pilight_shared)
target_link_libraries(pilight-control ${CMAKE_DL_LIBS})
target_link_libraries(pilight-control ${CMAKE_THREAD_LIBS_INIT})

add_executable(pilight-learn learn.c)
target_link_libraries(pilight-learn pilight_shared)
target_link_libraries(pilight-learn m)
target_link_libraries(pilight-learn ${CMAKE_DL_LIBS})
target_link_libraries(pilight-learn ${CMAKE_THREAD_LIBS_INIT})

add_executable(pilight-receive receive.c)
target_link_libraries(pilight-receive pilight_shared)
target_link_libraries(pilight-receive ${CMAKE_DL_LIBS})
target_link_libraries(pilight-receive ${CMAKE_THREAD_LIBS_INIT})

add_executable(pilight-debug debug.c)
target_link_libraries(pilight-debug pilight_shared)
target_link_libraries(pilight-debug ${CMAKE_DL_LIBS})
target_link_libraries(pilight-debug m)
target_link_libraries(pilight-debug ${CMAKE_THREAD_LIBS_INIT})

install(PROGRAMS pilight-daemon DESTINATION sbin)
install(PROGRAMS pilight-raw DESTINATION sbin)
install(PROGRAMS pilight-send DESTINATION sbin)
install(PROGRAMS pilight-control DESTINATION sbin)
install(PROGRAMS pilight-learn DESTINATION sbin)
install(PROGRAMS pilight-receive DESTINATION sbin)
install(PROGRAMS pilight-debug DESTINATION sbin)
install(SCRIPT cmake_postinstall.cmake)

WRITE_UNINSTALL_TARGET_SCRIPT()
configure_file("${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

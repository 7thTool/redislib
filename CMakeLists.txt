CMAKE_MINIMUM_REQUIRED(VERSION 3.4.0)
INCLUDE(GNUInstallDirs)
PROJECT(redislib)

MACRO(getVersionBit name)
  SET(VERSION_REGEX "^#define ${name} (.+)$")
  FILE(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/redis.h"
    VERSION_BIT REGEX ${VERSION_REGEX})
  STRING(REGEX REPLACE ${VERSION_REGEX} "\\1" ${name} "${VERSION_BIT}")
ENDMACRO(getVersionBit)

getVersionBit(REDIS_MAJOR)
getVersionBit(REDIS_MINOR)
getVersionBit(REDIS_PATCH)
getVersionBit(REDIS_SONAME)
SET(VERSION "${REDIS_MAJOR}.${REDIS_MINOR}.${REDIS_PATCH}")
MESSAGE("Detected version: ${VERSION}")

PROJECT(redislib VERSION "${VERSION}")

SET(ENABLE_EXAMPLES OFF CACHE BOOL "Enable building redis examples")

ADD_DEFINITIONS(-std=c11)

INCLUDE_DIRECTORIES(../deps/jemalloc ../deps/linenoise ../deps/lua  ../src .)
LINK_DIRECTORIES(../deps/jemalloc/lib)

ADD_LIBRARY(redis SHARED
    ../src/sds.c
    ../src/adlist.c 
    ../src/quicklist.c
    ../src/ziplist.c
    ../src/zipmap.c
    ../src/intset.c
    ../src/dict.c  
    ../src/crc64.c
    ../src/crc16.c
    ../src/sha1.c
    ../src/siphash.c

    ../src/endianconv.c
    ../src/anet.c 

    ../src/ae.c

    ../src/lzf_c.c
    ../src/lzf_d.c

    ../src/zmalloc.c 
    ../src/release.c 
    ../src/util.c
    ./redis.c
)

SET_TARGET_PROPERTIES(redis
    PROPERTIES
    VERSION "${REDIS_SONAME}")
IF(WIN32 OR MINGW)
    TARGET_LINK_LIBRARIES(redis PRIVATE ws2_32)
ENDIF()
TARGET_INCLUDE_DIRECTORIES(redis PUBLIC .)

CONFIGURE_FILE(redis.pc.in redis.pc @ONLY)

INSTALL(TARGETS redis
    DESTINATION "${CMAKE_INSTALL_LIBDIR}")

INSTALL(FILES redis.h read.h sds.h async.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/redis)
    
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/redis.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# Add examples
#IF(ENABLE_EXAMPLES)
    ADD_EXECUTABLE(example-ae example-ae.c)
    TARGET_LINK_LIBRARIES(example-ae redis)

    ADD_EXECUTABLE(example-client example-client.c)
    TARGET_LINK_LIBRARIES(example-client redis)

    ADD_EXECUTABLE(example-server example-server.c)
    TARGET_LINK_LIBRARIES(example-server redis)
#ENDIF(ENABLE_EXAMPLES)

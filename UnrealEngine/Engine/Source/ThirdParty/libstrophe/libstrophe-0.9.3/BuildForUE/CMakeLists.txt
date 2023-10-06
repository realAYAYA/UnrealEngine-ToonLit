cmake_minimum_required(VERSION 3.3)
project(libstrophe)

# Put our build output one level up so we can easily delete the temporary files and only check-in the final libs
#  I have commented these out because they break pathing for AutomationTool's BuildCMakeLib
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")

set(SOCKET_IMPL ../src/sock.c CACHE STRING "Socket implementation")
option(DISABLE_TLS "Disable TLS support" ON)
option(DISABLE_SRV_LOOKUP "Disable SRV lookup" OFF)

set(EXPAT_PATH "" CACHE STRING "Path to expat headers")
set(OPENSSL_PATH "" CACHE STRING "Path to openssl headers")
set(UNREALSSL_PATH "" CACHE STRING "Path to unreal ssl headers")
set(SOCKETAPI_PATH "" CACHE STRING "Path to socket api headers")

if(${DISABLE_TLS})
	set(TLS_IMPL "../src/tls_dummy.c")
else()
if(${USE_UNREAL_SSL})
	set(TLS_IMPL "../src/tls_unreal.c")
	include_directories(${UNREALSSL_PATH})
	add_definitions(-DUSE_UNREAL_SSL)
else()
	set(TLS_IMPL "../src/tls_openssl.c")
endif()
	include_directories(${OPENSSL_PATH})
endif()

if (${DISABLE_SRV_LOOKUP})
	add_definitions(-DDISABLE_SRV_LOOKUP)
endif()

if (${USE_SOCKETAPI_DISPATCH})
	include_directories(${SOCKETAPI_PATH})
	add_definitions(-DUSE_SOCKETAPI_DISPATCH)
endif()

add_definitions(-DXML_STATIC=1)

if(WIN32)
	add_definitions(/D_CRT_SECURE_NO_WARNINGS=1)
endif()

include_directories(../src .. ${EXPAT_PATH})

set(SOURCES ../src/auth.c ../src/conn.c ../src/crypto.c ../src/ctx.c
	../src/event.c ../src/handler.c ../src/hash.c ../src/jid.c ../src/md5.c
	../src/resolver.c ../src/sasl.c ../src/scram.c ../src/sha1.c
	../src/snprintf.c ../src/stanza.c ../src/util.c
	../src/rand.c ../src/uuid.c ../src/parser_expat.c ${SOCKET_IMPL} ${TLS_IMPL})
	
add_library(strophe STATIC ${SOURCES})

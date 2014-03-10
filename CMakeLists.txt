cmake_minimum_required(VERSION 2.8.5)

enable_testing()

include(config.cmake)
set(CMAKE_C_FLAGS "-Wall -O2 -g -std=gnu99 -D_XOPEN_SOURCE=700 -D_STDC_C99 -D_XPG6 -D__EXTENSIONS__ -D_REENTRANT")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-inline-small-functions")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-inline-functions-called-once")

find_package(BISON)
find_package(FLEX)
find_package(OpenSSL)

BISON_TARGET(fmt3 post_fmt3.y post_fmt3.tab.c COMPILE_FLAGS "-p fmt3_")
FLEX_TARGET(fmt3 post_fmt3.l post_fmt3.lex.c COMPILE_FLAGS "-P fmt3_")
ADD_FLEX_BISON_DEPENDENCY(fmt3 fmt3)

BISON_TARGET(tmpl template.y template.tab.c COMPILE_FLAGS "-p tmpl_")
FLEX_TARGET(tmpl template.l template.lex.c COMPILE_FLAGS "-P tmpl_")
ADD_FLEX_BISON_DEPENDENCY(tmpl tmpl)

if(USE_XMLRPC)
	set(PINGBACK_SRC pingback.c)
	find_package(XMLRPC COMPONENTS cgi-server REQUIRED)
	include_directories(${XMLRPC_INCLUDE_DIRS})
else()
	set(PINGBACK_SRC )
	set(XMLRPC_LIBRARIES )
endif()

find_library(SQLITE_LIB sqlite3)
find_path(SQLITE_INC sqlite3.h)

include_directories(${SQLITE_INC})

set(COMMON_SOURCES
	# post - all formats
	post.c

	# post - format 3
	${FLEX_fmt3_OUTPUTS} ${BISON_fmt3_OUTPUTS}
	post_fmt3_cmds.c
	str.c
	listing.c mangle.c

	# templates
	${FLEX_tmpl_OUTPUTS} ${BISON_tmpl_OUTPUTS}
	template_cache.c
	render.c
	pipeline.c

	# nvlist related things
	qstring.c
	decode.c fsm.c
	vars.c vars_dump.c
	val.c

	# request processing
	req.c

	# pages
	admin.c
	comment.c
	index.c
	sidebar.c
	story.c
	tag.c

	# database
	db.c

	# misc
	error.c
	utils.c
)

add_executable(blahg
	main.c
	${COMMON_SOURCES}
	${PINGBACK_SRC}
)

target_link_libraries(blahg
	${OPENSSL_CRYPTO_LIBRARY}
	${XMLRPC_LIBRARIES}
	${SQLITE_LIB}
	rt
	umem
	nvpair
	avl
	m
)

add_executable(blahgd
	daemon.c helpers.c
	scgi.c
	${COMMON_SOURCES}
	${PINGBACK_SRC}
)

target_link_libraries(blahgd
	${OPENSSL_CRYPTO_LIBRARY}
	${SQLITE_LIB}
	pthread
	socket
	nsl
	umem
	nvpair
	avl
	m
)

add_executable(test_fmt3
	test_fmt3.c
	val.c
	${FLEX_fmt3_OUTPUTS} ${BISON_fmt3_OUTPUTS}
	post_fmt3_cmds.c
	listing.c
	mangle.c utils.c error.c
	str.c
)

target_link_libraries(test_fmt3
	${OPENSSL_CRYPTO_LIBRARY}
	umem
)

add_executable(test_qstring
	test_qstring.c
	error.c
	utils.c
	vars.c vars_dump.c
	qstring.c
	decode.c fsm.c
)

target_link_libraries(test_qstring
	umem
	nvpair
)

function(simple_c_test type section data)
	add_test(NAME "${type}:${section}:${data}"
		 COMMAND "${CMAKE_BINARY_DIR}/test_${type}"
			 "${CMAKE_CURRENT_SOURCE_DIR}/${data}"
	)
endfunction()

add_subdirectory(test)

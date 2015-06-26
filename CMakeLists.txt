cmake_minimum_required(VERSION 2.8.5)

enable_testing()

include(config.cmake)
set(CMAKE_C_FLAGS "-Wall -O2 -g -std=gnu99 -D_XOPEN_SOURCE=700 -D__EXTENSIONS__ -D_REENTRANT")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
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

add_executable(blahgd
	daemon.c helpers.c
	scgi.c

	# file caching & change monitoring
	file_cache.c

	# post - all formats
	post.c
	post_nv.c

	# post - format 3
	${FLEX_fmt3_OUTPUTS} ${BISON_fmt3_OUTPUTS}
	post_fmt3_cmds.c
	str.c
	listing.c mangle.c
	math.c

	# templates
	${FLEX_tmpl_OUTPUTS} ${BISON_tmpl_OUTPUTS}
	render.c
	pipeline.c

	# nvlist related things
	qstring.c
	decode.c fsm.c
	vars.c vars_dump.c
	val.c
	nvl.c
	nvl_convert.c
	nvl_yaml.c

	# request processing
	req.c
	stats.c

	# pages
	admin.c
	comment.c
	index.c
	sidebar.c
	story.c
	tag.c
	static.c

	# database
	db.c

	# misc
	error.c
	utils.c

	${PINGBACK_SRC}
)

target_link_libraries(blahgd
	${OPENSSL_CRYPTO_LIBRARY}
	${SQLITE_LIB}
	yaml
	pthread
	socket
	nsl
	umem
	nvpair
	/usr/lib/libavl.so.1
	/usr/lib/libcmdutils.so.1
	m
)

add_executable(mathd
	mathd.c
	math.c
	error.c
	val.c
	str.c
)

target_link_libraries(mathd
	${OPENSSL_CRYPTO_LIBRARY}
	umem
)

add_executable(nvdump
	nvdump.c
	utils.c
	nvl.c
	vars_dump.c
	error.c
)

target_link_libraries(nvdump
	umem
	nvpair
)

add_executable(test_fmt3
	test_fmt3.c
	val.c
	post.c
	${FLEX_fmt3_OUTPUTS} ${BISON_fmt3_OUTPUTS}
	post_fmt3_cmds.c
	file_cache.c
	math.c
	listing.c
	mangle.c utils.c error.c
	str.c
	db.c
	nvl.c
	nvl_convert.c
	nvl_yaml.c
)

target_link_libraries(test_fmt3
	${OPENSSL_CRYPTO_LIBRARY}
	${SQLITE_LIB}
	yaml
	umem
	nvpair
	/usr/lib/libavl.so.1
	/usr/lib/libcmdutils.so.1
)

add_executable(test_qstring
	test_qstring.c
	error.c
	utils.c
	vars.c vars_dump.c nvl.c
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

#
# Copyright (c) 2011-2016 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

cmake_minimum_required(VERSION 2.8.12.2)

enable_testing()

include(config.cmake)

add_definitions(
	-D__EXTENSIONS__
	-D_REENTRANT
)

add_compile_options(
	-Wall
	-O2
	-g
	-std=gnu99
	-fno-omit-frame-pointer
	$<$<C_COMPILER_ID:gcc>:-fno-inline-small-functions>
	$<$<C_COMPILER_ID:gcc>:-fno-inline-functions-called-once>
)

find_package(BISON)
find_package(FLEX)

BISON_TARGET(fmt3 post_fmt3.y post_fmt3.tab.c COMPILE_FLAGS "-p fmt3_")
FLEX_TARGET(fmt3 post_fmt3.l post_fmt3.lex.c COMPILE_FLAGS "-P fmt3_")
ADD_FLEX_BISON_DEPENDENCY(fmt3 fmt3)

BISON_TARGET(tmpl template.y template.tab.c COMPILE_FLAGS "-p tmpl_")
FLEX_TARGET(tmpl template.l template.lex.c COMPILE_FLAGS "-P tmpl_")
ADD_FLEX_BISON_DEPENDENCY(tmpl tmpl)

include_directories(
	suntaskq/include
	${JEFFPC_INCLUDE_DIR}
)

add_subdirectory(suntaskq)

add_custom_target(revisiontag ALL)

add_custom_command(TARGET revisiontag
	COMMAND ${CMAKE_COMMAND}
		-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
		-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/git.cmake
)

add_library(version
	version.c
)
add_dependencies(version revisiontag)

add_executable(blahgd
	daemon.c helpers.c
	scgi.c
	config.c

	# file caching & change monitoring
	file_cache.c

	# post - all formats
	post.c
	post_index.c
	post_nv.c

	# post - format 3
	${FLEX_fmt3_OUTPUTS} ${BISON_fmt3_OUTPUTS}
	post_fmt3_cmds.c
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
	nvl.c
	nvl_convert.c

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

	# misc
	error.c
	utils.c
)

target_link_libraries(blahgd
	version
	pthread
	socket
	nsl
	umem
	nvpair
	md
	/usr/lib/libavl.so.1
	/usr/lib/libcmdutils.so.1
	m
	suntaskq
	${JEFFPC_LIBRARY}
)

add_executable(mathd
	mathd.c
	math.c
	error.c
	config.c
	mangle.c
	utils.c
)

target_link_libraries(mathd
	version
	umem
	md
	/usr/lib/libavl.so.1
	${JEFFPC_LIBRARY}
)

add_executable(lisplint
	lisplint.c
	error.c
	mangle.c
	utils.c

	nvl.c
	nvl_convert.c
)

target_link_libraries(lisplint
	umem
	nvpair
	/usr/lib/libavl.so.1
	${JEFFPC_LIBRARY}
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
	/usr/lib/libavl.so.1
	${JEFFPC_LIBRARY}
)

add_executable(test_fmt3
	test_fmt3.c
	post.c
	post_index.c
	${FLEX_fmt3_OUTPUTS} ${BISON_fmt3_OUTPUTS}
	post_fmt3_cmds.c
	file_cache.c
	config.c
	math.c
	listing.c
	mangle.c utils.c error.c
	nvl.c
	nvl_convert.c
)

target_link_libraries(test_fmt3
	umem
	nvpair
	md
	suntaskq
	/usr/lib/libavl.so.1
	/usr/lib/libcmdutils.so.1
	${JEFFPC_LIBRARY}
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
	/usr/lib/libavl.so.1
	${JEFFPC_LIBRARY}
)

function(simple_c_test type section bin data)
	add_test(NAME "${type}:${section}:${data}"
		 COMMAND "${CMAKE_BINARY_DIR}/test_${bin}"
			 "${CMAKE_CURRENT_SOURCE_DIR}/${data}"
	)
endfunction()

add_subdirectory(test)

cmake_minimum_required(VERSION 2.8)

include(CheckFunctionExists)
configure_file(config.h.in config.h)

check_function_exists(flock HAVE_FLOCK)

set(CMAKE_C_FLAGS "-Wall -O2 -g -std=gnu99 -D_XOPEN_SOURCE=700 -D_STDC_C99 -D_XPG6 -D__EXTENSIONS__")

find_package(BISON)
find_package(FLEX)

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

add_executable(blahg render.c post.c main.c
	db.c ${PINGBACK_SRC} vars.c pipeline.c avl.c
	${FLEX_tmpl_OUTPUTS} ${BISON_tmpl_OUTPUTS}
	${FLEX_fmt3_OUTPUTS} ${BISON_fmt3_OUTPUTS}
	index.c sidebar.c story.c tag.c feed.c comment.c
	admin.c
	listing.c utils.c error.c mangle.c
	decode.c fsm.c
)

find_library(SQLITE_LIB sqlite3)
find_path(SQLITE_INC sqlite3.h)

include_directories(${SQLITE_INC})

target_link_libraries(blahg ${XMLRPC_LIBRARIES} ${SQLITE_LIB})
target_link_libraries(blahg rt)
target_link_libraries(blahg umem)
target_link_libraries(blahg m)

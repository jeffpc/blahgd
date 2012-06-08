cmake_minimum_required(VERSION 2.8)

include(CheckFunctionExists)
configure_file(config.h.in config.h)

check_function_exists(flock HAVE_FLOCK)

set(CMAKE_C_FLAGS "-Wall -O2 -g -std=c99 -D_XOPEN_SOURCE=700 -D_STDC_C99 -D_XPG6 -D__EXTENSIONS__")

find_package(BISON)
find_package(FLEX)

BISON_TARGET(fmt3 post_fmt3.y post_fmt3.tab.c)
FLEX_TARGET(fmt3 post_fmt3.l post_fmt3.lex.c)
ADD_FLEX_BISON_DEPENDENCY(fmt3 fmt3)

add_executable(blahg sar.c post.c xattr.c html.c dir.c fsm.c decode.c
	listing.c main.c archive.c category.c comment.c feed.c index.c
	story.c pingback.c tag.c utils.c db.c
	${FLEX_fmt3_OUTPUTS}
	${BISON_fmt3_OUTPUTS}
)

find_library(SQLITE_LIB sqlite3)

find_package(XMLRPC COMPONENTS cgi-server REQUIRED)
include_directories(${XMLRPC_INCLUDE_DIRS})
target_link_libraries(blahg ${XMLRPC_LIBRARIES} ${SQLITE_LIB})

target_link_libraries(blahg rt)

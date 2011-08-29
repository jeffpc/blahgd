cmake_minimum_required(VERSION 2.6)

set(CMAKE_C_FLAGS "-Wall -O2 -g -std=c99 -D_POSIX_C_SOURCE=199309 -D_BSD_SOURCE")

find_package(BISON)
find_package(FLEX)

BISON_TARGET(fmt3 post_fmt3.y post_fmt3.tab.c)
FLEX_TARGET(fmt3 post_fmt3.l post_fmt3.lex.c)
ADD_FLEX_BISON_DEPENDENCY(fmt3 fmt3)

add_executable(blahg sar.c post.c xattr.c html.c dir.c fsm.c decode.c
	listing.c main.c archive.c category.c comment.c feed.c index.c
	story.c
	${FLEX_fmt3_OUTPUTS}
	${BISON_fmt3_OUTPUTS}
)

target_link_libraries(blahg rt)

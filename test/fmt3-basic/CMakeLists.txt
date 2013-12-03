file(GLOB TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.tex)
foreach(TEST ${TESTS})
	simple_c_test(fmt3 basic ${TEST})
endforeach()

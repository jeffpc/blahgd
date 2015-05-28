file(GLOB TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.qs)
foreach(TEST ${TESTS})
	simple_c_test(qstring basic ${TEST})
endforeach()

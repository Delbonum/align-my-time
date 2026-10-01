add_executable(amt_core_tests CoreTests.cpp)
target_link_libraries(amt_core_tests PRIVATE amt_core)
add_test(NAME amt_core_tests COMMAND amt_core_tests)

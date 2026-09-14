# Core unit tests: plain executables, no Qt.
add_executable(test_assembler test_assembler.cpp)
target_link_libraries(test_assembler PRIVATE casl::core)

add_executable(test_machine test_machine.cpp)
target_link_libraries(test_machine PRIVATE casl::core)

foreach(szTest test_assembler test_machine)
    add_test(NAME ${szTest} COMMAND ${szTest})
endforeach()

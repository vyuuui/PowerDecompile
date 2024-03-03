add_executable(flowgraph_test FlowGraphTest.cc)

target_link_libraries(flowgraph_test doctest decomp-lib)
add_test(flowgraph flowgraph_test)

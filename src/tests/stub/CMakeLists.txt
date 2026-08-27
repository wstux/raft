TestLibTarget(raft_stub INTERFACE
    HEADERS
        io_stub.h
        network_stub.h
    INCLUDE_DIR src/tests
    COMPILE_DEFINITIONS
        TEST_DATA_DIR="${CMAKE_BINARY_DIR}/__test_data"
)

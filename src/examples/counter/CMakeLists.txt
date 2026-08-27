if (BUILD_EXAMPLES)
    add_subdirectory(schema)
endif()

ExampleTarget(counter_cluster
    SOURCES
        main.cpp
        details/config.cpp
        details/counter.cpp
    INCLUDE_DIR
        src/examples
    LIBRARIES
        counter_schema
        raft
    DEPENDS
        grpc
)

if (BUILD_EXAMPLES)
    add_subdirectory(schema)
endif()

ExampleLibTarget(cluster STATIC
    SOURCES
        details/io.cpp
        details/node.cpp
    INCLUDE_DIR
        src/examples
    LIBRARIES
        cluster_schema
        raft
    DEPENDS
        boost
        lmdb
        grpc
)

ConfigureFile(config_file counter.config
    BUILD_DIR=${CMAKE_BINARY_DIR}
)

ExampleTarget(counter
    SOURCES
        main.cpp
        details/config.cpp
        details/counter.cpp
    INCLUDE_DIR
        src/examples
    LIBRARIES
        cluster
    DEPENDS
        config_file
)

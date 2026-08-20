if (BUILD_EXAMPLES)
    add_compile_options(-Wno-pedantic)
    add_compile_options(-Wno-unused-parameter)
    add_compile_options(-Wno-cast-function-type)
    add_compile_options(-Wno-sign-compare)
    add_compile_options(-Wno-ignored-qualifiers)
    add_compile_options(-Wno-type-limits)
    add_compile_options(-Wno-trigraphs)
    add_compile_options(-Wno-return-type)
    add_compile_options(-Wno-attributes)
    add_compile_options(-Wno-missing-field-initializers)
    set(CMAKE_CXX_FLAGS "-Wno-redundant-move")

    FetchTarget(grpc
        GIT_REPOSITORY  https://github.com/grpc/grpc.git
        GIT_TAG         f5ffb68d8a2fd603dff16287e90a4ac571e1fec6
        CMAKE_OPTIONS
            CMAKE_WARN_DEPRECATED=OFF
            # grps flags
            gRPC_BUILD_TESTS=OFF
            gRPC_BUILD_GRPC_CPP_PLUGIN=ON
            gRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF
            gRPC_BUILD_GRPC_NODE_PLUGIN=OFF
            gRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF
            gRPC_BUILD_GRPC_PHP_PLUGIN=OFF
            gRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF
            gRPC_BUILD_GRPC_RUBY_PLUGIN=OFF
            # abseil flags
            ABSL_BUILD_TESTING=OFF
            ABSL_BUILD_TEST_HELPERS=OFF
            ABSL_USE_EXTERNAL_GOOGLETEST=OFF
            ABSL_FIND_GOOGLETEST=OFF
            ABSL_USE_GOOGLETEST_HEAD=OFF
            # re2 flags
            RE2_TEST=OFF
            RE2_BENCHMARK=OFF
            RE2_BUILD_TESTING=OFF
            # zlib flags
            ZLIB_BUILD_EXAMPLES=OFF
        CMAKE_VARIABLES
            # abseil variables
            ABSL_ENABLE_INSTALL=ON
            # c-ares variables
            HAVE_SOCKLEN_T=1
            HAVE_SSIZE_T=1
            HAVE_LONGLONG=1
            HAVE_STRUCT_ADDRINFO=1
            HAVE_STRUCT_IN6_ADDR=1
            HAVE_STRUCT_SOCKADDR_IN6=1
            HAVE_STRUCT_SOCKADDR_STORAGE=1
            HAVE_STRUCT_TIMEVAL=1
#        COMPILE_OPTIONS
#            -Wno-pedantic
#            -Wno-unused-parameter
#            -Wno-cast-function-type
#            -Wno-sign-compare
#            -Wno-ignored-qualifiers
#            -Wno-type-limits
#            -Wno-trigraphs
#            -Wno-return-type
#            -Wno-attributes
#            -Wno-missing-field-initializers
#        CXX_COMPILE_FLAGS
#            -Wno-redundant-move
        LIBRARIES
            grpc++
            libprotobuf
            c-ares::cares
            re2::re2
    )

    include(grpc_schema_target.cmake)
endif()

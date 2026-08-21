add_subdirectory(boost)
add_subdirectory(grpc)

WrapperTarget(ext1
    BUILDSYS    cmake
    SOURCE_DIR
        ext1
    INCLUDE_DIR
        ${CMAKE_SOURCE_DIR}/externals/ext1/libs
    LIBRARIES
        ${CMAKE_BINARY_DIR}/externals/ext1/libs/ext_shared_lib/libext_shared_lib.so
)

WrapperTarget(ext2
    BUILDSYS
        cmake
    SOURCE_DIR
        ext2
    INCLUDE_DIR
        ${CMAKE_SOURCE_DIR}/externals/ext2/libs
)

if (BUILD_TESTS)
    ExternalTarget(googlebenchmark
        VERSION "1.9.5"
        URL     https://github.com/google/benchmark/archive/refs/tags/v1.9.5.tar.gz
        URL_MD5 "12c6c0c228fc07106c62634222bd2541"
        CONFIGURE_COMMAND
            cmake -DBENCHMARK_ENABLE_GTEST_TESTS=OFF
                -DBENCHMARK_DOWNLOAD_DEPENDENCIES=OFF
                -DCMAKE_BUILD_TYPE=Release
                --install-prefix ${EXTERNALS_PREFIX}/googlebenchmark/install ./
        BUILD_COMMAND
            cmake --build ./ --config Release && cmake --install ./
        INCLUDE_DIR
            ${EXTERNALS_PREFIX}/googlebenchmark/install/include
        INSTALL_DIR
            ${EXTERNALS_PREFIX}/googlebenchmark/install
        LIBRARIES
            libbenchmark.a
            libbenchmark_main.a
    )

    ExternalTarget(googletest
        VERSION "1.17.0"
        URL     https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
        URL_MD5 "b6f100bc2a5853a48046aa168ececf84"
        CONFIGURE_COMMAND
            cmake --install-prefix ${EXTERNALS_PREFIX}/googletest/install ./
        BUILD_COMMAND
            cmake --build ./ && cmake --install ./
        INCLUDE_DIR
            ${EXTERNALS_PREFIX}/googletest/install/include
        INSTALL_DIR
            ${EXTERNALS_PREFIX}/googletest/install
        LIBRARIES
            libgmock.a
            libgmock_main.a
            libgtest.a
            libgtest_main.a
    )

    ExternalTarget(testing
        URL
            https://github.com/wstux/testing_template/archive/refs/heads/master.zip
        #URL_MD5
        #    fe33be48eeb6de4dc2fefe60a3834c1f
        CONFIGURE_COMMAND
            cmake --install-prefix ${EXTERNALS_PREFIX}/testing/install  ./
        BUILD_COMMAND
            cmake --build ./
        INSTALL_COMMAND
            cmake --install ./
        INCLUDE_DIR
            ${EXTERNALS_PREFIX}/testing/install/include
        INSTALL_DIR
            ${EXTERNALS_PREFIX}/testing/install
    )
endif()

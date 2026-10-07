if (BUILD_EXAMPLES)
    ExternalTarget(lmdb
        VERSION "1.0.1"
        URL     https://github.com/LMDB/lmdb/archive/refs/tags/LMDB_1.0.1.tar.gz
        URL_MD5 "1306f1e75947830acbd50782b6c7714a"
        BUILD_COMMAND
            cd "libraries/liblmdb" && make && make install prefix=${EXTERNALS_PREFIX}/lmdb/install
        INCLUDE_DIR
            ${EXTERNALS_PREFIX}/lmdb/install/include
        INSTALL_DIR
            ${EXTERNALS_PREFIX}/lmdb/install
        LIBRARIES
            liblmdb.a
    )
endif()

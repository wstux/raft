if (BUILD_BOOST)
    set(CMAKE_CXX_FLAGS "-Wno-uninitialized;-Wno-deprecated-copy")

    ExternalTarget(boost
        URL     https://github.com/boostorg/boost/releases/download/boost-1.88.0/boost-1.88.0-cmake.tar.gz
        URL_MD5 "fe33be48eeb6de4dc2fefe60a3834c1f"
        CONFIGURE_COMMAND
            ./bootstrap.sh --prefix=${EXTERNALS_PREFIX}/boost/install
                           --with-toolset=gcc
                           --with-libraries=date_time,thread
        BUILD_COMMAND
            CXXFLAGS='${CMAKE_CXX_FLAGS};-Wno-uninitialized'
            ./b2 linkflags=\"${CMAKE_EXE_LINKER_FLAGS}\"
                 cflags=\"${CMAKE_C_FLAGS}\"
                 cxxflags=\"${CMAKE_CXX_FLAGS}\"
                 architecture=x86
                 address-model=64
                 #link=shared,static
                 link=static
                 --debug-configuration -q
                 --prefix=${EXTERNALS_PREFIX}/boost/install
                 --layout=tagged -j8 threading=multi
                 runtime-link=static
                 variant=release install
        INCLUDE_DIR
            ${EXTERNALS_PREFIX}/boost/install/include
        INSTALL_DIR
            ${EXTERNALS_PREFIX}/boost/install
        LIBRARIES
            libboost_date_time-mt-x64.a
            libboost_thread-mt-x64.a
    )
endif()

ExecTarget(executable
    SOURCES
        main.cpp
#    COMPILE_DEFINITIONS
#        CHECK_FIND_PACKAGE_BOOST
    LIBRARIES
        interface_lib
        shared_lib
        shared_lib_2
        shared_lib_3
        static_lib
        static_lib_2
        static_lib_3
        static_lib_boost
        version_lib
    DEPENDS
#        boost
        ext1
        ext2
        testing
)

# Unit tests

if (BUILD_TESTS)
    add_subdirectory(stub)
endif()

TestTarget(ut_raft_role
    SOURCES
        ut_raft_role.cpp
    LIBRARIES
        raft_le
    DEPENDS
        googletest
)

TestTarget(ut_raft_role_convert
    SOURCES
        ut_raft_role_convert.cpp
    LIBRARIES
        raft_le
        raft_stub
    DEPENDS
        googletest
)

TestTarget(ut_raft_role_election
    SOURCES
        ut_raft_role_election.cpp
    LIBRARIES
        raft_le
        raft_stub
    DEPENDS
        googletest
)

TestTarget(ut_raft_scheduler
    SOURCES
        ut_raft_scheduler.cpp
    LIBRARIES
        raft_le
    DEPENDS
        googletest
)

TestTarget(ut_raft_server_leader_election
    SOURCES
        ut_raft_server_leader_election.cpp
    LIBRARIES
        raft_le
        raft_stub
    DEPENDS
        googletest
)

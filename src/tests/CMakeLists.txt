add_subdirectory(benchmark)
add_subdirectory(stub)

# Unit tests

TestTarget(ut_raft_context
    SOURCES
        ut_raft_context.cpp
    LIBRARIES
        raft_le
    DEPENDS
        googletest
)

TestTarget(ut_raft_handlers
    SOURCES
        ut_raft_handlers.cpp
    LIBRARIES
        raft_le
    DEPENDS
        googletest
)

TestTarget(ut_raft_peers
    SOURCES
        ut_raft_peers.cpp
    LIBRARIES
        raft_le
    DEPENDS
        googletest
)

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
    DEPENDS
        googletest
)

TestTarget(ut_raft_role_election
    SOURCES
        ut_raft_role_election.cpp
    LIBRARIES
        raft_le
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

TestTarget(ut_raft_serialization
    SOURCES
        ut_raft_serialization.cpp
    LIBRARIES
        raft_le
    DEPENDS
        googletest
)

TestTarget(ut_raft_server
    SOURCES
        ut_raft_server.cpp
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

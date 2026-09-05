# Performance tests

PerfTestTarget(perf_raft_leader_election
    SOURCES
        perf_raft_leader_election.cpp
    LIBRARIES
        raft
        raft_stub
    DEPENDS
        googlebenchmark
)

PerfTestTarget(perf_raft_leader_election_async
    SOURCES
        perf_raft_leader_election.cpp
    COMPILE_DEFINITIONS USE_ASYNC_IO
    LIBRARIES
        raft
        raft_stub
    DEPENDS
        googlebenchmark
)

PerfTestTarget(perf_raft_membership
    SOURCES
        perf_raft_membership.cpp
    LIBRARIES
        raft
        raft_stub
    DEPENDS
        googlebenchmark
)

PerfTestTarget(perf_raft_membership_async
    SOURCES
        perf_raft_membership.cpp
    COMPILE_DEFINITIONS USE_ASYNC_IO
    LIBRARIES
        raft
        raft_stub
    DEPENDS
        googlebenchmark
)

PerfTestTarget(perf_raft_peers
    SOURCES
        perf_raft_peers.cpp
    LIBRARIES
        raft
    DEPENDS
        googlebenchmark
)

PerfTestTarget(perf_raft_serialization
    SOURCES
        perf_raft_serialization.cpp
    LIBRARIES
        raft
    DEPENDS
        googlebenchmark
)

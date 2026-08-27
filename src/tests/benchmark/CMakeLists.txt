# Performance tests

PerfTestTarget(perf_raft_leader_election
    SOURCES
        perf_raft_leader_election.cpp
    LIBRARIES
        raft_le
        raft_stub
    DEPENDS
        googlebenchmark
)

PerfTestTarget(perf_raft_peers
    SOURCES
        perf_raft_peers.cpp
    LIBRARIES
        raft_le
    DEPENDS
        googlebenchmark
)

PerfTestTarget(perf_raft_serialization
    SOURCES
        perf_raft_serialization.cpp
    LIBRARIES
        raft_le
    DEPENDS
        googlebenchmark
)

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

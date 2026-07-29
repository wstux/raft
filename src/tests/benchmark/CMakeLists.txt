# Performance tests

TestTarget(perf_raft_leader_election
    SOURCES
        perf_raft_leader_election.cpp
    LIBRARIES
        raft_le
        raft_stub
    DEPENDS
        googlebenchmark
    DISABLE
)

LibTarget(raft_le STATIC
    SOURCES
        details/context.cpp
        details/scheduler.cpp
        details/server.cpp
        details/connection/peer.cpp
    INCLUDE_DIR
        src/libs
    DEPENDS
        boost
)

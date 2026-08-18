LibTarget(raft_le STATIC
    SOURCES
        details/context.cpp
        details/scheduler.cpp
        details/server.cpp
        details/handlers/heartbeat_handler.cpp
        details/handlers/timeout_handler.cpp
        details/handlers/vote_handler.cpp
        details/role/convert.cpp
        details/role/election.cpp
    INCLUDE_DIR
        src/libs
    DEPENDS
        boost
)

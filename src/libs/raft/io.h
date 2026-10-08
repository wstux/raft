/*
 * The MIT License
 *
 * Copyright 2026 Chistyakov Alexander.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _LIBS_RAFT_IO_H_
#define _LIBS_RAFT_IO_H_

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "raft/details/span.h"

namespace wstux {
namespace raft {

/// \brief  Default allocator type for handling raw bytes.
using allocator_type = std::allocator<std::byte>;

/// \brief  Predicate function to check if threads/processes should be stopped.
using is_stop_fn_t = std::function<bool(void)>;

/// \brief  Unique identifier of a node (server) in the cluster.
using server_id_t = uint64_t;
/// \brief  Index of an entry in the Raft log.
using index_t = uint32_t;
/// \brief  Term number (epoch) in the Raft algorithm.
using term_t = uint32_t;

/// \brief  Non-owning input data buffer (immutable memory slice).
using inbuffer_type = details::span<const char>;
/// \brief  Dynamic buffer for storing serialized data.
using buffer_type = std::vector<char>;

/// \brief  Constant representing an invalid or empty server ID.
constexpr server_id_t gk_invalid_id = 0;

/**
 *  \brief  General configuration of the local Raft node.
 */
struct config final
{
    std::string address;    //!< Network address of the current server (IP:port).
    bool is_voter = false;  //!< Flag determining whether the node participates in voting.

    size_t vote_timeout_min_ms = 250;   //!< Minimum election timeout in milliseconds.
    size_t vote_timeout_max_ms = 500;   //!< Maximum election timeout in milliseconds.

    size_t heartbeat_interval_ms = 100; //!< Heartbeat broadcast interval by the leader, in ms.

    size_t scheduler_threads_count = 4; //!< Number of threads in the task scheduler.

    size_t snapshot_threshold = 512;    //!< Log entry count threshold that triggers snapshot creation.
    size_t snapshot_trailing = 1024;    //!< Number of log entries retained for trailing nodes after snapshot creation.

    bool is_async_io = false;   //!< Flag to enable asynchronous input/output (Async I/O).

    bool is_append_entries_log_ch_enabled = true; //!< Enables logging for the AppendEntries channel.
    bool is_join_log_ch_enabled = true;           //!< Enables logging for the node join channel.
    bool is_heartbeat_log_ch_enabled = true;      //!< Enables logging for sending/receiving heartbeats.
    bool is_snapshot_log_ch_enabled = true;       //!< Enables logging for snapshot creation/transmission processes.
    bool is_timeout_log_ch_enabled = true;        //!< Enables logging for timeout triggers.
    bool is_vote_log_ch_enabled = true;           //!< Enables logging for voting processes.
};

/**
 *  \brief  Configuration data for an individual server in the cluster.
 */
struct server_config final
{
    /// \brief Default constructor. Initializes the node as invalid.
    server_config()
        : id(gk_invalid_id)
        , address()
        , is_voter(false)
    {}

    /// \brief  Parameterized constructor.
    /// \param  id - unique identifier of the server.
    /// \param  addr - network address of the server.
    /// \param  is_voter - whether the server participates in voting.
    server_config(const server_id_t id, std::string addr, bool is_voter)
        : id(id)
        , address(std::move(addr))
        , is_voter(is_voter)
    {}

    server_id_t id;      //!< Server identifier.
    std::string address; //!< Server network address.
    bool is_voter;       //!< Voting member status.
};

/**
 *  \brief  Full configuration of the entire Raft cluster.
 */
struct cluster_config final
{
    std::vector<server_config> servers; //!< List of all servers included in the cluster.
};

/**
 *  \brief  Raft log entry type.
 */
enum entry_type : int32_t
{
    change = 0, //!< Cluster configuration change (e.g., adding/removing a node).
    command = 1 //!< Business logic command applied to the state machine (FSM).
};

/**
 *  \brief  Raft log entry.
 */
struct entry final
{
    using ptr = std::shared_ptr<entry>; //!< Smart pointer to a log entry.
    using list = std::vector<ptr>;      //!< List (batch) of log entries.

    term_t term;        //!< The term (epoch) when the entry was created by the leader.
    entry_type type;    //!< Entry type (command or configuration change).
    buffer_type buffer; //!< Payload (binary data of the command).
};

/**
 *  \brief  Snapshot (compact point-in-time image) of the finite state machine
 *      (FSM) and configuration.
 */
struct snapshot final
{
    index_t index; //!< Index of last entry included in the snapshot.
    term_t term;   //!< Term of last entry included in the snapshot.

    cluster_config conf; //!< Last committed configuration included in the snapshot.
    index_t conf_index;  //!< Index of last committed configuration.

    buffer_type buffer;  //!< Serialized data of the finite state machine (FSM) state..
};

/**
 *  \brief  Abstract base class for the Finite State Machine (FSM).
 *
 *  \details    Implements the deterministic business logic of the application.
 *      Raft log entries are sequentially applied to this state machine after
 *      they are committed.
 */
class fsm
{
public:
    using ptr = std::shared_ptr<fsm>; //!< Smart pointer to a finite state machine instance.

public:
    /// \brief  Virtual destructor.
    virtual ~fsm() {}

    /// \brief  Apply a command to the state machine.
    /// \param  buf - buffer containing serialized command data (entry_type::command).
    /// \return true if the command was successfully applied, false in case of a processing error.
    virtual bool apply(const buffer_type& buf) noexcept = 0;

    /// \brief  Deinitialize the state machine. Clean up resources before stopping.
    virtual void deinit() noexcept = 0;

    /// \brief  Initialize the state machine for a specific node.
    /// \param  id - identifier of the current server.
    /// \return true if initialization was successful, false In case of a critical error.
    virtual bool init(server_id_t id) noexcept = 0;

    /// \brief  Notify the state machine about a node configuration change.
    /// \param  id - new or updated server identifier.
    /// \return true upon successful reconfiguration, false if the configuration cannot be applied.
    virtual bool reconfigure(server_id_t id) noexcept = 0;

    /// \brief  Restore the state machine's state from a snapshot.
    /// \param  buf - serialized state (content of snapshot::buffer).
    /// \return true if the state was successfully restored, false if data is corrupted or cannot be applied.
    virtual bool restore(const buffer_type& buf) noexcept = 0;

    /// \brief  Take a snapshot of the current state machine state.
    /// \param  buf - buffer into which the current state will be serialized.
    /// \return true if the snapshot was successfully created, false if the state failed to serialize.
    virtual bool take_snapshot(buffer_type& buf) noexcept = 0;
};

/**
 *  \brief  Abstract base class for the input/output (I/O) subsystem.
 *
 *  \details    Responsible for two key aspects: persistence (saving logs, terms,
 *      and snapshots to disk) and network communication (sending messages to
 *      other cluster nodes).
 */
class io
{
public:
    using ptr = std::shared_ptr<io>; //!< Smart pointer to an I/O subsystem instance.

public:
    /// \brief Virtual destructor.
    virtual ~io() {}

    /// \brief  Append a batch of entries to the end of the persistent log.
    /// \param  entries - list of entries to be appended.
    /// \return true if all entries were successfully saved to disk, false in case of a write error.
    virtual bool append(const entry::list& entries) noexcept = 0;

    /// \brief  Deinitialize the I/O subsystem (close files, network sockets).
    virtual void deinit() noexcept = 0;

    /// \brief  Retrieve the latest snapshot from storage.
    /// \return The snapshot structure, or std::nullopt if no snapshot exists.
    virtual std::optional<snapshot> get_snapshot() const noexcept = 0;

    /// \brief  Initialize the I/O subsystem for a specific node.
    /// \param  id - identifier of the server.
    /// \return true if log files are opened and the network is ready for operation, false in case of a system error.
    virtual bool init(server_id_t id) noexcept = 0;

    /// \brief  Load all log entries from the persistent storage during startup.
    /// \return List of read entries.
    virtual entry::list load_entries() noexcept = 0;

    /// \brief  Load the index of the latest snapshot.
    /// \return Index of the last entry included in the snapshot (0 if no snapshots exist).
    virtual index_t load_snapshot_index() noexcept = 0;

    /// \brief  Load the term of the latest snapshot.
    /// \return Term of the last entry included in the snapshot.
    virtual term_t load_snapshot_term() noexcept = 0;

    /// \brief  Get the initial log index available in the storage.
    /// \return Starting index of the log.
    /// \details    If the log has been partially truncated after snapshot creation, this index will be greater than 1.
    virtual index_t load_start_index() noexcept = 0;

    /// \brief  Load the last saved term (epoch number).
    /// \return The saved term number. The term number is strictly greater than zero.
    virtual term_t load_term() noexcept = 0;

    /// \brief  Reconfigure network/disk resources for the specified ID.
    /// \param  id - new server identifier.
    /// \return true if resources were successfully reallocated.
    virtual bool reconfigure(server_id_t id) noexcept = 0;

    /// \brief  Synchronously or asynchronously send a network message to another cluster node.
    /// \param  id - identifier of the target server.
    /// \param  address - network address of the target server.
    /// \param  msg - binary data of the message.
    virtual void send(server_id_t id, std::string_view address, const buffer_type& msg) noexcept = 0;

    /// \brief  Save a new snapshot to persistent storage.
    /// \param  sh - structure of the snapshot to be written.
    /// \return true if the snapshot was successfully saved to disk, false in case of a write error.
    virtual bool set_snapshot(const snapshot& sh) noexcept = 0;

    /// \brief  Persistently save the current term (epoch).
    /// \param  term - new term number.
    virtual void set_term(term_t term) noexcept = 0;

    /// \brief  Save the identifier of the node that the local server voted for in the current term.
    /// \param  id - identifier of the server that received the vote.
    virtual void set_voted_for(server_id_t id) noexcept = 0;

    /// \brief  Truncate (delete) all entries from the log starting from the specified index.
    /// \param  begin - index starting from which (inclusive) entries are deleted.
    /// \return true upon successful log truncation, false in case of a disk subsystem failure.
    /// \details    Used during conflicts when a leader overwrites uncommitted entries.
    virtual bool truncate(const index_t begin) noexcept = 0;

    /// \brief  Read from storage the identifier of the node voted for in the current term.
    /// \return Server ID, or gk_invalid_id if no vote has been cast in this term yet.
    virtual server_id_t voted_for() const noexcept = 0;
};

/**
 *  \brief  Callback structure (C-style interface) for integration with the application logger.
 */
struct logging_handler
{
    using ptr = std::unique_ptr<logging_handler>; //!< Smart pointer to the logging handler.

    /**
     *  \brief  Log message severity levels, analogous to syslog.
     */
    enum severity_level
    {
        emerg   = 0, //!< System is unusable.
        fatal   = 1, //!< Critical core error.
        crit    = 2, //!< Critical condition of components.
        error   = 3, //!< Runtime error.
        warning = 4, //!< Warning.
        notice  = 5, //!< Important notification.
        info    = 6, //!< Informational message.
        debug   = 7, //!< Debugging message.
        trace   = 8  //!< Execution trace.
    };

    /// \brief  Function signature to check if a specific logging level is enabled.
    using can_log_fn_t = bool (*)(void* p_this, severity_level lvl);
    /// \brief  Function signature to write a text message to the log.
    using log_fn_t     = void (*)(void* p_this, severity_level lvl, const char* p_msg);

    /// \brief  Virtual destructor.
    virtual ~logging_handler() {}

    void* p_this = nullptr;            //!< Pointer to the user logger context (the 'this' equivalent).
    can_log_fn_t can_log_fn = nullptr; //!< Pointer to the log level filtering function.
    log_fn_t log_fn = nullptr;         //!< Pointer to the log writing function.
};

} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_IO_H_ */

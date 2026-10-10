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

#ifndef _LIBS_RAFT_SERVER_H_
#define _LIBS_RAFT_SERVER_H_

#include <atomic>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "raft/io.h"

namespace wstux {
namespace raft {
namespace details { struct context; }

/**
 *  \brief  Main control class (core) of the local Raft cluster node.
 *
 *  \details    This class encapsulates the entire logic of the Raft consensus
 *      protocol, coordinating the operations of the finite state machine (`fsm`),
 *      the input/output subsystem (`io`), and the internal execution context.
 *
 *  General principle of Raft operation on a node:
 *  1. lifecycle and roles: the server is always in one of three states (roles):
 *     'follower', 'candidate', or 'leader'. Transitions between states are managed
 *     by internal  timeouts (election timeouts and heartbeats).
 *  2. command acceptance (replication): clients submit commands via 'apply()' methods.
 *     If the server is the leader, it packages the command into a log entry ('entry'),
 *     saves it locally via 'io', and concurrently broadcasts it to the remaining
 *     nodes via AppendEntries RPCs.
 *  3. commit and application to fsm: as soon as an entry is acknowledged by a
 *     majority of voting nodes, the leader updates its commit index and sequentially
 *     applies ('apply') the command to the 'fsm'. The index of the last applied
 *     command can be retrieved via 'last_applied_index()'.
 *  4. message processing: all incoming network activity (AppendEntries requests/responses,
 *     RequestVote, InstallSnapshot) is passed from the application's network
 *     layer to the core via 'handle_message()'.
 *
 *  Initialization and startup (crucial architectural rule):
 *  There are two cold-start scenarios for a node, which are mutually exclusive:
 *  - Initial bootstrap of a new cluster: the 'bootstrap()' method is called. It
 *    performs the initial configuration layout for an absolute fresh database.
 *    Internally, 'bootstrap()' automatically calls 'init()', so calling 'init()'
 *    manually before or after it is strictly forbidden (doing so will lead to
 *    state corruption).
 *  - Restart of an existing node or start to join cluster: the 'init()' method
 *    is called. It reads previously persisted data, such as logs, terms, and
 *    snapshots, from the disk. The 'bootstrap()' method must not be called in
 *    this scenario.
 */
class server final
{
public:
    using allocator_type = std::allocator<server>; //!< Memory allocator for creating instances of the server class.
    using ptr = std::shared_ptr<server>;           //!< Smart pointer to a Raft server instance.

public:
    /// \brief  Constructor for the local Raft server.
    /// \param  id - unique numerical identifier of this server.
    /// \param  p_io - pointer to the persistent and network I/O subsystem.
    /// \param  p_fsm - pointer to the deterministic finite state machine of business logic.
    /// \param  p_handler - logger for recording system events and protocol tracing.
    /// \param  is_stop_fn - external predicate function for global application shutdown control.
    /// \param  alloc - memory allocator instance (optional).
    server(const server_id_t id, const io::ptr& p_io, const fsm::ptr p_fsm, logging_handler::ptr p_handler,
           const is_stop_fn_t& is_stop_fn, const allocator_type& alloc = allocator_type());

    /// \brief  Server destructor.
    ~server();

    /// \brief  Dynamically add a new server to the current cluster configuration.
    /// \param  id - identifier of the server to be added.
    /// \param  address - network address of the server to be added.
    /// \param  is_voter - whether the new server will participate in voting (or remain a non-voter/learner).
    /// \note   This method must only be called on the Leader. The change is replicated via the Raft log.
    void add(const server_id_t id, const std::string& address, const bool is_voter);

    /// \brief  Helper template method for submitting trivially copyable data types (POD).
    /// \param  var - reference to the constant object containing the command data.
    /// \details    Serializes the 'var' object into a raw byte buffer and passes it to the main 'apply' method.
    /// \tparam     T - type of data to be submitted. Must satisfy the 'std::is_trivially_copyable' trait.
    template<typename T, typename = typename std::enable_if<std::is_trivially_copyable<T>::value>::type>
    void apply(const T& var)
    {
        const char* ptr = reinterpret_cast<const char*>(&var);
        std::vector<char> buf(ptr, ptr + sizeof(T));
        return apply(std::move(buf));
    }

    /// \brief  Accept a new command from a client for replication and application
    ///     to the finite state machine (FSM).
    /// \param  buf - buffer containing the binary data of the business logic command.
    /// \warning    Calling this method is only meaningful on a server in the leader
    ///     role. If the node is not a leader, the command will be ignored or dropped
    ///     (depending on the internal routing implementation).
    void apply(buffer_type buf);

    /// \brief  Initial bootstrap of a completely brand new Raft cluster.
    /// \param  cfg - general configuration settings for the current server.
    /// \param  cluster_cfg - initial list of all servers included in the cluster
    ///     at launch time.
    /// \return true if bootstrap and internal initialization were successful, false
    ///     if data failed to be written or if the node has already been initialized before.
    /// \details    Used to lay out the metadata for the first node or to explicitly
    ///     specify the initial cluster composition when no records exist on disk yet.
    /// \warning    Mutual exclusion with init(): this method internally automatically
    ///     calls the 'init()' method. If you are calling 'bootstrap()', calling
    ///     'init()' beforehand or additionally is not required and forbidden.
    bool bootstrap(const config& cfg, const cluster_config& cluster_cfg);

    /// \brief  Full deinitialization of the server. Releases resources and closes
    ///     internal execution contexts.
    void deinit();

    /// \brief  Get the identifier of the current server.
    /// \return t Unique id of the node.
    server_id_t id() const { return m_id; }

    /// \brief  Initialize the server based on existing persistent log data and
    ///     snapshots.
    /// \param  cfg - configuration of local parameters and timings for the server.
    /// \return true if the state was successfully read from 'io' and the server
    ///     is ready to be launched ('start()'), false in case of log corruption
    ///     or configuration incompatibility.
    /// \details    Called during a planned or emergency node restart to recover
    ///     the state from disk.
    /// \warning    Mutual exclusion with bootstrap(): if the server is starting
    ///     "from scratch" and the 'bootstrap()' method is called for it, then
    ///     calling 'init()' separately is not necessary, as 'bootstrap()' will
    ///     handle the internal initialization on its own.
    bool init(const config& cfg);

    /// \brief  Check whether the server is in the candidate role.
    /// \return true if the node has initiated an election and is collecting votes.
    bool is_candidate() const;

    /// \brief  Check whether the server is in the follower role.
    /// \return true if the node is accepting heartbeats/logs from a legitimate Leader.
    bool is_follower() const;

    /// \brief  Check whether the server initialization process ('init' or 'bootstrap') has completed.
    /// \return true if the server is ready for the 'start()' method call.
    bool is_inited() const;

    /// \brief  Check whether this server is the current leader of the cluster.
    /// \return true iif the node successfully won the election and is coordinating replication.
    bool is_leader() const;

    /// \brief  Check whether the server is in the process of stopping or is already stopped.
    /// \return true if the server is stopping or stopped.
    bool is_stop() const { return m_is_stop || m_is_stop_fn(); }

    /// \brief  Submit a request to dynamically join an existing cluster.
    /// \param  cluster_addr - network address of any known active node (preferably
    ///     the leader) of the existing cluster.
    void join(std::string cluster_addr) const;

    /// \brief  Entry point for all incoming network packets (Raft RPC protocol messages).
    /// \param  msg_buf - constant memory slice containing the raw binary data of the incoming message.
    /// \details    The application's network transport must call this method upon
    ///     receiving any data addressed to this Raft server. A serialized RPC
    ///     packet is expected inside the buffer.
    void handle_message(const inbuffer_type& msg_buf);

    /// \brief  Get the index of the last log entry successfully applied to the finite state machine (FSM).
    /// \return Index of the last applied log entry.
    index_t last_applied_index() const;

    /// \brief  Get the identifier of the currently known cluster Leader.
    /// \return Leader id, or 'gk_invalid_id' if no leader has been determined in the current term yet.
    server_id_t leader_id() const;

    /// \brief  Update the server's configuration parameters "on the fly" without a full shutdown.
    /// \param  cfg - new configuration structure with updated timings or logging preferences.
    /// \return true if the new parameters were successfully applied by the internal context.
    bool reconfigure(const config& cfg);

    /// \brief  Dynamically remove a server from the cluster configuration.
    /// \param  id - identifier of the node being removed from the cluster.
    /// \note   This method must only be called on the Leader. The change is replicated via the Raft log.
    void remove(const server_id_t id);

    /// \brief  Start the internal Raft scheduler threads (activate election and heartbeat timers).
    /// \return true if worker threads are successfully started, false if the server is not initialized or is already running.
    /// \note   The server must be successfully initialized via init() or bootstrap().
    bool start();

    /// \brief  Issue a command to stop all background processes and threads of the server.
    void stop();

private:
    using context_ptr = std::shared_ptr<details::context>; //!< Smart pointer to the internal opaque execution context (Pimpl-like structure).

private:
    /// \brief  Internal method to read/load state from the context.
    /// \param  ctx - reference to the implementation details context object.
    /// \return true upon successful loading.
    static bool load(details::context& ctx);

private:
    const server_id_t m_id;     //!< Unique identifier of this server.
    allocator_type m_alloc;     //!< Memory allocator used by the server.

    is_stop_fn_t m_is_stop_fn;  //!< External stop checking function.
    std::atomic_bool m_is_stop; //!< Atomic internal server stop flag.

    context_ptr m_p_ctx;        //!< Pointer to the private context holding Raft state logic.
};

} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_SERVER_H_ */

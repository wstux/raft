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

#include <gtest/gtest.h>

#include "raft/details/log/log.h"

namespace {

class raft_log : public ::testing::Test
{
public:
    virtual void SetUp() override {}
    virtual void TearDown() override {}
};

} // <anonymous> namespace

/**
 *  \brief  Checking the request for entries from an empty log.
 *
 *  \details    The test requests entries from an empty log. The test is considered
 *  successful if an empty list of entries is returned.
 */
TEST_F(raft_log, acquire_empty_log)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.offset = 1;
    log.snapshot.last_index = 0;
    log.snapshot.last_term = 0;

    EXPECT_TRUE(log.entries.empty()) << "Entries size in log: " << log.entries.size();

    for (raft::index_t i = 1; i < 40; i += 10) {
        raft::entry::list entries = log.acquire(i);
        EXPECT_TRUE(entries.empty()) << "Acquired entries size is " << entries.size() << " with begin index " << i;
    }
}

/**
 *  \brief  Checking the request for entries from an log.
 *
 *  \details    The test requests entries from an log. The test is considered
 *  successful if a non-empty list of records is returned. The number of records
 *  returned must match the expected value.
 */
TEST_F(raft_log, acquire)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.offset = 1;
    log.snapshot.last_index = 0;
    log.snapshot.last_term = 0;

    for (size_t i = 1; i < 11; ++i) {
        raft::entry::ptr p_entry = std::make_shared<raft::entry>();
        p_entry->term = 0;
        p_entry->type = (i % 2 == 0) ? raft::entry_type::change : raft::entry_type::command;
        log.entries.emplace(i, p_entry);
    }

    EXPECT_TRUE(log.entries.size() == 10) << "Entries size in log: " << log.entries.size();

    raft::entry::list entries = log.acquire(1);
    EXPECT_TRUE(entries.size() == 10) << "Acquired entries size is " << entries.size()
        << " with begin index 1, but expected size is 10";

    entries = log.acquire(5);
    EXPECT_TRUE(entries.size() == 6) << "Acquired entries size is " << entries.size()
        << " with begin index 5, but expected size is 6";

    entries = log.acquire(10);
    EXPECT_TRUE(entries.size() == 1) << "Acquired entries size is " << entries.size()
        << " with begin index 10, but expected size is 1";
}

/**
 *  \brief  Checking for new entries added to the log.
 *
 *  \details    The test adds new entries to the log. The test is considered
 *  successful if entries are successfully added to the log with valid indexes.
 *  Indexes must be sequentially increasing and without gaps. The minimum index
 *  must be equal to the 'offset + 1', and the maximum index must be equal to
 *  the 'offset + number_of_added_entries + 1'.
 */
TEST_F(raft_log, append)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.offset = 1;
    log.snapshot.last_index = 0;
    log.snapshot.last_term = 0;

    EXPECT_TRUE(log.entries.empty()) << "Entries size in log: " << log.entries.size();

    for (size_t i = 0; i < 3; ++i) {
        raft::entry::ptr p_entry = std::make_shared<raft::entry>();
        p_entry->term = 0;
        p_entry->type = (i % 2 == 0) ? raft::entry_type::change : raft::entry_type::command;
        log.append(p_entry);
    }

    EXPECT_TRUE(log.entries.size() == 3) << "Entries size in log: " << log.entries.size();

    for (raft::index_t i = 2; i < 5; ++i) {
        raft_log::entry_map::iterator it = log.entries.find(i);
        EXPECT_TRUE(it != log.entries.end()) << "Entry with index " << i << " has not been found";
    }
}

/**
 *  \brief  Checking for new entries with change type added to the log.
 *
 *  \details    The test adds new entry to the log. The test is considered
 *  successful if entry is successfully added to the log with valid index.
 *  Index must be sequentially increasing and without gaps. The index must
 *  be equal to the 'offset + 1' with expected term and type.
 */
TEST_F(raft_log, append_change)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.offset = 1;
    log.snapshot.last_index = 0;
    log.snapshot.last_term = 0;

    EXPECT_TRUE(log.entries.empty()) << "Entries size in log: " << log.entries.size();

    log.append_change(11, raft::cluster_config());

    EXPECT_TRUE(log.entries.size() == 1) << "Entries size in log: " << log.entries.size();

    raft_log::entry_map::iterator it = log.entries.find(2);
    ASSERT_TRUE(it != log.entries.end()) << "Entry has not been found";

    raft::entry::ptr p_entry = it->second;
    ASSERT_TRUE(p_entry.get() != nullptr) << "Entry is null";

    EXPECT_TRUE(p_entry->term == 11) << "Entry term is " << p_entry->term;
    EXPECT_TRUE(p_entry->type == raft::entry_type::change) << "Entry is not have change type";
}

/**
 *  \brief  Checking for new entries with command type added to the log.
 *
 *  \details    The test adds new entry to the log. The test is considered
 *  successful if entry is successfully added to the log with valid index.
 *  Index must be sequentially increasing and without gaps. The index must
 *  be equal to the 'offset + 1' with expected term and type.
 */
TEST_F(raft_log, append_command)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.offset = 1;
    log.snapshot.last_index = 0;
    log.snapshot.last_term = 0;

    EXPECT_TRUE(log.entries.empty()) << "Entries size in log: " << log.entries.size();

    log.append_command(11, raft::buffer_type());

    EXPECT_TRUE(log.entries.size() == 1) << "Entries size in log: " << log.entries.size();

    raft_log::entry_map::iterator it = log.entries.find(2);
    ASSERT_TRUE(it != log.entries.end()) << "Entry has not been found";

    raft::entry::ptr p_entry = it->second;
    ASSERT_TRUE(p_entry.get() != nullptr) << "Entry is null";

    EXPECT_TRUE(p_entry->term == 11) << "Entry term is " << p_entry->term;
    EXPECT_TRUE(p_entry->type == raft::entry_type::command) << "Entry is not have command type";
}

/**
 *  \brief  Checking receipt of entries with valid indexes from the log.
 *
 *  \details    The test retrieves entries from the log. The test is considered
 *  successful if an entry with a valid index is successfully retrieved from the
 *  log, and the retrieved entry has the expected term and type.
 */
TEST_F(raft_log, get_entry_with_valid_index)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.offset = 1;
    log.snapshot.last_index = 0;
    log.snapshot.last_term = 0;

    EXPECT_TRUE(log.entries.empty()) << "Entries size in log: " << log.entries.size();

    log.append_command(11, raft::buffer_type());

    raft::entry::ptr p_entry = log.get_entry(2);
    ASSERT_TRUE(p_entry.get() != nullptr) << "Entry is null";
    EXPECT_TRUE(p_entry->term == 11) << "Entry term is " << p_entry->term;
    EXPECT_TRUE(p_entry->type == raft::entry_type::command) << "Entry is not have command type";
}

/**
 *  \brief  Checking receipt of entries with invalid indexes from the log.
 *
 *  \details    The test retrieves entries from the log. The test is considered
 *  successful if an entry with an invalid index is successfully retrieved from
 *  the log, and the entry is null.
 */
TEST_F(raft_log, get_entry_with_invalid_index)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.offset = 1;
    log.snapshot.last_index = 0;
    log.snapshot.last_term = 0;

    EXPECT_TRUE(log.entries.empty()) << "Entries size in log: " << log.entries.size();

    log.append_command(11, raft::buffer_type());

    raft::entry::ptr p_entry = log.get_entry(3);
    ASSERT_TRUE(p_entry.get() == nullptr) << "Entry is not null";
}

/**
 *  \brief  Checking if the latest log index has been received.
 *
 *  \details    The test verifies the validity of obtaining the last log index.
 *  Test cases:
 *  1) with the initial offset and an empty log, the value of the last index is
 *      equivalent to the offset value;
 *  2) after adding a log entry, the value of the last index is equivalent to
 *      the offset value + the number of log entries.
 */
TEST_F(raft_log, last_index)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.offset = 1;
    log.snapshot.last_index = 0;
    log.snapshot.last_term = 0;

    EXPECT_TRUE(log.entries.empty()) << "Entries size in log: " << log.entries.size();

    ASSERT_TRUE(log.last_index() == 1) << "Last index is " << log.last_index();

    log.append_command(11, raft::buffer_type());
    ASSERT_TRUE(log.last_index() == 2) << "Last index is " << log.last_index();

    log.snapshot.last_index = 1;
    ASSERT_TRUE(log.last_index() == 2) << "Last index is " << log.last_index();

    log.offset = 2;
    log.snapshot.last_index = 2;
    ASSERT_TRUE(log.last_index() == 3) << "Last index is " << log.last_index();
}

/**
 *  \brief  Checking if the latest log term has been received.
 *
 *  \details    The test verifies the validity of obtaining the last log term.
 *  The test is considered successful if a valid term is returned that matches
 *  the term in the last entry.
 */
TEST_F(raft_log, last_term)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.offset = 1;
    log.snapshot.last_index = 2;
    log.snapshot.last_term = 1;

    raft::entry::ptr p_entry = std::make_shared<raft::entry>();
    p_entry->term = 1;
    p_entry->type = raft::entry_type::change;
    log.entries.emplace(2, p_entry);

    ASSERT_TRUE(log.last_term() == 1) << "Last term is " << log.last_term();

    log.append_command(11, raft::buffer_type());
    ASSERT_TRUE(log.last_term() == 11) << "Last term is " << log.last_term();
}

/**
 *  \brief  Checking the loading of the basic log state.
 *
 *  \details    The test is considered successful if the last index of the snapshot
 *  matches the transferred one, the last term of the snapshot matches the transferred
 *  one, and the offset matches the starting index - 1.
 */
TEST_F(raft_log, load)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.load(2, 3, 3);

    EXPECT_TRUE(log.entries.empty()) << "Entries size in log: " << log.entries.size();
    EXPECT_TRUE(log.snapshot.last_index == 2) << "Last snapshot index: " << log.snapshot.last_index;
    EXPECT_TRUE(log.snapshot.last_term == 3) << "Last snapshot term: " << log.snapshot.last_term;
    EXPECT_TRUE(log.offset == 2) << "Offset: " << log.offset;
}

/**
 *  \brief  Checking the restoration of the log state.
 *
 *  \details    The test is considered successful if the log state after restore
 *  is as follows:
 *  - extra entries are removed;
 *  - the last index and term match those to which restore was performed;
 *  - the index and term match those to which restore was performed;
 *  - the offset matches the index to which restore was performed.
 */
TEST_F(raft_log, restore)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.load(0, 0, 1);
    for (size_t i = 0; i < 3; ++i) {
        log.append_change(i + 1, raft::cluster_config());
    }
    EXPECT_TRUE(log.entries.size() == 3) << "Entries size in log: " << log.entries.size();
    EXPECT_TRUE(log.last_index() == 3) << "Last log index: " << log.last_index();
    EXPECT_TRUE(log.snapshot.last_index == 0) << "Last snapshot index: " << log.snapshot.last_index;
    EXPECT_TRUE(log.snapshot.last_term == 0) << "Last snapshot term: " << log.snapshot.last_term;
    EXPECT_TRUE(log.offset == 0) << "Offset: " << log.offset;

    log.restore(2, 1);
    EXPECT_TRUE(log.entries.size() == 0) << "Entries size in log: " << log.entries.size();
    EXPECT_TRUE(log.last_index() == 2) << "Last log index: " << log.last_index();
    EXPECT_TRUE(log.snapshot.last_index == 2) << "Last snapshot index: " << log.snapshot.last_index;
    EXPECT_TRUE(log.snapshot.last_term == 1) << "Last snapshot term: " << log.snapshot.last_term;
    EXPECT_TRUE(log.offset == 2) << "Offset: " << log.offset;
}

/**
 *  \brief  Checking snapshot creation.
 *
 *  \details    The test is considered successful if the log state after recovery
 *  meets the following parameters:
 *  - redundant entries with an index less than the snapshot index have been
 *      removed;
 *  - the snapshot index matches the index at which the snapshot was created;
 *  - the snapshot term matches the one of the entry with the index at which the
 *      snapshot was created.
 *  - the offset value matches the index-1 at which the snapshot was created.
 */
TEST_F(raft_log, take_snapshot)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.load(0, 0, 1);
    for (size_t i = 0; i < 3; ++i) {
        log.append_change(1, raft::cluster_config());
    }
    EXPECT_TRUE(log.entries.size() == 3) << "Entries size in log: " << log.entries.size();
    EXPECT_TRUE(log.last_index() == 3) << "Last log index: " << log.last_index();
    EXPECT_TRUE(log.snapshot.last_index == 0) << "Last snapshot index: " << log.snapshot.last_index;
    EXPECT_TRUE(log.snapshot.last_term == 0) << "Last snapshot term: " << log.snapshot.last_term;
    EXPECT_TRUE(log.offset == 0) << "Offset: " << log.offset;

    log.take_snapshot(2);
    EXPECT_TRUE(log.entries.size() == 2) << "Entries size in log: " << log.entries.size();
    EXPECT_TRUE(log.last_index() == 3) << "Last log index: " << log.last_index();
    EXPECT_TRUE(log.snapshot.last_index == 2) << "Last snapshot index: " << log.snapshot.last_index;
    EXPECT_TRUE(log.snapshot.last_term == 1) << "Last snapshot term: " << log.snapshot.last_term;
    EXPECT_TRUE(log.offset == 1) << "Offset: " << log.offset;
}

/**
 *  \brief  Checking the query term from the log.
 *
 *  \details    The test is considered successful if:
 *  1) a query for a term with an index equal to the snapshot index returns the
 *      snapshot term;
 *  2) a query for a term with an index returns the term stored in the entry
 *      with that index;
 *  3) a query for a term with an invalid index returns 0.
 */
TEST_F(raft_log, term)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.offset = 1;
    log.snapshot.last_index = 2;
    log.snapshot.last_term = 2;
    for (size_t i = 1; i < 5; ++i) {
        raft::entry::ptr p_entry = std::make_shared<raft::entry>();
        p_entry->term = i;
        p_entry->type = raft::entry_type::change;
        log.entries.emplace(i, p_entry);
    }

    EXPECT_TRUE(log.term(log.snapshot.last_index) == log.snapshot.last_term) << log.term(log.snapshot.last_index);
    EXPECT_TRUE(log.term(3) == 3) << log.term(3);
    EXPECT_TRUE(log.term(4) == 4) << log.term(4);
    EXPECT_TRUE(log.term(5) == 0) << log.term(5);
}

/**
 *  \brief  Checking log truncation.
 *
 *  \details    The test is considered successful if, after truncating the log,
 *  all entries whose index is higher than or equal to the truncation index are
 *  removed.
 */
TEST_F(raft_log, truncate)
{
    namespace raft = ::wstux::raft;
    using raft_log = raft::details::log::store;

    raft_log log;
    log.load(0, 0, 1);
    for (size_t i = 0; i < 5; ++i) {
        log.append_change(i + 1, raft::cluster_config());
    }

    EXPECT_TRUE(log.entries.size() == 5) << "Entries size in log: " << log.entries.size();
    EXPECT_TRUE(log.last_index() == 5) << "Last log index: " << log.last_index();
    EXPECT_TRUE(log.last_term() == 5) << "Last log trm: " << log.last_term();

    log.truncate(3);
    EXPECT_TRUE(log.entries.size() == 2) << "Entries size in log: " << log.entries.size();
    EXPECT_TRUE(log.last_index() == 2) << "Last log index: " << log.last_index();
    EXPECT_TRUE(log.last_term() == 2) << "Last log trm: " << log.last_term();
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

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

#include "raft/details/role/role.h"

namespace {

class raft_role : public ::testing::Test
{
public:
    virtual void SetUp() override {}
    virtual void TearDown() override {}
};

std::string to_string(const ::wstux::raft::details::role::state& role)
{
    return std::string(role.str());
}

} // <anonymous> namespace

/**
 *  \brief  Checking the follower role.
 */
TEST_F(raft_role, follower)
{
    namespace raft = ::wstux::raft;
    using raft_role = raft::details::role::state;

    raft_role role;
    role.role = raft::details::role::role_type::follower;

    EXPECT_TRUE(role.is_follower()) << "role: " << role.role;
    EXPECT_FALSE(role.is_candidate()) << "role: " << role.role;
    EXPECT_FALSE(role.is_leader()) << "role: " << role.role;
    EXPECT_TRUE(to_string(role) == "follower") << "role: " << role.str();
}

/**
 *  \brief  Checking the candidate role.
 */
TEST_F(raft_role, candidate)
{
    namespace raft = ::wstux::raft;
    using raft_role = raft::details::role::state;

    raft_role role;
    role.role = raft::details::role::role_type::candidate;

    EXPECT_FALSE(role.is_follower()) << "role: " << role.role;
    EXPECT_TRUE(role.is_candidate()) << "role: " << role.role;
    EXPECT_FALSE(role.is_leader()) << "role: " << role.role;
    EXPECT_TRUE(to_string(role) == "candidate") << "role: " << role.str();
}

/**
 *  \brief  Checking the leader role.
 */
TEST_F(raft_role, leader)
{
    namespace raft = ::wstux::raft;
    using raft_role = raft::details::role::state;

    raft_role role;
    role.role = raft::details::role::role_type::leader;

    EXPECT_FALSE(role.is_follower()) << "role: " << role.role;
    EXPECT_FALSE(role.is_candidate()) << "role: " << role.role;
    EXPECT_TRUE(role.is_leader()) << "role: " << role.role;
    EXPECT_TRUE(to_string(role) == "leader") << "role: " << role.str();
}

/**
 *  \brief  Checking the undefined role.
 */
TEST_F(raft_role, undefined)
{
    namespace raft = ::wstux::raft;
    using raft_role = raft::details::role::state;

    raft_role role;
    role.role = (raft::details::role::role_type)255;

    EXPECT_FALSE(role.is_follower()) << "role: " << role.role;
    EXPECT_FALSE(role.is_candidate()) << "role: " << role.role;
    EXPECT_FALSE(role.is_leader()) << "role: " << role.role;
    EXPECT_TRUE(to_string(role) == "undefined") << "role: " << role.str();
}

TEST_F(raft_role, has_leader)
{
    namespace raft = ::wstux::raft;
    using raft_role = raft::details::role::state;

    raft_role role;
    role.role = raft::details::role::role_type::leader;

    EXPECT_FALSE(role.has_leader());

    role.role = raft::details::role::role_type::follower;
    role.follower_state.leader_id = 1;
    EXPECT_TRUE(role.has_leader());
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

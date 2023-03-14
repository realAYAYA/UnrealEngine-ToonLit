// Copyright Epic Games, Inc. All Rights Reserved.

#include "pmatests/Defs.h"

#include "pma/utils/ManagedInstance.h"

namespace pmatests {

namespace {

struct Stats {
    int constructed;
    int destructed;
};

class BaseType {
    public:
        virtual ~BaseType() = default;
};

class ManagableType : public BaseType {
    public:
        ManagableType(Stats& stats_) : stats{&stats_} {
            stats->constructed += 1;
        }

        ~ManagableType() {
            stats->destructed += 1;
        }

        ManagableType(const ManagableType&) = default;
        ManagableType& operator=(const ManagableType&) = default;

        ManagableType(ManagableType&&) = default;
        ManagableType& operator=(ManagableType&&) = default;

    private:
        Stats* stats;
};

}  // namespace

}  // namespace pmatests

TEST(ManagedInstanceTest, UniqueInstance) {
    pma::DefaultMemoryResource memRes;
    pmatests::Stats stats{};
    ASSERT_EQ(stats.constructed, 0);
    ASSERT_EQ(stats.destructed, 0);
    {
        auto ptr = pma::UniqueInstance<pmatests::ManagableType, pmatests::BaseType>::with(&memRes).create(stats);
        ASSERT_EQ(stats.constructed, 1);
        ASSERT_EQ(stats.destructed, 0);
    }
    ASSERT_EQ(stats.constructed, 1);
    ASSERT_EQ(stats.destructed, 1);
}

TEST(ManagedInstanceTest, SharedInstance) {
    pma::DefaultMemoryResource memRes;
    pmatests::Stats stats{};
    ASSERT_EQ(stats.constructed, 0);
    ASSERT_EQ(stats.destructed, 0);
    {
        std::shared_ptr<pmatests::BaseType> ptr;
        {
            auto ptr2 = pma::SharedInstance<pmatests::ManagableType, pmatests::BaseType>::with(&memRes).create(stats);
            ptr = ptr2;
            ASSERT_EQ(stats.constructed, 1);
            ASSERT_EQ(stats.destructed, 0);
        }
        ASSERT_EQ(stats.constructed, 1);
        ASSERT_EQ(stats.destructed, 0);
    }
    ASSERT_EQ(stats.constructed, 1);
    ASSERT_EQ(stats.destructed, 1);
}

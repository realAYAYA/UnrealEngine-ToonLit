
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE_1_0.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#ifndef CATCH_TEST_GROUP_EVENT_REGISTRY_IMPL_HPP_INCLUDED
#define CATCH_TEST_GROUP_EVENT_REGISTRY_IMPL_HPP_INCLUDED

#include <catch2/interfaces/catch_interfaces_group.hpp>
#include <catch2/internal/catch_unique_ptr.hpp>
#include <catch2/catch_test_group_info.hpp>

#include <vector>
#include <map>

namespace Catch {

    class TestGroupEventRegistry : public ITestGroupEventRegistry {
    public:
        ~TestGroupEventRegistry() override = default;

        void registerTestGroupEvent( std::string const& group, GroupLifecycleStage const& stage, IGroupLifecycleEventInvoker* invoker );
    };

    ///////////////////////////////////////////////////////////////////////////

    class GroupLifecycleEventInvokerAsFunction final : public IGroupLifecycleEventInvoker {
        using GroupLifecycleEventType = void(*)();
        GroupLifecycleEventType m_groupLifecycleEventAsFunction;
        GroupLifecycleStage m_groupLifecycleStage;
        bool m_wasInvoked = false;
    public:
        GroupLifecycleEventInvokerAsFunction(GroupLifecycleEventType groupLifecycleEventAsFunction, GroupLifecycleStage groupLifecycleStage) noexcept:
            m_groupLifecycleEventAsFunction(groupLifecycleEventAsFunction),
            m_groupLifecycleStage(groupLifecycleStage) {}

        ~GroupLifecycleEventInvokerAsFunction() override {
		}

        virtual GroupLifecycleStage getStage() const override {
            return m_groupLifecycleStage;
        }

		virtual bool wasInvoked() const override {
            return m_wasInvoked;
        }
        virtual void invoke() override {
            m_groupLifecycleEventAsFunction();
            m_wasInvoked = true;
        }
    };

    ///////////////////////////////////////////////////////////////////////////


} // end namespace Catch


#endif // CATCH_TEST_GROUP_EVENT_REGISTRY_IMPL_HPP_INCLUDED

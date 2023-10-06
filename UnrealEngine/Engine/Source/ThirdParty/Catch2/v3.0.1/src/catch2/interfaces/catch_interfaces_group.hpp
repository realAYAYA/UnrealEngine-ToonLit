
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE_1_0.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#ifndef CATCH_INTERFACES_GROUP_HPP_INCLUDED
#define CATCH_INTERFACES_GROUP_HPP_INCLUDED

namespace Catch {

    enum GroupLifecycleStage {
        BeforeAllTests, AfterAllTests, BeforeEachTest, AfterEachTest, BeforeGlobal, AfterGlobal
    };

    class IGroupLifecycleEventInvoker {
    public:
        virtual GroupLifecycleStage getStage() const = 0;
        virtual void invoke () = 0;
        virtual bool wasInvoked() const = 0;
        virtual ~IGroupLifecycleEventInvoker(); // = default
    };

    class ITestGroupEventRegistry {
    public:
        virtual ~ITestGroupEventRegistry(); // = default
    };
}

#endif // CATCH_INTERFACES_GROUP_HPP_INCLUDED
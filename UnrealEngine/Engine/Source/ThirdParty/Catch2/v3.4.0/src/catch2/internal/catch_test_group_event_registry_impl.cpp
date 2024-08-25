
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE_1_0.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#include <catch2/internal/catch_test_group_event_registry_impl.hpp>

namespace Catch {

   void TestGroupEventRegistry::registerTestGroupEvent( std::string const& group, GroupLifecycleStage const& stage, IGroupLifecycleEventInvoker* invoker ) {
        TestGroupHandle* groupHandle = TestGroupHandleFactory::getMutable().getGroupHandle( group );
        switch (stage)
        {
            case GroupLifecycleStage::BeforeAllTests:
                groupHandle->setBeforeAllTestsInvoker(invoker);
                break;
            case GroupLifecycleStage::AfterAllTests:
                groupHandle->setAfterAllTestsInvoker(invoker);
                break;
            case GroupLifecycleStage::BeforeEachTest:
                groupHandle->setBeforeEachTestInvoker(invoker);
                break;
            case GroupLifecycleStage::AfterEachTest:
                groupHandle->setAfterEachTestInvoker(invoker);
                break;
			case GroupLifecycleStage::BeforeGlobal:
                groupHandle->setBeforeAllGlobalInvoker(invoker);
                break;
            case GroupLifecycleStage::AfterGlobal:
                groupHandle->setAfterAllGlobalInvoker(invoker);
                break;
        }
   }

} // end namespace Catch

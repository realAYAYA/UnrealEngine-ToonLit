
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE_1_0.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#include <catch2/catch_test_group_info.hpp>
#include <catch2/internal/catch_enforce.hpp>

namespace Catch {

    void TestGroupHandle::setBeforeAllTestsInvoker(IGroupLifecycleEventInvoker* beforeAllTestsInvoker) {
        if (!m_beforeAllTestsInvoker)
        {
            m_beforeAllTestsInvoker = beforeAllTestsInvoker;
        }
        else
        {
            CATCH_RUNTIME_ERROR("Before all tests handler already set for group " << m_group);
        }
    }

    void TestGroupHandle::setAfterAllTestsInvoker(IGroupLifecycleEventInvoker* afterAllTestsInvoker) {
        if (!m_afterAllTestsInvoker)
        {
            m_afterAllTestsInvoker = afterAllTestsInvoker;
        }
        else
        {
            CATCH_RUNTIME_ERROR("After all tests handler already set for group " << m_group);
        }
    }

    void TestGroupHandle::setBeforeEachTestInvoker(IGroupLifecycleEventInvoker* beforeEachTestInvoker) {
        if (!m_beforeEachTestInvoker)
        {
            m_beforeEachTestInvoker = beforeEachTestInvoker;
        }
        else
        {
            CATCH_RUNTIME_ERROR("Before each test handler already set for group " << m_group);
        }
    }

    void TestGroupHandle::setAfterEachTestInvoker(IGroupLifecycleEventInvoker* afterEachTestInvoker) {
        if (!m_afterEachTestInvoker)
        {
            m_afterEachTestInvoker = afterEachTestInvoker;
        }
        else
        {
            CATCH_RUNTIME_ERROR("After each test handler already set for group " << m_group);
        }
    }

	  void TestGroupHandle::setBeforeAllGlobalInvoker(IGroupLifecycleEventInvoker* beforeAllGlobalInvoker) {
        if (!m_beforeAllGlobalInvoker)
        {
            m_beforeAllGlobalInvoker = beforeAllGlobalInvoker;
        }
        else
        {
            CATCH_RUNTIME_ERROR("Before all global handler already set for group " << m_group);
        }
    }

    void TestGroupHandle::setAfterAllGlobalInvoker(IGroupLifecycleEventInvoker* afterAllGlobalInvoker) {
        if (!m_afterAllGlobalInvoker)
        {
            m_afterAllGlobalInvoker = afterAllGlobalInvoker;
        }
        else
        {
            CATCH_RUNTIME_ERROR("After all global handler already set for group " << m_group);
        }
    }

    IGroupLifecycleEventInvoker* TestGroupHandle::getBeforeAllTestsInvoker() const {
        return m_beforeAllTestsInvoker;
    };

    IGroupLifecycleEventInvoker* TestGroupHandle::getAfterAllTestsInvoker() const {
		return m_afterAllTestsInvoker;
    };

    IGroupLifecycleEventInvoker* TestGroupHandle::getBeforeEachTestInvoker() const {
        return m_beforeEachTestInvoker;
    };

    IGroupLifecycleEventInvoker* TestGroupHandle::getAfterEachTestInvoker() const {
        return m_afterEachTestInvoker;
    };

	IGroupLifecycleEventInvoker* TestGroupHandle::getBeforeAllGlobalInvoker() const {
        return m_beforeAllGlobalInvoker;
    };

    IGroupLifecycleEventInvoker* TestGroupHandle::getAfterAllGlobalInvoker() const {
		return m_afterAllGlobalInvoker;
    };

    std::string const& TestGroupHandle::getGroupName() const {
        return m_group;
    }
} // end namespace Catch

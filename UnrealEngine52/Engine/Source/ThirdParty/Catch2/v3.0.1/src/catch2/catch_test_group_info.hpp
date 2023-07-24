
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE_1_0.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#ifndef CATCH_TEST_GROUP_INFO_HPP_INCLUDED
#define CATCH_TEST_GROUP_INFO_HPP_INCLUDED

#include <catch2/interfaces/catch_interfaces_group.hpp>
#include <catch2/internal/catch_case_insensitive_comparisons.hpp>
#include <catch2/internal/catch_constants.hpp>
#include <catch2/internal/catch_stringref.hpp>
#include <catch2/internal/catch_singletons.hpp>
#include <catch2/internal/catch_unique_ptr.hpp>

#include <map>

namespace Catch {

    class TestGroupHandle {
        std::string m_group = DefaultGroup;
        IGroupLifecycleEventInvoker* m_beforeAllTestsInvoker = nullptr;
        IGroupLifecycleEventInvoker* m_afterAllTestsInvoker = nullptr;
        IGroupLifecycleEventInvoker* m_beforeEachTestInvoker = nullptr;
        IGroupLifecycleEventInvoker* m_afterEachTestInvoker = nullptr;
		IGroupLifecycleEventInvoker* m_beforeAllGlobalInvoker = nullptr;
        IGroupLifecycleEventInvoker* m_afterAllGlobalInvoker = nullptr;
    public:
        TestGroupHandle( std::string group ):
            m_group( group ) {}

		TestGroupHandle() {}

		~TestGroupHandle() {
			delete m_beforeAllTestsInvoker;
			delete m_afterAllTestsInvoker;
			delete m_beforeEachTestInvoker;
			delete m_afterEachTestInvoker;
			delete m_beforeAllGlobalInvoker;
			delete m_afterAllGlobalInvoker;
		}

        void setBeforeAllTestsInvoker(IGroupLifecycleEventInvoker* beforeAllTestsInvoker);
        void setAfterAllTestsInvoker(IGroupLifecycleEventInvoker* afterAllTestsInvoker);
        void setBeforeEachTestInvoker(IGroupLifecycleEventInvoker* beforeEachTestInvoker);
        void setAfterEachTestInvoker(IGroupLifecycleEventInvoker* afterEachTestInvoker);
		void setBeforeAllGlobalInvoker(IGroupLifecycleEventInvoker* beforeAllGlobalInvoker);
        void setAfterAllGlobalInvoker(IGroupLifecycleEventInvoker* afterAllGlobalInvoker);

        IGroupLifecycleEventInvoker* getBeforeAllTestsInvoker() const;
        IGroupLifecycleEventInvoker* getAfterAllTestsInvoker() const;
        IGroupLifecycleEventInvoker* getBeforeEachTestInvoker() const;
        IGroupLifecycleEventInvoker* getAfterEachTestInvoker() const;
		IGroupLifecycleEventInvoker* getBeforeAllGlobalInvoker() const;
        IGroupLifecycleEventInvoker* getAfterAllGlobalInvoker() const;

        std::string const& getGroupName() const;
    };

	typedef Detail::unique_ptr<TestGroupHandle> TestGroupHandlePtr;

    class TestGroupHandles {
    private:
         std::map<std::string, TestGroupHandle> m_groupHandles;
    public:
        TestGroupHandle* getGroupHandle( std::string group )
        {
            if ( m_groupHandles.find( group ) == m_groupHandles.end() ) {
                TestGroupHandle newGroupHandle( group );
                m_groupHandles.emplace(group, newGroupHandle);
            }
            return &m_groupHandles[ group ];
        }
    };

    typedef Singleton<TestGroupHandles> TestGroupHandleFactory;
}

#endif // CATCH_TEST_GROUP_INFO_HPP_INCLUDED

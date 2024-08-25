
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#ifndef CATCH_TEST_CASE_REGISTRY_IMPL_HPP_INCLUDED
#define CATCH_TEST_CASE_REGISTRY_IMPL_HPP_INCLUDED

#include <catch2/interfaces/catch_interfaces_testcase.hpp>
#include <catch2/interfaces/catch_interfaces_config.hpp>
#include <catch2/internal/catch_unique_ptr.hpp>

#include <vector>
#include <map>

namespace Catch {

    class IConfig;
    class ITestInvoker;
    class TestCaseHandle;
    class TestSpec;

    std::vector<TestCaseHandle> sortTests( IConfig const& config, std::vector<TestCaseHandle> const& unsortedTestCases );

    bool isThrowSafe( TestCaseHandle const& testCase, IConfig const& config );

    std::vector<TestCaseHandle> filterTests( std::vector<TestCaseHandle> const& testCases, TestSpec const& testSpec, IConfig const& config );
    std::vector<TestCaseHandle> const& getAllTestCasesSorted( IConfig const& config );

    class TestRegistry : public ITestCaseRegistry {
    public:
        ~TestRegistry() override = default;

        void registerTest( Detail::unique_ptr<TestCaseInfo> testInfo, Detail::unique_ptr<ITestInvoker> testInvoker );

        std::vector<TestCaseInfo*> const& getAllInfos() const override;
        std::map<StringRef, TestCaseHandleVector> const& getAllGroups() const override;
        std::vector<TestCaseHandle> const& getAllTests() const override;
        std::vector<TestCaseHandle> const& getAllTestsSorted( IConfig const& config ) const override;

    private:
        void updateAllTestHandles() const;

        std::vector<Detail::unique_ptr<TestCaseInfo>> m_owned_test_infos;
        // Keeps a materialized vector for `getAllInfos`.
        // We should get rid of that eventually (see interface note)
        std::vector<TestCaseInfo*> m_viewed_test_infos;

        std::map<StringRef, TestCaseHandleVector> m_groups;
        // all handles from all test groups
        mutable std::vector<TestCaseHandle> m_handles;
        std::vector<Detail::unique_ptr<ITestInvoker>> m_invokers;
        mutable TestRunOrder m_currentSortOrder = TestRunOrder::Declared;
        mutable std::vector<TestCaseHandle> m_sortedFunctions;
    };

    ///////////////////////////////////////////////////////////////////////////


} // end namespace Catch


#endif // CATCH_TEST_CASE_REGISTRY_IMPL_HPP_INCLUDED

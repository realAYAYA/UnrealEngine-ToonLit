
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE_1_0.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#include <catch2/internal/catch_test_case_registry_impl.hpp>

#include <catch2/internal/catch_context.hpp>
#include <catch2/internal/catch_enforce.hpp>
#include <catch2/interfaces/catch_interfaces_registry_hub.hpp>
#include <catch2/internal/catch_random_number_generator.hpp>
#include <catch2/internal/catch_run_context.hpp>
#include <catch2/internal/catch_sharding.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <catch2/catch_test_spec.hpp>
#include <catch2/internal/catch_move_and_forward.hpp>
#include <catch2/internal/catch_test_case_info_hasher.hpp>

#include <algorithm>
#include <set>

namespace Catch {

    std::vector<TestCaseHandle> sortTests( IConfig const& config, std::vector<TestCaseHandle> const& unsortedTestCases ) {
        switch (config.runOrder()) {
            case TestRunOrder::Declared:
                // by default sorted by group thanks to std::map being ordered by default
                return unsortedTestCases;

            case TestRunOrder::LexicographicallySorted: {
                std::vector<TestCaseHandle> sorted = unsortedTestCases;
                // The "<" comparator maintains test group order
                std::sort(
                    sorted.begin(),
                    sorted.end(),
                    []( TestCaseHandle const& lhs, TestCaseHandle const& rhs ) {
                        return lhs.getTestCaseInfo() < rhs.getTestCaseInfo();
                    }
                );
                return sorted;
            }
            case TestRunOrder::Randomized: {
                seedRng(config);
                using TestWithHash = std::pair<TestCaseInfoHasher::hash_t, TestCaseHandle>;

                TestCaseInfoHasher h{ config.rngSeed() };
                std::vector<TestWithHash> indexed_tests;
                indexed_tests.reserve(unsortedTestCases.size());

                for (auto const& handle : unsortedTestCases) {
                    indexed_tests.emplace_back(h(handle.getTestCaseInfo()), handle);
                }

                std::sort( indexed_tests.begin(),
                        indexed_tests.end(),
                        []( TestWithHash const& lhs, TestWithHash const& rhs ) {
                            if ( lhs.first == rhs.first ) {
                                return lhs.second.getTestCaseInfo() <
                                        rhs.second.getTestCaseInfo();
                            }
                            return lhs.first < rhs.first;
                        } );

                // sort again by group to maintain group order
                std::sort( indexed_tests.begin(),
                        indexed_tests.end(),
                        []( TestWithHash const& lhs, TestWithHash const& rhs ) {
                            return lhs.second.getTestCaseInfo().group <
                                    rhs.second.getTestCaseInfo().group;
                        } );

                std::vector<TestCaseHandle> randomized;
                randomized.reserve(indexed_tests.size());

                for (auto const& indexed : indexed_tests) {
                    randomized.push_back(indexed.second);
                }

                return randomized;
            }
        }

        CATCH_INTERNAL_ERROR("Unknown test order value!");
    }

    bool isThrowSafe( TestCaseHandle const& testCase, IConfig const& config ) {
        return !testCase.getTestCaseInfo().throws() || config.allowThrows();
    }

    bool matchTest( TestCaseHandle const& testCase, TestSpec const& testSpec, IConfig const& config ) {
        return testSpec.matches( testCase.getTestCaseInfo() ) && isThrowSafe( testCase, config );
    }

    void
    enforceNoDuplicateTestCases( std::vector<TestCaseHandle> const& tests ) {
#if !defined(CATCH_DUPLICATE_DONT_USE_COMPARATOR)
        auto testInfoCmp = []( TestCaseInfo const* lhs,
                               TestCaseInfo const* rhs ) {
            return *lhs < *rhs;
        };
        std::set<TestCaseInfo const*, decltype(testInfoCmp)> seenTests(testInfoCmp);
#else
        std::set<TestCaseInfo const*> seenTests;
#endif
        for ( auto const& test : tests ) {
            const auto infoPtr = &test.getTestCaseInfo();
            const auto prev = seenTests.insert( infoPtr );
            CATCH_ENFORCE(
                prev.second,
                "error: test case \"" << infoPtr->name << "\", with tags \""
                    << infoPtr->tagsAsString() << "\" already defined.\n"
                    << "\tFirst seen at " << ( *prev.first )->lineInfo << "\n"
                    << "\tRedefined at " << infoPtr->lineInfo );
        }
    }

    std::vector<TestCaseHandle> filterTests( std::vector<TestCaseHandle> const& testCases, TestSpec const& testSpec, IConfig const& config ) {
        std::vector<TestCaseHandle> filtered;
        filtered.reserve( testCases.size() );
        for (auto const& testCase : testCases) {
            if ((!testSpec.hasFilters() && !testCase.getTestCaseInfo().isHidden()) ||
                (testSpec.hasFilters() && matchTest(testCase, testSpec, config))) {
                filtered.push_back(testCase);
            }
        }
        return createShard(filtered, config.shardCount(), config.shardIndex());
    }
    std::vector<TestCaseHandle> const& getAllTestCasesSorted( IConfig const& config ) {
        return getRegistryHub().getTestCaseRegistry().getAllTestsSorted( config );
    }

    void TestRegistry::registerTest(Detail::unique_ptr<TestCaseInfo> testInfo, Detail::unique_ptr<ITestInvoker> testInvoker) {
        auto handlesForGroup = m_groups.find(testInfo.get()->group);
        if (handlesForGroup == m_groups.end()) {
            std::vector<TestCaseHandle> newGroupHandles;
            newGroupHandles.emplace_back(testInfo.get(), testInvoker.get());
            m_groups.emplace(testInfo.get()->group, newGroupHandles);
        } else {
            handlesForGroup->second.emplace_back(testInfo.get(), testInvoker.get());
        }

        m_viewed_test_infos.push_back(testInfo.get());
        m_owned_test_infos.push_back(CATCH_MOVE(testInfo));
        m_invokers.push_back(CATCH_MOVE(testInvoker));
    }

    std::vector<TestCaseInfo*> const& TestRegistry::getAllInfos() const {
        return m_viewed_test_infos;
    }

    std::map<StringRef, TestCaseHandleVector> const& TestRegistry::getAllGroups() const {
        return m_groups;
    }

    void TestRegistry::updateAllTestHandles() const {
        m_handles.clear();
        for ( auto const& group : m_groups ) {
            m_handles.insert( m_handles.end(), group.second.begin(), group.second.end() );
        }
    }

    std::vector<TestCaseHandle> const& TestRegistry::getAllTests() const {
        updateAllTestHandles();
        return m_handles;
    }

    std::vector<TestCaseHandle> const& TestRegistry::getAllTestsSorted( IConfig const& config ) const {
        updateAllTestHandles();
        if( m_sortedFunctions.empty() )
            enforceNoDuplicateTestCases( m_handles );

        if(  m_currentSortOrder != config.runOrder() || m_sortedFunctions.empty() ) {
            m_sortedFunctions = sortTests( config, m_handles );
            m_currentSortOrder = config.runOrder();
        }
        return m_sortedFunctions;
    }

    ///////////////////////////////////////////////////////////////////////////
    void TestInvokerAsFunction::invoke() const {
        m_testAsFunction();
    }

} // end namespace Catch

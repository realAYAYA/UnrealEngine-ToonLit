// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestHarness.h"

#if PAS_ENABLE_ISO_TEST

#include "iso_test_heap.h"
#include "iso_test_heap_config.h"
#include <map>
#include "pas_get_page_base.h"
#include "pas_scavenger.h"
#include "pas_segregated_page_inlines.h"
#include <set>
#include <vector>

using namespace std;

namespace {

void testSmallSegregatedExclusiveTryFindAllocatedObjectStart(size_t size, const vector<int>& toFree)
{
    pas_scavenger_suspend();
    
    unsigned numberOfObjects = pas_segregated_page_number_of_objects(
        size, ISO_TEST_HEAP_CONFIG.small_segregated_config, pas_segregated_page_exclusive_role);

    set<void*> objectSet;
    vector<void*> objectArray;
    pas_segregated_page* page = nullptr;
    for (unsigned index = 0; index < numberOfObjects; ++index) {
        void* object = iso_test_allocate_common_primitive(size);
        pas_page_base* pageBase = pas_get_page_base(object, ISO_TEST_HEAP_CONFIG);
        CHECK(pageBase);
        if (page)
            CHECK_EQUAL(pageBase, &page->base);
        else {
            CHECK_EQUAL(pas_page_base_get_kind(pageBase), pas_small_exclusive_segregated_page_kind);
            page = pas_page_base_get_segregated(pageBase);
            CHECK_EQUAL(page->object_size, size);
        }
        objectSet.insert(object);
        objectArray.push_back(object);
    }
    for (int indexish : toFree) {
        unsigned index;
        if (indexish >= 0)
            index = static_cast<unsigned>(indexish);
        else
            index = static_cast<unsigned>(objectArray.size() + indexish);
        CHECK_LESS(index, objectArray.size());
        CHECK(objectSet.count(objectArray[index]));
        iso_test_deallocate(objectArray[index]);
        objectSet.erase(objectArray[index]);
    }

    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_test_heap_config), pas_lock_is_not_held);

    char* boundary = static_cast<char*>(pas_segregated_page_boundary(page, ISO_TEST_HEAP_CONFIG.small_segregated_config));

    for (size_t offset = 0; offset < ISO_TEST_HEAP_CONFIG.small_segregated_config.base.page_size; ++offset) {
        char* object = boundary + offset;
        
        void* expectedObject = nullptr;
        auto iter = objectSet.upper_bound(object);
        if (iter != objectSet.begin()) {
            --iter;
            CHECK_LESS_EQUAL(*iter, static_cast<void*>(object));
            if (object - static_cast<char*>(*iter) < size)
                expectedObject = *iter;
        }

        void* foundObject = reinterpret_cast<void*>(
            pas_segregated_page_try_find_allocated_object_start_with_page(
                page, reinterpret_cast<uintptr_t>(object), ISO_TEST_HEAP_CONFIG.small_segregated_config,
                pas_segregated_page_exclusive_role));

        CHECK_EQUAL(foundObject, expectedObject);
    }
}

} // anonymous namespace

#endif // PAS_ENABLE_ISO_TEST

void addTryFindAllocatedObjectStartTests()
{
#if PAS_ENABLE_ISO_TEST
    DisablePageBalancing disablePageBalancing;
    ForceExclusives forceExclusives;
    DisableBitfit disableBitfit;
    
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(16, { }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(16, { 0 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(16, { 0, -1 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(16, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(16, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 30, 99, 777, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(16, { 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 13, 14, 15, 30, 99, 777, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(16, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 30, 99, 777, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(32, { }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(32, { 0 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(32, { 0, -1 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(32, { 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(32, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(32, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 30, 99, 333, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(32, { 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 13, 14, 15, 30, 99, 333, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(32, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 30, 99, 333, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(896, { }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(896, { 0, 9, -1 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(896, { 0, 8, -1 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(944, { }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(944, { 2 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(944, { 0, 8, -1 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(1008, { }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(1008, { 8 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(1008, { 0, 8, -1 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(1072, { }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(1072, { 8 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(1072, { 0, 4, -1 }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(2688, { }));
    ADD_TEST(testSmallSegregatedExclusiveTryFindAllocatedObjectStart(2688, { 0, 1, -1 }));
#endif // PAS_ENABLE_ISO_TEST
}


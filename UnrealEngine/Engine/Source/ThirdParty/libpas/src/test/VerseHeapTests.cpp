// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestHarness.h"

#if PAS_ENABLE_VERSE && PAS_ENABLE_BMALLOC

#include "bmalloc_heap.h"
#include <map>
#include <mutex>
#include "pas_scavenger.h"
#include <set>
#include <thread>
#include "pas_committed_pages_vector.h"
#include "ue_include/verse_heap_config_ue.h"
#include "ue_include/verse_heap_mark_bits_page_commit_controller_ue.h"
#include "ue_include/verse_heap_ue.h"
#include "verse_heap_inlines.h"
#include "verse_heap_object_set_inlines.h"
#include <vector>

using namespace std;

namespace {

pas_heap* defaultHeap;

void initializeDefaultHeap()
{
    defaultHeap = verse_heap_create(VERSE_HEAP_MIN_ALIGN, 0, 0);
}

void initializeOnlyDefaultHeap()
{
    initializeDefaultHeap();
    verse_heap_did_become_ready_for_allocation();
}

void stopAllocatorsForNode(pas_thread_local_cache_node* node)
{
	verse_heap_thread_local_cache_node_stop_local_allocators(node, pas_thread_local_cache_node_version(node));
}

void handshakeOnOneThread()
{
	stopAllocatorsForNode(verse_heap_get_thread_local_cache_node());
}

set<void*> foundObjects;

void objectCallback(void* object, void* arg)
{
    CHECK(!arg);
    CHECK(!foundObjects.count(object));
    foundObjects.insert(object);
}

void iterateAllObjectsOnOneThread(verse_heap_iterate_filter filter)
{
    foundObjects.clear();
    verse_heap_object_set_start_iterate_before_handshake(&verse_heap_all_objects);
    handshakeOnOneThread();
    size_t iterateSize = verse_heap_object_set_start_iterate_after_handshake(&verse_heap_all_objects);
    verse_heap_object_set_iterate_range(
        &verse_heap_all_objects, 0, iterateSize, filter, objectCallback, nullptr);
    verse_heap_object_set_end_iterate(&verse_heap_all_objects);
}

void sweepOnOneThread()
{
    verse_heap_start_sweep_before_handshake();
    handshakeOnOneThread();
    size_t sweepSize = verse_heap_start_sweep_after_handshake();
    verse_heap_sweep_range(0, sweepSize);
    verse_heap_end_sweep();
}

void doWildAddressChecks()
{
    int x;
    CHECK_EQUAL(verse_heap_find_allocated_object_start(0), 0);
    CHECK_EQUAL(verse_heap_find_allocated_object_start(1), 0);
    CHECK_EQUAL(verse_heap_find_allocated_object_start(42), 0);
    CHECK_EQUAL(verse_heap_find_allocated_object_start(666), 0);
    CHECK_EQUAL(verse_heap_find_allocated_object_start(10000), 0);
    CHECK_EQUAL(verse_heap_find_allocated_object_start(10000000000000000llu), 0);
    CHECK_EQUAL(verse_heap_find_allocated_object_start(0xF000000000000000llu), 0);
    CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(&foundObjects)), 0);
    CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(&x)), 0);
    CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(new int)), 0);
    CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(malloc(42))), 0);
    CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(bmalloc_try_allocate(666))), 0);
}

void testEmptyHeap()
{
    initializeOnlyDefaultHeap();
    doWildAddressChecks();
    CHECK_EQUAL(verse_heap_find_allocated_object_start(1000000000), 0);

	verse_heap_mark_bits_page_commit_controller_lock();
    iterateAllObjectsOnOneThread(verse_heap_iterate_unmarked);
    CHECK_EQUAL(foundObjects.size(), 0);
    iterateAllObjectsOnOneThread(verse_heap_iterate_marked);
    CHECK_EQUAL(foundObjects.size(), 0);
}

void testAllocations(size_t count,
                     size_t size,
                     pas_object_kind expectedObjectKind,
                     verse_heap_black_allocation_mode allocationMode)
{
    CHECK(count);

    pas_scavenger_suspend();
	verse_heap_mark_bits_page_commit_controller_lock();
    
    initializeOnlyDefaultHeap();

    if (allocationMode == verse_heap_allocate_black)
        verse_heap_start_allocating_black_before_handshake();

    vector<void*> ptrs;
    set<void*> ptrSet;
    size_t actualSize = 0;

    for (size_t index = 0; index < count; ++index) {
        void* ptr = verse_heap_allocate(defaultHeap, size);

        CHECK_EQUAL(verse_heap_is_marked(ptr),
                    allocationMode == verse_heap_allocate_black);
        
        size_t allocationSize = verse_heap_get_allocation_size(reinterpret_cast<uintptr_t>(ptr));
        if (!index) {
            CHECK_GREATER_EQUAL(allocationSize, size);
            actualSize = allocationSize;
        } else
            CHECK_EQUAL(allocationSize, actualSize);

        ptrs.push_back(ptr);

        CHECK(!ptrSet.count(ptr));
        ptrSet.insert(ptr);
    }

    CHECK(actualSize);

    auto basicChecks = [&] () {
        for (void* ptr : ptrs) {
            CHECK_EQUAL(verse_heap_is_marked(ptr),
                        allocationMode == verse_heap_allocate_black);
            CHECK_EQUAL(verse_heap_get_allocation_size(reinterpret_cast<uintptr_t>(ptr)), actualSize);
            CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(ptr)),
                        reinterpret_cast<uintptr_t>(ptr));
            CHECK_EQUAL(verse_heap_get_heap_inline(reinterpret_cast<uintptr_t>(ptr)), defaultHeap);
            
            CHECK_EQUAL(verse_heap_get_object_kind(reinterpret_cast<uintptr_t>(ptr)), expectedObjectKind);

            if (count == 1) {
                // Check that pas can find the object even if we give it a pointer anywhere into its allocation.
                for (uintptr_t innerPtr = reinterpret_cast<uintptr_t>(ptr) + actualSize;
                     innerPtr-- > reinterpret_cast<uintptr_t>(ptr);)
                    CHECK_EQUAL(verse_heap_find_allocated_object_start(innerPtr), reinterpret_cast<uintptr_t>(ptr));
            } else {
                // Check that pas can find the object even if we give it a pointer in a random place in its
                // allocation.
                CHECK_EQUAL(verse_heap_find_allocated_object_start(
                                reinterpret_cast<uintptr_t>(ptr) + deterministicRandomNumber(actualSize)),
                            reinterpret_cast<uintptr_t>(ptr));
            }
        }
    };

    basicChecks();
    
    iterateAllObjectsOnOneThread(
        allocationMode == verse_heap_allocate_black ? verse_heap_iterate_marked : verse_heap_iterate_unmarked);
    CHECK_EQUAL(foundObjects.size(), ptrs.size());
    for (void* ptr : ptrs)
        CHECK(foundObjects.count(ptr));
    iterateAllObjectsOnOneThread(
        allocationMode == verse_heap_allocate_black ? verse_heap_iterate_unmarked : verse_heap_iterate_marked);
    CHECK_EQUAL(foundObjects.size(), 0);

    // Check that we can still find the object *after* we stop allocators.
    basicChecks();

    // Now that allocators are stopped, we can also check that objects we didn't allocate are not findable.
    if (count == 1) {
        CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(ptrs[0]) - 1), 0);
        CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(ptrs[0]) + actualSize), 0);
        CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(ptrs[0]) + 0xF000000000000000llu), 0);
        CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(ptrs[0]) + 0x1000000000000llu), 0);
    }

    // Next up: check that if we sweep with all of these objects marked, then they survive.
    for (void* ptr : ptrs) {
        if (allocationMode == verse_heap_allocate_black)
            CHECK(verse_heap_is_marked(ptr));
        else {
            CHECK(!verse_heap_is_marked(ptr));
            verse_heap_set_is_marked(ptr, true);
        }
    }
    if (allocationMode == verse_heap_do_not_allocate_black)
        verse_heap_start_allocating_black_before_handshake();
    sweepOnOneThread();

    for (void* ptr : ptrs) {
        CHECK(!verse_heap_is_marked(ptr));
        CHECK_EQUAL(verse_heap_get_allocation_size(reinterpret_cast<uintptr_t>(ptr)), actualSize);
        CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(ptr)),
                    reinterpret_cast<uintptr_t>(ptr));
        CHECK_EQUAL(verse_heap_get_heap_inline(reinterpret_cast<uintptr_t>(ptr)), defaultHeap);
    }

    // And then sweep them dead.
    verse_heap_start_allocating_black_before_handshake();
    sweepOnOneThread();

    for (void* ptr : ptrs) {
        CHECK(!verse_heap_is_marked(ptr));
        CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(ptr)), 0);
    }

    while (ptrSet.size()) {
        void* ptr = verse_heap_allocate(defaultHeap, size);
        CHECK(ptrSet.count(ptr));
        ptrSet.erase(ptr);
    }

    doWildAddressChecks();
}

void testAllocateDuringIteration(verse_heap_black_allocation_mode firstAllocationMode,
                                 verse_heap_iterate_filter filter)
{
    initializeOnlyDefaultHeap();

	verse_heap_mark_bits_page_commit_controller_lock();

    if (firstAllocationMode == verse_heap_allocate_black)
        verse_heap_start_allocating_black_before_handshake();

    void* ptr1 = verse_heap_allocate(defaultHeap, 100);

    if (firstAllocationMode == verse_heap_do_not_allocate_black) {
        verse_heap_start_allocating_black_before_handshake();
        handshakeOnOneThread();
    }

    verse_heap_object_set_start_iterate_before_handshake(&verse_heap_all_objects);
    handshakeOnOneThread();
    size_t iterateSize = verse_heap_object_set_start_iterate_after_handshake(&verse_heap_all_objects);

    CHECK_EQUAL(foundObjects.size(), 0);

    void* ptr2 = verse_heap_allocate(defaultHeap, 100);

    CHECK_EQUAL(verse_heap_get_segregated_page(reinterpret_cast<uintptr_t>(ptr1)),
                verse_heap_get_segregated_page(reinterpret_cast<uintptr_t>(ptr2)));

	CHECK_EQUAL(foundObjects.size(), 0);

    verse_heap_object_set_iterate_range(
        &verse_heap_all_objects, 0, iterateSize, filter, objectCallback, nullptr);
    verse_heap_object_set_end_iterate(&verse_heap_all_objects);

    // And then actually iterating the page during iterate_range should have done nothing, which causes ptr2 to be
    // ignord. That's right! If we iterate concurrently to the mutator then we iterate the heap as it was at the
    // moment we began, ignoring any newer objects.
    if ((firstAllocationMode == verse_heap_allocate_black) == (filter == verse_heap_iterate_marked)) {
        CHECK_EQUAL(foundObjects.size(), 1);
        CHECK(foundObjects.count(ptr1));
    } else
        CHECK_EQUAL(foundObjects.size(), 0);
}

enum Opcode {
    Invalid,
    Allocate,
    Mark,
    Check,
    StartIterate,
    CheckNumObjectsToSeeDuringIteration,
    IterateIncrement,
    FinishIterate,
    EndIterate,
    StartBlackAllocation,
    StartSweep,
    SweepIncrement,
    FinishSweep,
    EndSweep,
    CheckEmpty,
    CheckNumObjects,
    StopAllocators
};

ostream& operator<<(ostream& output, Opcode opcode)
{
    switch (opcode) {
    case Invalid:
        return output << "Invalid";
    case Allocate:
        return output << "Allocate";
    case Mark:
        return output << "Mark";
    case Check:
        return output << "Check";
    case StartIterate:
        return output << "StartIterate";
    case CheckNumObjectsToSeeDuringIteration:
        return output << "CheckNumObjectsToSeeDuringIteration";
    case IterateIncrement:
        return output << "IterateIncrement";
    case FinishIterate:
        return output << "FinishIterate";
    case EndIterate:
        return output << "EndIterate";
    case StartBlackAllocation:
        return output << "StartBlackAllocation";
    case StartSweep:
        return output << "StartSweep";
    case SweepIncrement:
        return output << "SweepIncrement";
    case FinishSweep:
        return output << "FinishSweep";
    case EndSweep:
        return output << "EndSweep";
    case CheckEmpty:
        return output << "CheckEmpty";
    case CheckNumObjects:
        return output << "CheckNumObjects";
    case StopAllocators:
        return output << "StopAllocators";
    }
    CHECK(!"Should not be reached");
    return output;
}

enum Heap {
    Default,
    Cage,
    Things
};

constexpr unsigned numHeaps = 3;

Heap randomHeap()
{
    return static_cast<Heap>(deterministicRandomNumber(numHeaps));
}

ostream& operator<<(ostream& output, Heap heap)
{
    switch (heap) {
    case Default:
        return output << "Default";
    case Cage:
        return output << "Cage";
    case Things:
        return output << "Things";
    }
    CHECK(!"Should not be reached");
    return output;
}

constexpr size_t cageSize = 1073741824;

enum ObjectSet {
    AllObjects,
    CageAndThings,
    DefaultAndCage,
    CageSet,
    ThingsSet
};

ostream& operator<<(ostream& output, ObjectSet objectSet)
{
    switch (objectSet) {
    case AllObjects:
        return output << "AllObjects";
    case CageAndThings:
        return output << "CageAndThings";
    case DefaultAndCage:
        return output << "DefaultAndCage";
    case CageSet:
        return output << "CageSet";
    case ThingsSet:
        return output << "ThingsSet";
    }
    CHECK(!"Should not be reached");
    return output;
}

bool heapBelongsToSet(Heap heap, ObjectSet objectSet)
{
    switch (objectSet) {
    case AllObjects:
        return true;
    case CageAndThings:
        return heap == Cage || heap == Things;
    case DefaultAndCage:
        return heap == Default || heap == Cage;
    case CageSet:
        return heap == Cage;
    case ThingsSet:
        return heap == Things;
    }
    CHECK(!"Should not be reached");
    return false;
}

struct Op {
    Op() { }
    
    Op(Opcode opcode)
        : opcode(opcode)
    {
        CHECK(opcode == Invalid ||
              opcode == StartBlackAllocation ||
              opcode == StartSweep ||
              opcode == SweepIncrement ||
              opcode == FinishSweep ||
              opcode == EndSweep ||
              opcode == CheckEmpty ||
              opcode == StopAllocators);
    }

    Op(Opcode opcode, uintptr_t index)
        : opcode(opcode)
        , index(index)
    {
        CHECK(opcode == Mark ||
              opcode == Check ||
              opcode == CheckNumObjects ||
              opcode == CheckNumObjectsToSeeDuringIteration);
    }

    Op(Opcode opcode, uintptr_t index, Heap heap, size_t size)
        : opcode(opcode)
        , index(index)
        , heap(heap)
        , size(size)
    {
        CHECK(opcode == Allocate);
    }

    Op(Opcode opcode, ObjectSet objectSet, verse_heap_iterate_filter filter)
        : opcode(opcode)
        , objectSet(objectSet)
        , filter(filter)
    {
        CHECK(opcode == StartIterate ||
              opcode == IterateIncrement ||
              opcode == FinishIterate);
    }

    Op(Opcode opcode, ObjectSet objectSet)
        : opcode(opcode)
        , objectSet(objectSet)
    {
        CHECK(opcode == EndIterate);
    }

    Opcode opcode { Invalid };
    uintptr_t index { 0 };
    Heap heap { Default };
    size_t size { 0 };
    ObjectSet objectSet { AllObjects };
    verse_heap_iterate_filter filter { verse_heap_iterate_unmarked };
};

ostream& operator<<(ostream& output, Op op)
{
    output << op.opcode;
    switch (op.opcode) {
    case Mark:
    case Check:
    case CheckNumObjects:
    case CheckNumObjectsToSeeDuringIteration:
        output << "(" << op.index << ")";
        break;
    case Allocate:
        output << "(" << op.index << ", " << op.heap << ", " << op.size << ")";
        break;
    case StartIterate:
    case IterateIncrement:
    case FinishIterate:
        output << "(" << op.objectSet << ", " << verse_heap_iterate_filter_get_string(op.filter) << ")";
        break;
    case EndIterate:
        output << "(" << op.objectSet << ")";
        break;
    default:
        break;
    }
    return output;
}

pas_heap* cage;
pas_heap* things;

constexpr size_t thingsMinalign = 128;

pas_heap* getHeap(Heap heap)
{
    switch (heap) {
    case Default:
        return defaultHeap;
    case Cage:
        return cage;
    case Things:
        return things;
    }
    CHECK(!"Should not be reached");
    return nullptr;
}

verse_heap_object_set* cageAndThings;
verse_heap_object_set* defaultAndCage;
verse_heap_object_set* cageSet;
verse_heap_object_set* thingsSet;

verse_heap_object_set* getSet(ObjectSet set)
{
    switch (set) {
    case AllObjects:
        return &verse_heap_all_objects;
    case CageAndThings:
        return cageAndThings;
    case DefaultAndCage:
        return defaultAndCage;
    case CageSet:
        return cageSet;
    case ThingsSet:
        return thingsSet;
    }
    CHECK(!"Should not be reached");
    return nullptr;
}

struct Object {
    Object() { }
    
    Object(void* ptr,
           size_t size,
           Heap heap)
        : ptr(ptr)
        , size(size)
        , heap(heap)
    {
    }

    void check() const
    {
        CHECK_EQUAL(verse_heap_get_allocation_size(reinterpret_cast<uintptr_t>(ptr)), size);
        bool heapIsMarked = verse_heap_is_marked(ptr);
        if (heapIsMarked != isMarked) {
            cout << "Object has unexpected mark bit value, ptr = " << ptr << ", size = " << size
                 << ", kind = " << verse_heap_get_object_kind(reinterpret_cast<uintptr_t>(ptr))
                 << ", page = " << verse_heap_get_segregated_page(reinterpret_cast<uintptr_t>(ptr)) << "\n";
            CHECK_EQUAL(heapIsMarked, isMarked);
        }
        CHECK_EQUAL(verse_heap_get_heap_inline(reinterpret_cast<uintptr_t>(ptr)), getHeap(heap));

        switch (heap) {
        case Default:
            CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(ptr), VERSE_HEAP_MIN_ALIGN));
            break;
        case Cage:
            CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(ptr), VERSE_HEAP_MIN_ALIGN));
            CHECK_GREATER_EQUAL(ptr, verse_heap_get_base(cage));
            CHECK_LESS(ptr, static_cast<void*>(static_cast<char*>(verse_heap_get_base(cage)) + cageSize));
            break;
        case Things:
            CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(ptr), thingsMinalign));
            break;
        }
    }

    bool operator==(const Object& other) const { return ptr == other.ptr; }
    bool operator<(const Object& other) const { return ptr < other.ptr; }
    
    void* ptr { 0 };
    size_t size { 0 };
    Heap heap { Default };

    // This should accurately predict the state of the mark bit, modulo races if the test is multi-threaded,
    // and except during sweeping. During sweeping, the collector will clear these bits in some order.
    bool isMarked { false };

    // If an object is allocated during sweep, then we know that it won't be marked after sweep is over
    // but it may or may not have been marked at time of allocation - so the isMarked bit doesn't predict
    // the post-sweep state.
    bool allocatedDuringSweep { false };
};

enum BlackAllocationMode {
    AllocatingWhite,
    AllocatingBlack,
    AllocatingDuringSweep
};

map<uintptr_t, Object> objects;
map<void*, uintptr_t> addressToIndex;
set<void*> objectsToSeeDuringIteration;

void workflowObjectCallback(void* object, void* arg)
{
    CHECK(!arg);
    CHECK(objectsToSeeDuringIteration.count(object));
    objectsToSeeDuringIteration.erase(object);
    CHECK(addressToIndex.count(object));
    CHECK(objects.count(addressToIndex[object]));
    objects[addressToIndex[object]].check();
}

void initializeInterestingHeaps()
{
    initializeDefaultHeap();

    cage = verse_heap_create(VERSE_HEAP_MIN_ALIGN, cageSize, cageSize);
    CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(verse_heap_get_base(cage)), cageSize));
    
    things = verse_heap_create(thingsMinalign, 0, 0);

    cageAndThings = verse_heap_object_set_create();
    defaultAndCage = verse_heap_object_set_create();
    cageSet = verse_heap_object_set_create();
    thingsSet = verse_heap_object_set_create();
    
    verse_heap_add_to_set(defaultHeap, defaultAndCage);
    verse_heap_add_to_set(cage, cageAndThings);
    verse_heap_add_to_set(cage, defaultAndCage);
    verse_heap_add_to_set(cage, cageSet);
    verse_heap_add_to_set(things, cageAndThings);
    verse_heap_add_to_set(things, thingsSet);

    verse_heap_did_become_ready_for_allocation();
}

void testWorkflow(const std::vector<Op>& ops)
{
    initializeInterestingHeaps();
    
	verse_heap_mark_bits_page_commit_controller_lock();

    BlackAllocationMode blackAllocationMode = AllocatingWhite;
    
    size_t iterateIndex = 0;
    size_t iterateSize = 0;

    size_t sweepSize = 0;
    size_t sweepIndex = 0;

    for (const Op& op : ops) {
        cout << "    " << op << "\n";
        
        auto get = [&] () -> Object& {
            CHECK(objects.count(op.index));
            return objects[op.index];
        };
        
        auto check = [&] () {
            get().check();
        };
        
        switch (op.opcode) {
        case Invalid: {
            CHECK(!"Should not encounter Invalid opcode.");
            break;
        }
        case Allocate: {
            void* ptr = verse_heap_allocate(getHeap(op.heap), op.size);
            size_t actualSize = verse_heap_get_allocation_size(reinterpret_cast<uintptr_t>(ptr));
            CHECK_GREATER_EQUAL(actualSize, op.size);
            Object object(ptr, actualSize, op.heap);
            switch (blackAllocationMode) {
            case AllocatingBlack:
                object.isMarked = true;
                break;
            case AllocatingWhite:
                break;
            case AllocatingDuringSweep:
                object.isMarked = verse_heap_is_marked(ptr);
                object.allocatedDuringSweep = true;
                break;
            }
            CHECK(!objects.count(op.index));
            objects[op.index] = object;
            addressToIndex[ptr] = op.index;
            check();
            break;
        }
        case Mark: {
            check();
            verse_heap_set_is_marked(get().ptr, true);
            get().isMarked = true;
            check();
            break;
        }
        case Check: {
            check();
            break;
        }
        case StartIterate: {
            CHECK(objectsToSeeDuringIteration.empty());
            for (auto& pair : objects) {
                if (pair.second.isMarked != (op.filter == verse_heap_iterate_marked))
                    continue;
                if (!heapBelongsToSet(pair.second.heap, op.objectSet))
                    continue;
                objectsToSeeDuringIteration.insert(pair.second.ptr);
            }
            verse_heap_object_set_start_iterate_before_handshake(getSet(op.objectSet));
            handshakeOnOneThread();
            iterateSize = verse_heap_object_set_start_iterate_after_handshake(getSet(op.objectSet));
            iterateIndex = 0;
            break;
        }
        case CheckNumObjectsToSeeDuringIteration: {
            CHECK_EQUAL(objectsToSeeDuringIteration.size(), op.index);
            break;
        }
        case IterateIncrement: {
            if (iterateIndex < iterateSize) {
                verse_heap_object_set_iterate_range(
                    getSet(op.objectSet), iterateIndex, iterateIndex + 1, op.filter,
                    workflowObjectCallback, nullptr);
                iterateIndex++;
            }
            break;
        }
        case FinishIterate: {
            if (iterateIndex < iterateSize) {
                verse_heap_object_set_iterate_range(
                    getSet(op.objectSet), iterateIndex, iterateSize, op.filter,
                    workflowObjectCallback, nullptr);
                iterateIndex = iterateSize;
            }
            break;
        }
        case EndIterate: {
            CHECK_EQUAL(iterateIndex, iterateSize);
            verse_heap_object_set_end_iterate(getSet(op.objectSet));
            CHECK(objectsToSeeDuringIteration.empty());
            break;
        }
        case StartBlackAllocation: {
            verse_heap_start_allocating_black_before_handshake();
            handshakeOnOneThread();
            blackAllocationMode = AllocatingBlack;
            break;
        }
        case StartSweep: {
            verse_heap_start_sweep_before_handshake();
            handshakeOnOneThread();
            sweepSize = verse_heap_start_sweep_after_handshake();
            sweepIndex = 0;
            blackAllocationMode = AllocatingDuringSweep;
            break;
        }
        case SweepIncrement: {
            if (sweepIndex < sweepSize) {
                verse_heap_sweep_range(sweepIndex, sweepIndex + 1);
                sweepIndex++;
            }
            break;
        }
        case FinishSweep: {
            if (sweepIndex < sweepSize) {
                verse_heap_sweep_range(sweepIndex, sweepSize);
                sweepIndex = sweepSize;
            }
            break;
        }
        case EndSweep: {
            verse_heap_end_sweep();

            // At this point:
            // - Objects that we thought were marked should still be alive but not marked.
            // - Objects that we thought were not marked should be dead or allocated over.
            vector<uintptr_t> indices;
            for (auto& pair : objects)
                indices.push_back(pair.first);
            size_t survivorSize = 0;
			size_t bytesSwept = 0;
            for (uintptr_t index : indices) {
                Object& object = objects[index];
                
                if (object.isMarked || object.allocatedDuringSweep) {
                    CHECK(!verse_heap_is_marked(object.ptr));
                    CHECK_EQUAL(
                        verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(object.ptr)),
                        reinterpret_cast<uintptr_t>(object.ptr));
                    object.isMarked = false;
                    object.allocatedDuringSweep = false;
                    object.check();
                    survivorSize += object.size;
                    continue;
                }

                if (verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(object.ptr))
                    == reinterpret_cast<uintptr_t>(object.ptr)) {
                    // It got allocated over! Make sure that we're talking about a different object.
                    CHECK_NOT_EQUAL(addressToIndex[object.ptr], index);
                } else
                    addressToIndex.erase(object.ptr);

                // Clear the object out of our tables.
                objects.erase(index);

				bytesSwept += object.size;
            }

            handshakeOnOneThread();
            CHECK_EQUAL(verse_heap_live_bytes, survivorSize);
			CHECK_EQUAL(verse_heap_swept_bytes, bytesSwept);

            blackAllocationMode = AllocatingWhite;
            break;
        }
        case CheckEmpty: {
            CHECK(objects.empty());
            CHECK(addressToIndex.empty());
            CHECK(objectsToSeeDuringIteration.empty());
            break;
        }
        case CheckNumObjects: {
            CHECK_EQUAL(objects.size(), op.index);
            CHECK_EQUAL(addressToIndex.size(), op.index);
            break;
        }
        case StopAllocators: {
            handshakeOnOneThread();
            break;
        } }
    }
}

constexpr size_t numEarlyMarkedObjects = 100;
constexpr size_t numObjectsToMark = 1000;
constexpr size_t maxNumObjects = 1000000;
constexpr size_t maxNumMutatorThreads = 100;
constexpr size_t smallAllocationsPerIteration = 10;
constexpr size_t maxSmallObjectSize = 500;
constexpr size_t mediumAllocationsPerIteration = 1;
constexpr size_t maxMediumObjectSize = 50000;
constexpr size_t iterationsPerLargeAllocation = 100;
constexpr size_t maxLargeObjectSize = 1000000;
constexpr size_t objectArraySize =
     maxNumObjects + maxNumMutatorThreads * (smallAllocationsPerIteration +
                                             mediumAllocationsPerIteration + 1);
constexpr size_t requiredCollectorIterations = 1000;
constexpr size_t requiredMutatorIterations = 10000;
constexpr size_t maxHeapSize = 100000000;
constexpr size_t attemptsPerIteration = 100;

enum State {
    NotCollecting,
    StartingToAllocateBlack,
    Marking,
    Sweeping
};

ostream& operator<<(ostream& output, State state)
{
    switch (state) {
    case NotCollecting:
        return output << "NotCollecting";
    case StartingToAllocateBlack:
        return output << "StartingToAllocateBlack";
    case Marking:
        return output << "Marking";
    case Sweeping:
        return output << "Sweeping";
    }
    CHECK(!"Should not be reached");
    return output;
}

struct Thread {
    explicit Thread(size_t index)
        : index(index)
    {
    }

    size_t index;
    pas_thread_local_cache_node* node { nullptr };
    mutex lock;
    size_t numIterations { 0 };
};

State state;
Object objectArray[objectArraySize];
size_t numObjects; // Appending and reading this field requires holding `lock` or being in a hard handshake.
vector<Thread*> threads;
size_t heapSize;
mutex lock;
mutex iterationLock;
set<void*> objectsNotToSeeDuringIteration;
verse_heap_iterate_filter iterateFilter;
size_t numCollectorIterations;
bool iterateDeterministically;

bool shouldContinue()
{
    if (numCollectorIterations < requiredCollectorIterations)
        return true;
    for (Thread* thread : threads) {
        if (thread->numIterations < requiredMutatorIterations)
            return true;
    }
    return false;
}

void chaosMutatorThreadMain(Thread* thread)
{
    static constexpr bool verbose = false;
    
    {
        lock_guard<mutex> locker(thread->lock);
        thread->node = verse_heap_get_thread_local_cache_node();
    }

    bool shouldReportStuck = false;

    for (; shouldContinue(); thread->numIterations++) {
        whiteDelay();
        
        lock_guard<mutex> locker(thread->lock);
        if (verbose || (thread->numIterations % (requiredMutatorIterations / 10)) == 0) {
            cout << "    Mutator #" << thread->index << ": Starting iteration " << thread->numIterations << "\n";
            shouldReportStuck = true;
        }
        if (numObjects >= maxNumObjects) {
            if (shouldReportStuck) {
                cout << "    Mutator #" << thread->index << ": stuck!\n";
                shouldReportStuck = false;
            }
            whiteDelay();
            continue;
        }
        
        vector<Object> myObjects;
        size_t myHeapSize = 0;

        auto allocate = [&] (size_t size) {
            Heap heap = randomHeap();
            void* ptr = verse_heap_try_allocate(getHeap(heap), size);
            if (heap == Cage) {
                if (!ptr)
                    return;
            } else
                CHECK(ptr);

            size_t actualSize = verse_heap_get_allocation_size(reinterpret_cast<uintptr_t>(ptr));
            CHECK_GREATER_EQUAL(actualSize, size);

            Object object(ptr, actualSize, heap);

            switch (state) {
            case NotCollecting:
                object.isMarked = false;
                break;
            case Marking:
                object.isMarked = true;
                break;
            case StartingToAllocateBlack:
            case Sweeping:
                object.isMarked = verse_heap_is_marked(ptr);
                break;
            }

            if (state == Sweeping)
                object.allocatedDuringSweep = true;
            else
                object.check();
            
            myHeapSize += actualSize;
            myObjects.push_back(object);
        };

        for (size_t count = smallAllocationsPerIteration; count--;)
            allocate(deterministicRandomNumber(maxSmallObjectSize));
        if (heapSize + myHeapSize < maxHeapSize) {
            for (size_t count = mediumAllocationsPerIteration; count--;)
                allocate(deterministicRandomNumber(maxMediumObjectSize));
            if (!deterministicRandomNumber(iterationsPerLargeAllocation))
                allocate(deterministicRandomNumber(maxLargeObjectSize));
        }

        if (verbose) {
            cout << "    Mutator #" << thread->index << ": Allocated " << myHeapSize << " bytes during iteration "
                 << thread->numIterations << "\n";
        }

        lock_guard<mutex> lockLocker(lock);
        CHECK_LESS_EQUAL(numObjects + myObjects.size(), objectArraySize);
        for (Object object : myObjects)
            objectArray[numObjects++] = object;
        heapSize += myHeapSize;
    }

    lock_guard<mutex> locker(thread->lock);
    // Need to do this here; otherwise, the soft handshake won't stop the allocators and they will be active at the
    // time that iteration happens.
	stopAllocatorsForNode(thread->node);
    thread->node = nullptr;
}

template<typename Func>
void hardHandshake(const Func& func)
{
    static constexpr bool verbose = false;
    if (verbose)
        cout << "    Starting hard handshake!\n";
    for (size_t index = 0; index < threads.size(); ++index) {
        Thread* thread = threads[index];
        thread->lock.lock();
        if (verbose)
            cout << "    Hard handshake: " << index + 1 << "/" << threads.size() << "\n";
    }
    func();
    if (verbose)
        cout << "    Finishing hard handshake!\n";
    for (Thread* thread : threads)
        thread->lock.unlock();
}

template<typename Func>
void softHandshake(const Func& func)
{
    static constexpr bool verbose = false;
    if (verbose)
        cout << "    Starting soft handshake!\n";
    for (size_t index = 0; index < threads.size(); ++ index) {
        Thread* thread = threads[index];
        lock_guard<mutex> locker(thread->lock);
        func(thread);
        if (verbose)
            cout << "    Soft handshake: " << index + 1 << "/" << threads.size() << "\n";
    }
}

void stopAllocators(Thread* thread)
{
    if (thread->node)
		stopAllocatorsForNode(thread->node);
}

PAS_UNUSED void stopAllAllocators()
{
    for (Thread* thread : threads)
        stopAllocators(thread);
}

void chaosObjectCallback(void* object, void* arg)
{
    CHECK(!arg);

	// We should never be called with the pas heap_lock held.
	pas_heap_lock_lock();
	pas_heap_lock_unlock();

	// It should be possible to allocate and free with bmalloc.
	bmalloc_deallocate(bmalloc_allocate(100));
	
    lock_guard<mutex> locker(iterationLock);
    switch (iterateFilter) {
    case verse_heap_iterate_marked:
        CHECK(verse_heap_is_marked(object));
        CHECK(!objectsNotToSeeDuringIteration.count(object));
        objectsNotToSeeDuringIteration.insert(object);
        break;
    case verse_heap_iterate_unmarked:
        CHECK(!verse_heap_is_marked(object));
        CHECK(objectsToSeeDuringIteration.count(object));
        break;
    }
    objectsToSeeDuringIteration.erase(object);
}

void chaosIterate(ObjectSet objectSet, verse_heap_iterate_filter filter)
{
    static constexpr bool verbose = false;

    if (!iterateDeterministically && deterministicRandomNumber(attemptsPerIteration))
        return;
    
    if (verbose || !iterateDeterministically)
        cout << "    Iterating " << objectSet << "/" << verse_heap_iterate_filter_get_string(filter) << "\n";
    
    size_t numObjectsForIterate;
    {
        lock_guard<mutex> locker(lock);
        numObjectsForIterate = numObjects;
    }
    CHECK(objectsToSeeDuringIteration.empty());
    CHECK(objectsNotToSeeDuringIteration.empty());
    for (size_t index = numObjectsForIterate; index--;) {
        const Object& object = objectArray[index];
        object.check();
        if (object.isMarked == (filter == verse_heap_iterate_marked) &&
            heapBelongsToSet(object.heap, objectSet))
            objectsToSeeDuringIteration.insert(object.ptr);
        else if (filter == verse_heap_iterate_marked)
            objectsNotToSeeDuringIteration.insert(object.ptr);
    }

    {
        lock_guard<mutex> locker(iterationLock);
        iterateFilter = filter;
    }
    verse_heap_object_set_start_iterate_before_handshake(getSet(objectSet));
    softHandshake(stopAllocators);
    size_t iterateSize = verse_heap_object_set_start_iterate_after_handshake(getSet(objectSet));
    verse_heap_object_set_iterate_range(
        getSet(objectSet), 0, iterateSize, filter, chaosObjectCallback, nullptr);
    if (objectsToSeeDuringIteration.size()) {
        for (void* ptr : objectsToSeeDuringIteration) {
            cout << "Missed object: " << ptr << "/" << verse_heap_get_object_kind(reinterpret_cast<uintptr_t>(ptr))
                 << "\n";
        }
        CHECK_EQUAL(objectsToSeeDuringIteration.size(), 0);
    }
    objectsNotToSeeDuringIteration.clear();
    verse_heap_object_set_end_iterate(getSet(objectSet));
}

void chaosIterateAllSets(verse_heap_iterate_filter filter)
{
    chaosIterate(AllObjects, filter);
    chaosIterate(CageAndThings, filter);
    chaosIterate(DefaultAndCage, filter);
    chaosIterate(CageSet, filter);
    chaosIterate(ThingsSet, filter);
}

void chaosIterateAll()
{
    chaosIterateAllSets(verse_heap_iterate_marked);
    chaosIterateAllSets(verse_heap_iterate_unmarked);
}

void chaosCollectorThreadMain()
{
    static constexpr bool verbose = false;

    for (; shouldContinue(); numCollectorIterations++) {
        CHECK_EQUAL(state, NotCollecting);

        if (verbose || (numCollectorIterations % (requiredCollectorIterations / 10)) == 0)
            cout << "    Collector: starting iteration " << numCollectorIterations << "\n";

        whiteDelay();
        vector<Object*> objectsToMarkEarly;
        hardHandshake([&] () {
            for (size_t index = numObjects; index--;)
                objectArray[index].check();

            set<size_t> seenIndices;
            if (numObjects) {
                for (size_t count = numEarlyMarkedObjects; count--;) {
                    size_t index = deterministicRandomNumber(numObjects);
                    if (seenIndices.count(index))
                        continue;
                    seenIndices.insert(index);
                    CHECK(objectArray[index].ptr);
                    objectsToMarkEarly.push_back(objectArray + index);
                }
            }
        });
        whiteDelay();
		verse_heap_mark_bits_page_commit_controller_lock();
        for (Object* object : objectsToMarkEarly) {
            CHECK(!verse_heap_is_marked(object->ptr));
            CHECK(!object->isMarked);
            verse_heap_set_is_marked(object->ptr, true);
            object->isMarked = true;
        }
        whiteDelay();
        
        CHECK_EQUAL(state, NotCollecting);

        auto setState = [&] (State newState) {
            if (verbose)
                cout << "    Collector: " << state << "->" << newState << "\n";
            state = newState;
        };
        
        hardHandshake([&] () {
            setState(StartingToAllocateBlack);
        });
        verse_heap_start_allocating_black_before_handshake();
        softHandshake(stopAllocators);
        size_t myNumObjects;
        hardHandshake([&] () {
            myNumObjects = numObjects;
            if (verbose)
                cout << "    Collector: myNumObjects = " << myNumObjects << "\n";
            setState(Marking);
        });
        set<void*> myObjectSet;
        for (size_t index = myNumObjects; index--;) {
            const Object& object = objectArray[index];
            CHECK(!myObjectSet.count(object.ptr));
            myObjectSet.insert(object.ptr);
            object.check();
        }

        auto markSomeObjects = [&] () {
            if (!myNumObjects)
                return;
            for (size_t count = numObjectsToMark; count--;) {
                size_t index = deterministicRandomNumber(myNumObjects);
                Object& object = objectArray[index];
                object.check();
                if (object.isMarked)
                    continue;
                verse_heap_set_is_marked(object.ptr, true);
                object.isMarked = true;
            }
        };

        chaosIterateAll();
        markSomeObjects();
        chaosIterateAll();
        markSomeObjects();
        chaosIterateAll();

        hardHandshake([&] () { setState(Sweeping); });
        if (verbose)
            cout << "    numObjects right before sweep = " << numObjects << "\n";
        verse_heap_start_sweep_before_handshake();
        if (verbose)
            cout << "    numObjects right after starting sweep = " << numObjects << "\n";
        softHandshake(stopAllocators);
        size_t sweepSize = verse_heap_start_sweep_after_handshake();
        verse_heap_sweep_range(0, sweepSize);
        verse_heap_end_sweep();
        if (verbose)
            cout << "    numObjects after sweep ended = " << numObjects << "\n";
        hardHandshake([&] () {
            setState(NotCollecting);

            size_t dstIndex = 0;
			size_t bytesSwept = 0;
            for (size_t srcIndex = 0; srcIndex < numObjects; srcIndex++) {
                Object object = objectArray[srcIndex];
                myObjectSet.erase(object.ptr);
                if (object.isMarked || object.allocatedDuringSweep) {
                    if (verse_heap_is_marked(object.ptr)) {
                        cout << "Unexpected marked object after sweep at srcIndex = " << srcIndex << ", ptr = "
                             << object.ptr << ", size = " << object.size << ", kind = "
                             << verse_heap_get_object_kind(reinterpret_cast<uintptr_t>(object.ptr))
                             << ", page = " << verse_heap_get_segregated_page(reinterpret_cast<uintptr_t>(object.ptr))
                             << "\n";
                        CHECK(!verse_heap_is_marked(object.ptr));
                    }
                    CHECK_EQUAL(
                        verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(object.ptr)),
                        reinterpret_cast<uintptr_t>(object.ptr));
                    object.isMarked = false;
                    object.allocatedDuringSweep = false;
                    object.check();
                    objectArray[dstIndex++] = object;
                    continue;
                }

                heapSize -= object.size;
				bytesSwept += object.size;
            }
            numObjects = dstIndex;
            CHECK_EQUAL(myObjectSet.size(), 0);

            stopAllAllocators();
            CHECK_EQUAL(verse_heap_live_bytes, heapSize);
			CHECK_EQUAL(verse_heap_swept_bytes, bytesSwept);
        });
		verse_heap_mark_bits_page_commit_controller_unlock();
    }
}

void testChaos(size_t numMutatorThreads, bool passedIterateDeterministically)
{
    CHECK_LESS_EQUAL(numMutatorThreads, maxNumMutatorThreads);

    iterateDeterministically = passedIterateDeterministically;
    
    initializeInterestingHeaps();

    for (size_t index = 0; index < numMutatorThreads; index++)
        threads.push_back(new Thread(index));
    
    vector<thread> threadHandles;

    threadHandles.push_back(thread(chaosCollectorThreadMain));

    for (Thread* threadObject : threads)
        threadHandles.push_back(thread(chaosMutatorThreadMain, threadObject));

    for (thread& thread : threadHandles)
        thread.join();
}

void testConservativeMarking(size_t size,
                             size_t numTestObjects,
                             size_t numSurvivors,
                             bool expectFreePages,
                             size_t numCrazyPointers,
                             size_t numAddressSpacePointers,
                             size_t numSurvivorPointers,
                             size_t numDeadPointers,
                             size_t numAllocatingThreads,
                             size_t numMarkingThreads,
                             bool scavenge)
{
    static constexpr bool verbose = false;
    
    initializeOnlyDefaultHeap();
	verse_heap_mark_bits_page_commit_controller_lock();
    
    set<void*> fullObjectSet;
    vector<void*> fullObjectArray;
    set<void*> liveObjectSet;
    vector<void*> liveObjectArray;
    vector<void*> deadObjectArray;

    for (size_t count = numTestObjects; count--;) {
        void* ptr = verse_heap_allocate(defaultHeap, size);
        CHECK(!fullObjectSet.count(ptr));
        CHECK(!liveObjectSet.count(ptr));
        fullObjectSet.insert(ptr);
        fullObjectArray.push_back(ptr);
        liveObjectSet.insert(ptr);
        liveObjectArray.push_back(ptr);
    }

    CHECK_EQUAL(liveObjectSet.size(), fullObjectSet.size());
    CHECK_EQUAL(liveObjectSet.size(), liveObjectArray.size());
    CHECK_EQUAL(liveObjectSet.size(), fullObjectArray.size());

    while (liveObjectArray.size() > numSurvivors) {
        unsigned index = deterministicRandomNumber(liveObjectArray.size());
        void* ptr = liveObjectArray[index];
        liveObjectSet.erase(ptr);
        liveObjectArray[index] = liveObjectArray.back();
        liveObjectArray.pop_back();
        deadObjectArray.push_back(ptr);
    }

    CHECK_EQUAL(liveObjectSet.size(), liveObjectArray.size());
    CHECK_LESS_EQUAL(liveObjectSet.size(), numSurvivors);
    CHECK_EQUAL(liveObjectArray.size() + deadObjectArray.size(), fullObjectArray.size());

    verse_heap_start_allocating_black_before_handshake();
    handshakeOnOneThread();
    for (void* ptr : liveObjectArray)
        CHECK(verse_heap_set_is_marked(ptr, true));
    sweepOnOneThread();

    pas_scavenger_run_synchronously_now();

    if (verbose)
        printStatusReport();

    if (expectFreePages) {
        bool foundEmptyPage = false;
        for (void* ptr : deadObjectArray) {
            verse_heap_chunk_map_entry entry = verse_heap_get_chunk_map_entry(reinterpret_cast<uintptr_t>(ptr));
            if (verse_heap_chunk_map_entry_is_empty(entry))
                foundEmptyPage = true;
            if (verse_heap_chunk_map_entry_is_small_segregated(entry)) {
                unsigned bitvector;
                size_t index;

                bitvector = verse_heap_chunk_map_entry_small_segregated_ownership_bitvector(entry);
                index = pas_modulo_power_of_2(reinterpret_cast<uintptr_t>(ptr), VERSE_HEAP_CHUNK_SIZE)
                    / VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE;

                CHECK(index);

                if (!pas_bitvector_get_from_one_word(&bitvector, index))
                    foundEmptyPage = true;
            }
        }
        
        CHECK(foundEmptyPage);
    }

    iterateAllObjectsOnOneThread(verse_heap_iterate_unmarked);
    CHECK_EQUAL(foundObjects, liveObjectSet);
    iterateAllObjectsOnOneThread(verse_heap_iterate_marked);
    CHECK_EQUAL(foundObjects, set<void*>());

    set<void*> pointersToTrySet;

    for (size_t count = numCrazyPointers; count--;) {
        pointersToTrySet.insert(
            reinterpret_cast<void*>(
                (static_cast<uintptr_t>(deterministicRandomNumber()) << 32) +
                static_cast<uintptr_t>(deterministicRandomNumber())));
    }

    for (size_t count = numAddressSpacePointers; count--;) {
        pointersToTrySet.insert(
            reinterpret_cast<void*>(
                (static_cast<uintptr_t>(deterministicRandomNumber(0x10000)) << 32) +
                static_cast<uintptr_t>(deterministicRandomNumber())));
    }

    for (size_t count = numSurvivorPointers; count--;) {
        void* ptr = liveObjectArray[deterministicRandomNumber(liveObjectArray.size())];
        void* innerPtr = static_cast<char*>(ptr) + deterministicRandomNumber(size);
        CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(ptr)),
                    reinterpret_cast<uintptr_t>(ptr));
        CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(innerPtr)),
                    reinterpret_cast<uintptr_t>(ptr));
        pointersToTrySet.insert(innerPtr);
    }

    for (size_t count = numDeadPointers; count--;) {
        void* ptr = deadObjectArray[deterministicRandomNumber(deadObjectArray.size())];
        void* innerPtr = static_cast<char*>(ptr) + deterministicRandomNumber(size);
        CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(ptr)), 0);
        CHECK_EQUAL(verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(innerPtr)), 0);
        pointersToTrySet.insert(innerPtr);
    }

    vector<void*> pointersToTryArray;
    for (void* ptr : pointersToTrySet)
        pointersToTryArray.push_back(ptr);

    verse_heap_start_allocating_black_before_handshake();
    handshakeOnOneThread();

    if (scavenge)
        pas_scavenger_run_synchronously_now();

    cout << "    Starting threads.\n";

    vector<thread> local_threads;

    uintptr_t numObjectsToAllocate = deadObjectArray.size();
    for (size_t count = numAllocatingThreads; count--;) {
		local_threads.push_back(thread([&] () {
            for (;;) {
                size_t oldNumObjectsToAllocate = numObjectsToAllocate;
                if (!oldNumObjectsToAllocate)
                    break;
                size_t newNumObjectsToAllocate = oldNumObjectsToAllocate - 1;
                if (!pas_compare_and_swap_uintptr_weak(
                        &numObjectsToAllocate, oldNumObjectsToAllocate, newNumObjectsToAllocate))
                    continue;
                verse_heap_allocate(defaultHeap, size);
            }
            cout << "    Allocating thread done.\n";
        }));
    }

    size_t markingIndex = 0;
    for (size_t count = numMarkingThreads; count--;) {
		local_threads.push_back(thread([&] () {
            for (;;) {
                size_t oldMarkingIndex = markingIndex;
                if (oldMarkingIndex >= pointersToTryArray.size()) {
                    CHECK_EQUAL(oldMarkingIndex, pointersToTryArray.size());
                    break;
                }
                size_t newMarkingIndex = oldMarkingIndex + 1;
                if (!pas_compare_and_swap_uintptr_weak(&markingIndex, oldMarkingIndex, newMarkingIndex))
                    continue;
                void* ptr = pointersToTryArray[oldMarkingIndex];
                uintptr_t ptrBase = verse_heap_find_allocated_object_start(reinterpret_cast<uintptr_t>(ptr));
                bool didMark = ptrBase && verse_heap_set_is_marked(reinterpret_cast<void*>(ptrBase), true);
                void* actualBase = nullptr;
                auto iter = liveObjectSet.upper_bound(ptr);

                auto dump = [&] () {
                    cout << "For object ptr = " << ptr << "\n";
                    cout << "actualBase = " << actualBase << "\n";
                    cout << "ptrBase = " << reinterpret_cast<void*>(ptrBase) << "\n";
                    cout << "didMark = " << didMark << "\n";
                };
                
                if (iter != liveObjectSet.begin()) {
                    --iter;
                    if (static_cast<char*>(ptr) - static_cast<char*>(*iter) < size) {
                        actualBase = *iter;
                        if (reinterpret_cast<void*>(ptrBase) != actualBase) {
                            dump();
                            CHECK_EQUAL(reinterpret_cast<void*>(ptrBase), actualBase);
                        }
                    }
                }
                if (didMark && !actualBase) {
                    dump();
                    CHECK(!"Marked an object that wasn't in the original survivor set.");
                }
            }
            cout << "    Marking thread done.\n";
        }));
    }

    for (thread& thread : local_threads)
        thread.join();

    for (void* ptr : pointersToTryArray) {
        auto iter = liveObjectSet.upper_bound(ptr);
        if (iter != liveObjectSet.begin()) {
            --iter;
            if (static_cast<char*>(ptr) - static_cast<char*>(*iter) < size) {
                void* actualPtr = *iter;
                if (liveObjectSet.count(actualPtr)
                    && !verse_heap_is_marked(actualPtr)) {
                    cout << "Survivor we tried to mark is not marked: " << actualPtr << "\n";
                    cout << "We tried to mark it with ptr = " << ptr << "\n";
                    CHECK(!"Surviving object we tried to mark is not marked.");
                }
            }
        }
    }
}

#if PAS_ENABLE_TESTING
bool didCheckConservativeMarkingInViewUpForBump = false;

void checkConservativeMarkingInViewUpForBump(pas_race_test_hook_kind kind, va_list argList)
{
    if (kind != pas_race_test_hook_local_allocator_prepare_to_allocate_bump_case_after_allocate_black)
        return;

    va_arg(argList, pas_local_allocator*);
    pas_segregated_exclusive_view* view = va_arg(argList, pas_segregated_exclusive_view*);

    const pas_segregated_page_config* pageConfig = pas_segregated_page_config_kind_get_config(
        pas_compact_segregated_size_directory_ptr_load_non_null(&view->directory)->base.page_config_kind);

    if (!pas_segregated_page_config_is_verse(*pageConfig))
        return;

    uintptr_t pageBoundary = reinterpret_cast<uintptr_t>(view->page_boundary);
    pas_segregated_page* page = pas_segregated_page_for_boundary(view->page_boundary, *pageConfig);

    for (size_t offset = pas_segregated_page_offset_from_page_boundary_to_first_object_exclusive(page->object_size,
                                                                                                 *pageConfig);
         offset < pas_segregated_page_offset_from_page_boundary_to_end_of_last_object_exclusive(page->object_size,
                                                                                                *pageConfig);
         offset += page->object_size) {
        uintptr_t ptrAsInt = pageBoundary + offset;
        void* ptr = reinterpret_cast<void*>(ptrAsInt);
        CHECK_EQUAL(verse_heap_find_allocated_object_start(ptrAsInt), 0);
        CHECK(verse_heap_is_marked(ptr));
    }
    
    didCheckConservativeMarkingInViewUpForBump = true;
}

void testConservativeMarkDuringPrepareForBumpAllocation(size_t size)
{
    initializeOnlyDefaultHeap();
	verse_heap_mark_bits_page_commit_controller_lock();

    verse_heap_start_allocating_black_before_handshake();

    CHECK(!didCheckConservativeMarkingInViewUpForBump);
    pas_race_test_hook_callback_instance = checkConservativeMarkingInViewUpForBump;
    verse_heap_allocate(defaultHeap, 16);
    CHECK(didCheckConservativeMarkingInViewUpForBump);
}

set<void*> objectsThatShouldGetMarked;

void checkConservativeMarkingInViewUpForBits(pas_race_test_hook_kind kind, va_list argList)
{
    if (kind != pas_race_test_hook_local_allocator_scan_bits_to_set_up_free_bits_loop_after_allocate_black)
        return;

    va_arg(argList, pas_local_allocator*);
    uintptr_t pageBoundary = va_arg(argList, uintptr_t);
    const pas_segregated_page_config* pageConfig = va_arg(argList, const pas_segregated_page_config*);

    if (!pas_segregated_page_config_is_verse(*pageConfig))
        return;

    pas_segregated_page* page = pas_segregated_page_for_boundary(reinterpret_cast<void*>(pageBoundary), *pageConfig);

    for (size_t offset = pas_segregated_page_offset_from_page_boundary_to_first_object_exclusive(page->object_size,
                                                                                                 *pageConfig);
         offset < pas_segregated_page_offset_from_page_boundary_to_end_of_last_object_exclusive(page->object_size,
                                                                                                *pageConfig);
         offset += page->object_size) {
        uintptr_t ptrAsInt = pageBoundary + offset;
        void* ptr = reinterpret_cast<void*>(ptrAsInt);
        if (objectsThatShouldGetMarked.count(ptr)) {
            CHECK_EQUAL(verse_heap_find_allocated_object_start(ptrAsInt), ptrAsInt);
            CHECK(!verse_heap_is_marked(ptr));
        } else {
            // Here are the possibilities we're fine with:
            // - Object is allocated an marked.
            // - Object is not allocated and marked.
            // - Object is not allocated and not marked.
            // The one case we don't want is:
            // - Object is allocated and not marked.
            CHECK(!verse_heap_find_allocated_object_start(ptrAsInt) || verse_heap_is_marked(ptr));
        }
    }
    
    didCheckConservativeMarkingInViewUpForBump = true;
}

void testConservativeMarkDuringPrepareForBitsAllocation(size_t size)
{
    initializeOnlyDefaultHeap();
	verse_heap_mark_bits_page_commit_controller_lock();

    objectsThatShouldGetMarked.insert(verse_heap_allocate(defaultHeap, 16));

    verse_heap_start_allocating_black_before_handshake();
    handshakeOnOneThread();

    CHECK(!didCheckConservativeMarkingInViewUpForBump);
    pas_race_test_hook_callback_instance = checkConservativeMarkingInViewUpForBits;
    verse_heap_allocate(defaultHeap, 16);
    CHECK(didCheckConservativeMarkingInViewUpForBump);
}
#endif // PAS_ENABLE_TESTING

void testCreateHeap(size_t minAlign, size_t reservationSize, size_t reservationAlignment)
{
    pas_heap* heap = verse_heap_create(minAlign, reservationSize, reservationAlignment);
    verse_heap_did_become_ready_for_allocation();
    void* ptr = verse_heap_allocate(heap, 0);
    CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(ptr), minAlign));
    if (reservationSize) {
        void* base = verse_heap_get_base(heap);
        CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(base), reservationAlignment));
        CHECK_GREATER_EQUAL(ptr, base);
        CHECK_LESS(reinterpret_cast<char*>(ptr), reinterpret_cast<char*>(base) + reservationSize);
    }
}

void expectedNotToGetCalledObjectCallback(void* object, void* arg)
{
	PAS_UNUSED_PARAM(object);
	CHECK(!arg);
	CHECK(!"Should not be reached");
}

void testRepeatedIteration(size_t size, unsigned count, bool refillEachTime)
{
	initializeOnlyDefaultHeap();
	verse_heap_mark_bits_page_commit_controller_lock();

	mutex handshakeLock;
	bool allocatorIsDone = false;
	pas_thread_local_cache_node* node = nullptr;
	
	thread allocator([&] () {
		{
			lock_guard<mutex> guard(handshakeLock);
			node = verse_heap_get_thread_local_cache_node();
		}
		for (unsigned index = count; index--;) {
			lock_guard<mutex> guard(handshakeLock);
			if (refillEachTime)
				stopAllocatorsForNode(node);
			verse_heap_allocate(defaultHeap, size);
		}
		lock_guard<mutex> guard(handshakeLock);
		stopAllocatorsForNode(node);
		allocatorIsDone = true;
		node = nullptr;
	});

	uint64_t numIterations = 0;
	while (!allocatorIsDone) {
		verse_heap_object_set_start_iterate_before_handshake(&verse_heap_all_objects);
		{
			lock_guard<mutex> guard(handshakeLock);
			if (node)
				stopAllocatorsForNode(node);
		}
		size_t iterateSize = verse_heap_object_set_start_iterate_after_handshake(&verse_heap_all_objects);
		verse_heap_object_set_iterate_range(
			&verse_heap_all_objects, 0, iterateSize, verse_heap_iterate_marked,
			expectedNotToGetCalledObjectCallback, nullptr);
		verse_heap_object_set_end_iterate(&verse_heap_all_objects);
		numIterations++;
	}
	cout << "    Did " << numIterations << " iterations.\n";

	allocator.join();
}

mutex scavengerLock;
condition_variable scavengerConditionVariable;
bool scavengerDidShutDown;

void scavengerWillShutDown()
{
	lock_guard<mutex> lock(scavengerLock);
	scavengerDidShutDown = true;
	pas_scavenger_will_shut_down_callback = nullptr;
	scavengerConditionVariable.notify_all();
}

void waitForScavengerShutdown()
{
	PAS_ASSERT(!scavengerDidShutDown);
	PAS_ASSERT(!pas_scavenger_will_shut_down_callback);
	
	pas_scavenger_will_shut_down_callback = scavengerWillShutDown;

	unique_lock<mutex> lock(scavengerLock);
	scavengerConditionVariable.wait(lock, [&] () {
		return scavengerDidShutDown;
	});

	scavengerDidShutDown = false;
	pas_scavenger_will_shut_down_callback = nullptr;
}

void testDecommitMarkBits()
{
	pas_scavenger_deep_sleep_timeout_in_milliseconds = 1.;
	pas_scavenger_period_in_milliseconds = 1.;
	pas_scavenger_max_epoch_delta = 1000ll * 1000ll;
	
	initializeOnlyDefaultHeap();

	void* smallObject = verse_heap_allocate(defaultHeap, 16);
	void* mediumObject = verse_heap_allocate(defaultHeap, 10000);
	void* largeObject = verse_heap_allocate(defaultHeap, 10000000);

	CHECK_EQUAL(verse_heap_get_object_kind(reinterpret_cast<uintptr_t>(smallObject)), pas_small_segregated_object_kind);
	CHECK_EQUAL(verse_heap_get_object_kind(reinterpret_cast<uintptr_t>(mediumObject)), pas_medium_segregated_object_kind);
	CHECK_EQUAL(verse_heap_get_object_kind(reinterpret_cast<uintptr_t>(largeObject)), pas_large_object_kind);

	CHECK(verse_heap_object_is_allocated(smallObject));
	CHECK(verse_heap_object_is_allocated(mediumObject));
	CHECK(verse_heap_object_is_allocated(largeObject));

	auto areMarkBitsCommitted = [&] (void* object) -> bool {
		void* pageBase = reinterpret_cast<void*>(pas_round_down_to_power_of_2(reinterpret_cast<uintptr_t>(object), VERSE_HEAP_CHUNK_SIZE));
		size_t numCommittedPages = pas_count_committed_pages(pageBase, VERSE_HEAP_PAGE_SIZE, &allocationConfig);
		CHECK(!numCommittedPages || numCommittedPages == (VERSE_HEAP_PAGE_SIZE >> pas_page_malloc_alignment_shift()));
		return !!numCommittedPages;
	};

	verse_heap_mark_bits_page_commit_controller_lock();

	CHECK(areMarkBitsCommitted(smallObject));
	CHECK(areMarkBitsCommitted(mediumObject));
	CHECK(areMarkBitsCommitted(largeObject));

	CHECK(verse_heap_object_is_allocated(smallObject));
	CHECK(verse_heap_object_is_allocated(mediumObject));
	CHECK(verse_heap_object_is_allocated(largeObject));
	
	verse_heap_mark_bits_page_commit_controller_unlock();
	pas_scavenger_run_synchronously_now();

	CHECK(!areMarkBitsCommitted(smallObject));
	CHECK(!areMarkBitsCommitted(mediumObject));
	CHECK(!areMarkBitsCommitted(largeObject));

	CHECK(verse_heap_object_is_allocated(smallObject));
	CHECK(verse_heap_object_is_allocated(mediumObject));
	CHECK(verse_heap_object_is_allocated(largeObject));

	verse_heap_mark_bits_page_commit_controller_lock();

	CHECK(areMarkBitsCommitted(smallObject));
	CHECK(areMarkBitsCommitted(mediumObject));
	CHECK(areMarkBitsCommitted(largeObject));

	CHECK(verse_heap_object_is_allocated(smallObject));
	CHECK(verse_heap_object_is_allocated(mediumObject));
	CHECK(verse_heap_object_is_allocated(largeObject));

	verse_heap_mark_bits_page_commit_controller_unlock();
	waitForScavengerShutdown();

	CHECK(!areMarkBitsCommitted(smallObject));
	CHECK(!areMarkBitsCommitted(mediumObject));
	CHECK(!areMarkBitsCommitted(largeObject));

	CHECK(verse_heap_object_is_allocated(smallObject));
	CHECK(verse_heap_object_is_allocated(mediumObject));
	CHECK(verse_heap_object_is_allocated(largeObject));
	
	verse_heap_mark_bits_page_commit_controller_lock();

	CHECK(areMarkBitsCommitted(smallObject));
	CHECK(areMarkBitsCommitted(mediumObject));
	CHECK(areMarkBitsCommitted(largeObject));

	CHECK(verse_heap_object_is_allocated(smallObject));
	CHECK(verse_heap_object_is_allocated(mediumObject));
	CHECK(verse_heap_object_is_allocated(largeObject));
	
	verse_heap_start_allocating_black_before_handshake();
	handshakeOnOneThread();
	sweepOnOneThread();
	waitForScavengerShutdown();

	CHECK(areMarkBitsCommitted(smallObject));
	CHECK(areMarkBitsCommitted(mediumObject));
	CHECK(!areMarkBitsCommitted(largeObject));

	CHECK(!verse_heap_object_is_allocated(smallObject));
	CHECK(!verse_heap_object_is_allocated(mediumObject));
	CHECK(!verse_heap_object_is_allocated(largeObject));

	verse_heap_mark_bits_page_commit_controller_unlock();
	waitForScavengerShutdown();
	
	CHECK(!areMarkBitsCommitted(smallObject));
	CHECK(!areMarkBitsCommitted(mediumObject));
	CHECK(!areMarkBitsCommitted(largeObject));

	CHECK(!verse_heap_object_is_allocated(smallObject));
	CHECK(!verse_heap_object_is_allocated(mediumObject));
	CHECK(!verse_heap_object_is_allocated(largeObject));
}

void doOneSizeWorkflowTests(size_t size)
{
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            StartBlackAllocation,
                            Op(Mark, 0),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(Check, 0) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            StartBlackAllocation,
                            Op(Allocate, 1, Default, size),
                            Op(Mark, 0),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(Check, 0),
                            Op(Check, 1) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            StartBlackAllocation,
                            Op(Mark, 0),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(Allocate, 1, Default, size),
                            Op(Check, 0),
                            Op(Check, 1),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            StartBlackAllocation,
                            Op(Allocate, 1, Default, size),
                            Op(Mark, 0),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(Allocate, 2, Default, size),
                            Op(Check, 0),
                            Op(Check, 1),
                            Op(Check, 2),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ StartBlackAllocation,
                            Op(Allocate, 1, Default, size),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(Allocate, 2, Default, size),
                            Op(Check, 1),
                            Op(Check, 2),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ StartBlackAllocation,
                            StartSweep,
                            Op(Allocate, 0, Default, size),
                            FinishSweep,
                            EndSweep,
                            Op(Allocate, 1, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            Op(Allocate, 1, Default, size),
                            FinishSweep,
                            EndSweep,
                            Op(Check, 1),
                            Op(CheckNumObjects, 1),
                            Op(Allocate, 2, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            Op(Allocate, 1, Default, size),
                            FinishSweep,
                            EndSweep,
                            StopAllocators,
                            Op(Check, 1),
                            Op(CheckNumObjects, 1),
                            Op(Allocate, 2, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ StartBlackAllocation,
                            StartSweep,
                            Op(Allocate, 0, Default, size),
                            FinishSweep,
                            EndSweep,
                            Op(Check, 0),
                            Op(CheckNumObjects, 1),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ Op(Allocate, 1, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            Op(Allocate, 0, Default, size),
                            FinishSweep,
                            EndSweep,
                            Op(Check, 0),
                            Op(CheckNumObjects, 1),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            Op(Allocate, 0, Default, size),
                            EndSweep,
                            Op(Check, 0),
                            Op(CheckNumObjects, 1),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ Op(Allocate, 1, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            Op(Allocate, 0, Default, size),
                            EndSweep,
                            Op(Check, 0),
                            Op(CheckNumObjects, 1),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ StartBlackAllocation,
                            StartSweep,
                            Op(Allocate, 0, Default, size),
                            FinishSweep,
                            EndSweep,
                            Op(Check, 0),
                            Op(CheckNumObjects, 1),
                            Op(Allocate, 2, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ Op(Allocate, 1, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            Op(Allocate, 0, Default, size),
                            FinishSweep,
                            EndSweep,
                            Op(Check, 0),
                            Op(CheckNumObjects, 1),
                            Op(Allocate, 2, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            Op(Allocate, 0, Default, size),
                            EndSweep,
                            Op(Check, 0),
                            Op(CheckNumObjects, 1),
                            Op(Allocate, 2, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ Op(Allocate, 1, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            Op(Allocate, 0, Default, size),
                            EndSweep,
                            Op(Check, 0),
                            Op(CheckNumObjects, 1),
                            Op(Allocate, 2, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ StartBlackAllocation,
                            StartSweep,
                            Op(Allocate, 0, Default, size),
                            FinishSweep,
                            EndSweep,
                            Op(Check, 0),
                            Op(CheckNumObjects, 1),
                            Op(Allocate, 2, Default, size),
                            Op(Mark, 0),
                            Op(Mark, 2),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(CheckNumObjects, 2) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 1, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            Op(Allocate, 0, Default, size),
                            FinishSweep,
                            EndSweep,
                            Op(Check, 0),
                            Op(CheckNumObjects, 1),
                            Op(Allocate, 2, Default, size),
                            Op(Mark, 0),
                            Op(Mark, 2),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(CheckNumObjects, 2) }));
    ADD_TEST(testWorkflow({ StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            Op(Allocate, 0, Default, size),
                            EndSweep,
                            Op(Check, 0),
                            Op(CheckNumObjects, 1),
                            Op(Allocate, 2, Default, size),
                            Op(Mark, 0),
                            Op(Mark, 2),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(CheckNumObjects, 2) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 1, Default, size),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            Op(Allocate, 0, Default, size),
                            EndSweep,
                            Op(Check, 0),
                            Op(CheckNumObjects, 1),
                            Op(Allocate, 2, Default, size),
                            Op(Mark, 2),
                            Op(Mark, 0),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(CheckNumObjects, 2) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            Op(Allocate, 1, Default, size),
                            Op(Allocate, 2, Default, size),
                            StartBlackAllocation,
                            Op(StartIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 3),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(EndIterate, AllObjects),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            Op(Allocate, 1, Default, size),
                            Op(Allocate, 2, Default, size),
                            StartBlackAllocation,
                            Op(StartIterate, AllObjects, verse_heap_iterate_marked),
                            Op(CheckNumObjectsToSeeDuringIteration, 0),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_marked),
                            Op(EndIterate, AllObjects),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            CheckEmpty }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            Op(Allocate, 1, Default, size),
                            Op(Allocate, 2, Default, size),
                            Op(Mark, 0),
                            Op(Mark, 2),
                            StartBlackAllocation,
                            Op(StartIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 1),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(EndIterate, AllObjects),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(CheckNumObjects, 2) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            Op(Allocate, 1, Default, size),
                            Op(Allocate, 2, Default, size),
                            Op(Mark, 0),
                            Op(Mark, 2),
                            StartBlackAllocation,
                            Op(StartIterate, AllObjects, verse_heap_iterate_marked),
                            Op(CheckNumObjectsToSeeDuringIteration, 2),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_marked),
                            Op(EndIterate, AllObjects),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(CheckNumObjects, 2) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            Op(Allocate, 1, Default, size),
                            Op(Allocate, 2, Default, size),
                            Op(Mark, 0),
                            Op(Mark, 2),
                            StartBlackAllocation,
                            Op(StartIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 1),
                            Op(Allocate, 3, Default, size),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(EndIterate, AllObjects),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(CheckNumObjects, 3) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            Op(Allocate, 1, Default, size),
                            Op(Allocate, 2, Default, size),
                            Op(Mark, 0),
                            Op(Mark, 2),
                            StartBlackAllocation,
                            Op(StartIterate, AllObjects, verse_heap_iterate_marked),
                            Op(CheckNumObjectsToSeeDuringIteration, 2),
                            Op(Allocate, 3, Default, size),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_marked),
                            Op(EndIterate, AllObjects),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(CheckNumObjects, 3) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            Op(Allocate, 1, Default, size),
                            Op(Allocate, 2, Default, size),
                            Op(Mark, 0),
                            Op(Mark, 2),
                            StartBlackAllocation,
                            Op(StartIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 1),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(Allocate, 3, Default, size),
                            Op(EndIterate, AllObjects),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(CheckNumObjects, 3) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, size),
                            Op(Allocate, 1, Default, size),
                            Op(Allocate, 2, Default, size),
                            Op(Mark, 0),
                            Op(Mark, 2),
                            StartBlackAllocation,
                            Op(StartIterate, AllObjects, verse_heap_iterate_marked),
                            Op(CheckNumObjectsToSeeDuringIteration, 2),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_marked),
                            Op(Allocate, 3, Default, size),
                            Op(EndIterate, AllObjects),
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(CheckNumObjects, 3) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Things, size),
                            Op(Allocate, 1, Things, size),
                            Op(Allocate, 2, Things, size),
                            Op(Allocate, 3, Cage, size),
                            Op(StartIterate, ThingsSet, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 3),
                            Op(FinishIterate, ThingsSet, verse_heap_iterate_unmarked),
                            Op(EndIterate, ThingsSet),
                            Op(StartIterate, CageSet, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 1),
                            Op(FinishIterate, CageSet, verse_heap_iterate_unmarked),
                            Op(EndIterate, CageSet),
                            StartBlackAllocation,
                            StartSweep,
                            FinishSweep,
                            EndSweep,
                            Op(StartIterate, ThingsSet, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 0),
                            Op(FinishIterate, ThingsSet, verse_heap_iterate_unmarked),
                            Op(EndIterate, ThingsSet),
                            Op(StartIterate, CageSet, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 0),
                            Op(FinishIterate, CageSet, verse_heap_iterate_unmarked),
                            Op(EndIterate, CageSet) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Things, size),
                            Op(StartIterate, ThingsSet, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 1),
                            Op(Allocate, 1, Default, size),
                            Op(FinishIterate, ThingsSet, verse_heap_iterate_unmarked),
                            Op(EndIterate, ThingsSet) }));
}

} // anonymous namespace

#endif // PAS_ENABLE_VERSE && PAS_ENABLE_BMALLOC

void addVerseHeapTests()
{
    static constexpr bool verbose = false;

    if (verbose) {
        DUMP_VALUE(VERSE_HEAP_MIN_ALIGN_SHIFT);
        DUMP_VALUE(VERSE_HEAP_MIN_ALIGN);
        DUMP_VALUE(VERSE_HEAP_PAGE_SIZE_SHIFT);
        DUMP_VALUE(VERSE_HEAP_PAGE_SIZE);
        DUMP_VALUE(VERSE_HEAP_CHUNK_SIZE_SHIFT);
        DUMP_VALUE(VERSE_HEAP_CHUNK_SIZE);
        DUMP_VALUE(VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_BITS);
        DUMP_VALUE(VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_MASK);
        DUMP_VALUE(VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_SIZE);
        DUMP_VALUE(VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_BITS);
        DUMP_VALUE(VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_MASK);
        DUMP_VALUE(VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_SIZE);
        DUMP_VALUE(VERSE_HEAP_CHUNK_MAP_FIRST_LEVEL_SHIFT);
        DUMP_VALUE(VERSE_HEAP_CHUNK_MAP_SECOND_LEVEL_SHIFT);
    }
    
#if PAS_ENABLE_VERSE && PAS_ENABLE_BMALLOC
    ADD_TEST(testEmptyHeap());
    ADD_TEST(testAllocations(1, 0, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1, 1, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1, 7, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1, 16, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1, 100, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1, 100, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1, 666, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1, 1000, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1, 10000, pas_medium_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1, 100000, pas_medium_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1, 1000000, pas_large_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1, 10000000, pas_large_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1, 0, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(1, 1, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(1, 7, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(1, 16, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(1, 100, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(1, 100, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(1, 666, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(1, 1000, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(1, 10000, pas_medium_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(1, 100000, pas_medium_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(1, 1000000, pas_large_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(1, 10000000, pas_large_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(100000, 0, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(100000, 1, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(100000, 7, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(100000, 16, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(100000, 100, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(100000, 100, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(100000, 666, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(100000, 1000, pas_small_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(100000, 10000, pas_medium_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(10000, 100000, pas_medium_segregated_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(1000, 1000000, pas_large_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(100, 10000000, pas_large_object_kind, verse_heap_do_not_allocate_black));
    ADD_TEST(testAllocations(100000, 0, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(100000, 1, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(100000, 7, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(100000, 16, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(100000, 100, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(100000, 100, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(100000, 666, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(100000, 1000, pas_small_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(100000, 10000, pas_medium_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(10000, 100000, pas_medium_segregated_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(1000, 1000000, pas_large_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocations(100, 10000000, pas_large_object_kind, verse_heap_allocate_black));
    ADD_TEST(testAllocateDuringIteration(verse_heap_allocate_black, verse_heap_iterate_marked));
    ADD_TEST(testAllocateDuringIteration(verse_heap_allocate_black, verse_heap_iterate_unmarked));
    ADD_TEST(testAllocateDuringIteration(verse_heap_do_not_allocate_black, verse_heap_iterate_marked));
    ADD_TEST(testAllocateDuringIteration(verse_heap_do_not_allocate_black, verse_heap_iterate_unmarked));
    ADD_TEST(testWorkflow({ }));
    ADD_GROUP(doOneSizeWorkflowTests(16));
    ADD_GROUP(doOneSizeWorkflowTests(10000));
    ADD_GROUP(doOneSizeWorkflowTests(1000000));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, 42),
                            Op(Allocate, 1, Default, 22222),
                            Op(Allocate, 11, Default, 42),
                            Op(Mark, 0),
                            Op(Allocate, 2, Default, 666666666),
                            StartBlackAllocation,
                            Op(Allocate, 3, Default, 100),
                            Op(Mark, 3),
                            Op(Mark, 2),
                            Op(StartIterate, AllObjects, verse_heap_iterate_marked),
                            Op(CheckNumObjectsToSeeDuringIteration, 3),
                            Op(Allocate, 4, Default, 66666),
                            Op(IterateIncrement, AllObjects, verse_heap_iterate_marked),
                            Op(Allocate, 6, Default, 100),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_marked),
                            Op(Allocate, 5, Default, 42),
                            Op(EndIterate, AllObjects),
                            Op(StartIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 2),
                            Op(Allocate, 12, Default, 66666),
                            Op(IterateIncrement, AllObjects, verse_heap_iterate_unmarked),
                            Op(Allocate, 13, Default, 100),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(Allocate, 14, Default, 42),
                            Op(EndIterate, AllObjects),
                            Op(Allocate, 15, Default, 22222),
                            StartSweep,
                            Op(Allocate, 7, Default, 42),
                            SweepIncrement,
                            Op(Allocate, 8, Default, 100),
                            SweepIncrement,
                            Op(Allocate, 9, Default, 22222),
                            FinishSweep,
                            Op(Allocate, 10, Default, 42),
                            EndSweep,
                            Op(CheckNumObjects, 14) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Default, 42),
                            Op(Allocate, 1, Default, 22222),
                            Op(Allocate, 11, Default, 42),
                            Op(Mark, 0),
                            Op(Allocate, 2, Default, 666666666),
                            StartBlackAllocation,
                            Op(Allocate, 3, Default, 100),
                            Op(Mark, 3),
                            Op(Mark, 2),
                            Op(StartIterate, AllObjects, verse_heap_iterate_marked),
                            Op(CheckNumObjectsToSeeDuringIteration, 3),
                            Op(Allocate, 4, Default, 666660000),
                            Op(IterateIncrement, AllObjects, verse_heap_iterate_marked),
                            Op(Allocate, 6, Default, 100),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_marked),
                            Op(Allocate, 5, Default, 42),
                            Op(EndIterate, AllObjects),
                            Op(StartIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 2),
                            Op(Allocate, 12, Default, 66666),
                            Op(IterateIncrement, AllObjects, verse_heap_iterate_unmarked),
                            Op(Allocate, 13, Default, 100000000),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(Allocate, 14, Default, 42),
                            Op(EndIterate, AllObjects),
                            Op(Allocate, 15, Default, 22222),
                            StartSweep,
                            Op(Allocate, 7, Default, 42),
                            SweepIncrement,
                            Op(Allocate, 8, Default, 100000000),
                            SweepIncrement,
                            Op(Allocate, 9, Default, 22222),
                            FinishSweep,
                            Op(Allocate, 10, Default, 42),
                            EndSweep,
                            Op(CheckNumObjects, 14) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Cage, 42),
                            Op(StartIterate, CageSet, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 1),
                            Op(FinishIterate, CageSet, verse_heap_iterate_unmarked),
                            Op(EndIterate, CageSet),
                            Op(StartIterate, ThingsSet, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 0),
                            Op(FinishIterate, ThingsSet, verse_heap_iterate_unmarked),
                            Op(EndIterate, ThingsSet) }));
    ADD_TEST(testWorkflow({ Op(Allocate, 0, Cage, 42),
                            Op(Allocate, 1, Default, 666),
                            Op(Allocate, 2, Things, 1000000),
                            Op(StartIterate, CageSet, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 1),
                            Op(FinishIterate, CageSet, verse_heap_iterate_unmarked),
                            Op(EndIterate, CageSet),
                            Op(StartIterate, ThingsSet, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 1),
                            Op(FinishIterate, ThingsSet, verse_heap_iterate_unmarked),
                            Op(EndIterate, ThingsSet),
                            Op(StartIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 3),
                            Op(FinishIterate, AllObjects, verse_heap_iterate_unmarked),
                            Op(EndIterate, AllObjects),
                            Op(StartIterate, CageAndThings, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 2),
                            Op(FinishIterate, CageAndThings, verse_heap_iterate_unmarked),
                            Op(EndIterate, CageAndThings),
                            Op(StartIterate, DefaultAndCage, verse_heap_iterate_unmarked),
                            Op(CheckNumObjectsToSeeDuringIteration, 2),
                            Op(FinishIterate, DefaultAndCage, verse_heap_iterate_unmarked),
                            Op(EndIterate, DefaultAndCage) }));
    ADD_TEST(testChaos(1, true));
    ADD_TEST(testChaos(10, false));
    ADD_TEST(testConservativeMarking(16, 1000000, 500000, false, 10000, 10000, 300000, 300000, 1, 1, false));
    ADD_TEST(testConservativeMarking(16, 1000000, 500000, false, 10000, 10000, 300000, 300000, 10, 10, false));
    ADD_TEST(testConservativeMarking(16, 1000000, 1000, true, 10000, 10000, 300000, 300000, 1, 1, false));
    ADD_TEST(testConservativeMarking(16, 1000000, 1000, true, 10000, 10000, 300000, 300000, 10, 10, false));
    ADD_TEST(testConservativeMarking(10000, 100000, 50000, false, 10000, 10000, 300000, 300000, 1, 1, false));
    ADD_TEST(testConservativeMarking(10000, 100000, 50000, false, 10000, 10000, 300000, 300000, 10, 10, false));
    ADD_TEST(testConservativeMarking(10000, 100000, 1000, true, 10000, 10000, 300000, 300000, 1, 1, false));
    ADD_TEST(testConservativeMarking(10000, 100000, 1000, true, 10000, 10000, 300000, 300000, 10, 10, false));
    ADD_TEST(testConservativeMarking(1000000, 1000, 500, true, 10000, 10000, 300000, 300000, 1, 1, false));
    ADD_TEST(testConservativeMarking(1000000, 1000, 500, true, 10000, 10000, 300000, 300000, 10, 10, false));
    ADD_TEST(testConservativeMarking(16, 1000000, 500000, false, 10000, 10000, 300000, 300000, 1, 1, true));
    ADD_TEST(testConservativeMarking(16, 1000000, 500000, false, 10000, 10000, 300000, 300000, 10, 10, true));
    ADD_TEST(testConservativeMarking(16, 1000000, 1000, true, 10000, 10000, 300000, 300000, 1, 1, true));
    ADD_TEST(testConservativeMarking(16, 1000000, 1000, true, 10000, 10000, 300000, 300000, 10, 10, true));
    ADD_TEST(testConservativeMarking(10000, 100000, 50000, false, 10000, 10000, 300000, 300000, 1, 1, true));
    ADD_TEST(testConservativeMarking(10000, 100000, 50000, false, 10000, 10000, 300000, 300000, 10, 10, true));
    ADD_TEST(testConservativeMarking(10000, 100000, 1000, true, 10000, 10000, 300000, 300000, 1, 1, true));
    ADD_TEST(testConservativeMarking(10000, 100000, 1000, true, 10000, 10000, 300000, 300000, 10, 10, true));
    ADD_TEST(testConservativeMarking(1000000, 1000, 500, true, 10000, 10000, 300000, 300000, 1, 1, true));
    ADD_TEST(testConservativeMarking(1000000, 1000, 500, true, 10000, 10000, 300000, 300000, 10, 10, true));
#if PAS_ENABLE_TESTING
    ADD_TEST(testConservativeMarkDuringPrepareForBumpAllocation(16));
    ADD_TEST(testConservativeMarkDuringPrepareForBumpAllocation(10000));
    ADD_TEST(testConservativeMarkDuringPrepareForBitsAllocation(16));
    ADD_TEST(testConservativeMarkDuringPrepareForBitsAllocation(10000));
#endif // PAS_ENABLE_TESTING
    ADD_TEST(testCreateHeap(1, 0, 0));
    ADD_TEST(testCreateHeap(8, 0, 0));
    ADD_TEST(testCreateHeap(16, 0, 0));
    ADD_TEST(testCreateHeap(32, 0, 0));
    ADD_TEST(testCreateHeap(1, 0, 666));
    ADD_TEST(testCreateHeap(1, 16 * 1024 * 1024, 1));
	ADD_TEST(testRepeatedIteration(16, 1000000, false));
	ADD_TEST(testRepeatedIteration(16, 1000000, true));
	ADD_TEST(testDecommitMarkBits());
#endif // PAS_ENABLE_VERSE && PAS_ENABLE_BMALLOC
}

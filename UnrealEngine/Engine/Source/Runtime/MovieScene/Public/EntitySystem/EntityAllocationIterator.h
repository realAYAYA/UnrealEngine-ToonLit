// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "EntitySystem/MovieSceneEntitySystemTypes.h"

namespace UE
{
namespace MovieScene
{

class FEntityManager;
struct FEntityAllocation;
struct FEntityAllocationIterator;

using FEntityAllocationIteratorItem = FEntityAllocationProxy;

/**
 * Object that iterates all entity allocations that match a specific filter
 */
struct MOVIESCENE_API FEntityAllocationIterator
{
	/**
	 * End iterator constructor
	 */
	explicit FEntityAllocationIterator(const FEntityManager* InManager);

	/**
	 * Construction from the entity manager to iterate, and a filter
	 *
	 * @param InManager        The entity manager to iterate
	 * @param InFilter         Filter that defines the components to match. Copied into this object.
	 */
	explicit FEntityAllocationIterator(const FEntityManager* InManager, const FEntityComponentFilter* InFilter);

	FEntityAllocationIterator(FEntityAllocationIterator&&);
	FEntityAllocationIterator& operator=(FEntityAllocationIterator&&);

	/**
	 * Destructor
	 */
	~FEntityAllocationIterator();


	/**
	 * Retrieve the entity allocation that this iterator represents. Only valid if this iterator != end()
	 */
	FEntityAllocationIteratorItem operator*() const;


	/**
	 * Increment this iterator to the next matching allocation. Only valid if this iterator != end()
	 */
	FEntityAllocationIterator& operator++();


	/**
	 * Test whether this iterator is valid (ie not at the end of the iteration)
	 */
	bool operator!=(const FEntityAllocationIterator& Other) const;

private:

	/**
	 * Find the index of the next allocation that matches our filter
	 */
	int32 FindMatchingAllocationStartingAt(int32 Index) const;

	/** Filter to match entity allocations against */
	const FEntityComponentFilter* Filter;

	/** Entity manager being iterated */
	const FEntityManager* Manager;

	/** Current allocation index or Manager->EntityAllocationMasks.GetMaxIndex() when finished */
	int32 AllocationIndex;
};


/**
 * Iterator proxy type returned from FEntityManager::Iterate that supports ranged-for iteration of all entity allocations matching the specified filter
 */
struct FEntityAllocationIteratorProxy
{
	FEntityAllocationIteratorProxy(const FEntityManager* InManager, const FEntityComponentFilter* InFilter)
		: Manager(InManager), Filter(InFilter)
	{}

	/**~ Do not use directly - for stl iteration */
	FEntityAllocationIterator begin() const { return FEntityAllocationIterator(Manager, Filter); }
	FEntityAllocationIterator end()  const  { return FEntityAllocationIterator(Manager);         }

private:

	const FEntityManager* Manager;
	const FEntityComponentFilter* Filter;
};


} // namespace MovieScene
} // namespace UE

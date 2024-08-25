// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Class.h"
#include "Containers/ArrayView.h"
#include "Containers/StridedView.h"
#include "Containers/UnrealString.h"
#include "MassEntityTypes.h"

struct FMassEntityManager;
struct FMassArchetypeData;
struct FMassExecutionContext;
struct FMassFragment;
struct FMassArchetypeChunkIterator;
struct FMassEntityQuery;
struct FMassArchetypeEntityCollection;
struct FMassEntityView;
struct FMassDebugger;
struct FMassArchetypeHelper;

typedef TFunction< void(FMassExecutionContext& /*ExecutionContext*/) > FMassExecuteFunction;
typedef TFunction< bool(const FMassExecutionContext& /*ExecutionContext*/) > FMassChunkConditionFunction;


//////////////////////////////////////////////////////////////////////
// FMassArchetypeHandle

// An opaque handle to an archetype
struct FMassArchetypeHandle final
{
	FMassArchetypeHandle() = default;
	bool IsValid() const { return DataPtr.IsValid(); }

	bool operator==(const FMassArchetypeHandle& Other) const { return DataPtr == Other.DataPtr; }
	bool operator!=(const FMassArchetypeHandle& Other) const { return DataPtr != Other.DataPtr; }

	MASSENTITY_API friend uint32 GetTypeHash(const FMassArchetypeHandle& Instance);
private:
	FMassArchetypeHandle(const TSharedPtr<FMassArchetypeData>& InDataPtr)
	: DataPtr(InDataPtr)
	{}
	TSharedPtr<FMassArchetypeData> DataPtr;

	friend FMassArchetypeHelper;
	friend FMassEntityManager;
};


//////////////////////////////////////////////////////////////////////
// FMassArchetypeEntityCollection 

/** A struct that converts an arbitrary array of entities of given Archetype into a sequence of continuous
 *  entity chunks. The goal is to have the user create an instance of this struct once and run through a bunch of
 *  systems. The runtime code usually uses FMassArchetypeChunkIterator to iterate on the chunk collection.
 */
struct MASSENTITY_API FMassArchetypeEntityCollection 
{
public:
	struct FArchetypeEntityRange
	{
		int32 ChunkIndex = INDEX_NONE;
		int32 SubchunkStart = 0;
		/** negative or 0-length means "all available entities within chunk" */
		int32 Length = 0;

		FArchetypeEntityRange() = default;
		explicit FArchetypeEntityRange(const int32 InChunkIndex, const int32 InSubchunkStart = 0, const int32 InLength = 0) : ChunkIndex(InChunkIndex), SubchunkStart(InSubchunkStart), Length(InLength) {}
		/** Note that we consider invalid-length chunks valid as long as ChunkIndex and SubchunkStart are valid */
		bool IsSet() const { return ChunkIndex != INDEX_NONE && SubchunkStart >= 0; }

		/** Checks if given InRange comes right after this instance */
		bool IsAdjacentAfter(const FArchetypeEntityRange& Other) const
		{
			return ChunkIndex == Other.ChunkIndex && SubchunkStart + Length == Other.SubchunkStart;
		}

		bool operator==(const FArchetypeEntityRange& Other) const
		{
			return ChunkIndex == Other.ChunkIndex && SubchunkStart == Other.SubchunkStart && Length == Other.Length;
		}
		bool operator!=(const FArchetypeEntityRange& Other) const { return !(*this == Other); }
	};

	enum EDuplicatesHandling
	{
		NoDuplicates,	// indicates that the caller guarantees there are no duplicates in the input Entities collection
						// note that in no-shipping builds a `check` will fail if duplicates are present.
		FoldDuplicates,	// indicates that it's possible that Entities contains duplicates. The input Entities collection 
						// will be processed and duplicates will be removed.
	};

	enum EInitializationType
	{
		GatherAll,	// default behavior, makes given FMassArchetypeEntityCollection instance represent all entities of the given archetype
		DoNothing,	// meant for procedural population by external code (like child classes)
	};

	using FEntityRangeArray = TArray<FArchetypeEntityRange>;
	using FConstEntityRangeArrayView = TConstArrayView<FArchetypeEntityRange>;

private:
	FEntityRangeArray Ranges;
	/** entity indices indicated by EntityRanges are only valid with given Archetype */
	FMassArchetypeHandle Archetype;

public:
	FMassArchetypeEntityCollection() = default;
	FMassArchetypeEntityCollection(const FMassArchetypeHandle& InArchetype, TConstArrayView<FMassEntityHandle> InEntities, EDuplicatesHandling DuplicatesHandling);
	explicit FMassArchetypeEntityCollection(const FMassArchetypeHandle& InArchetypeHandle, const EInitializationType Initialization = EInitializationType::GatherAll);
	explicit FMassArchetypeEntityCollection(TSharedPtr<FMassArchetypeData>& InArchetype, const EInitializationType Initialization = EInitializationType::GatherAll);
	FMassArchetypeEntityCollection(const FMassArchetypeHandle& InArchetypeHandle, FEntityRangeArray&& InEntityRanges)
		: Ranges(MoveTemp(InEntityRanges))
		, Archetype(InArchetypeHandle)
	{}

	FConstEntityRangeArrayView GetRanges() const { return Ranges; }
	const FMassArchetypeHandle& GetArchetype() const { return Archetype; }
	bool IsEmpty() const { return Ranges.Num() == 0 && Archetype.IsValid() == false; }
	bool IsSet() const { return Archetype.IsValid(); }
	void Reset() 
	{ 
		Archetype = FMassArchetypeHandle();
		Ranges.Reset();
	}

	/** The comparison function that checks if Other is identical to this. Intended for diagnostics/debugging. */
	bool IsSame(const FMassArchetypeEntityCollection& Other) const;

protected:
	friend struct FMassArchetypeEntityCollectionWithPayload;
	void BuildEntityRanges(TStridedView<const int32> TrueIndices);
	
private:
	void GatherChunksFromArchetype();
};

struct MASSENTITY_API FMassArchetypeEntityCollectionWithPayload
{
	explicit FMassArchetypeEntityCollectionWithPayload(const FMassArchetypeEntityCollection& InEntityCollection)
		: Entities(InEntityCollection)
	{
	}

	static void CreateEntityRangesWithPayload(const FMassEntityManager& EntitySubsystem, const TConstArrayView<FMassEntityHandle> Entities
		, const FMassArchetypeEntityCollection::EDuplicatesHandling DuplicatesHandling, FMassGenericPayloadView Payload
		, TArray<FMassArchetypeEntityCollectionWithPayload>& OutEntityCollections);

	const FMassArchetypeEntityCollection& GetEntityCollection() const { return Entities; }
	const FMassGenericPayloadViewSlice& GetPayload() const { return PayloadSlice; }

private:
	FMassArchetypeEntityCollectionWithPayload(const FMassArchetypeHandle& InArchetype, TStridedView<const int32> TrueIndices, FMassGenericPayloadViewSlice&& Payload);

	FMassArchetypeEntityCollection Entities;
	FMassGenericPayloadViewSlice PayloadSlice;
};

//////////////////////////////////////////////////////////////////////
// FMassArchetypeChunkIterator

/**
 *  The type used to iterate over given archetype's chunks, be it full, continuous chunks or sparse subchunks. It hides
 *  this details from the rest of the system.
 */
struct MASSENTITY_API FMassArchetypeChunkIterator
{
private:
	FMassArchetypeEntityCollection::FConstEntityRangeArrayView EntityRanges;
	int32 CurrentChunkIndex = 0;

public:
	explicit FMassArchetypeChunkIterator(const FMassArchetypeEntityCollection::FConstEntityRangeArrayView& InEntityRanges) : EntityRanges(InEntityRanges), CurrentChunkIndex(0) {}

	operator bool() const { return EntityRanges.IsValidIndex(CurrentChunkIndex) && EntityRanges[CurrentChunkIndex].IsSet(); }
	FMassArchetypeChunkIterator& operator++() { ++CurrentChunkIndex; return *this; }

	const FMassArchetypeEntityCollection::FArchetypeEntityRange* operator->() const { check(bool(*this)); return &EntityRanges[CurrentChunkIndex]; }
	const FMassArchetypeEntityCollection::FArchetypeEntityRange& operator*() const { check(bool(*this)); return EntityRanges[CurrentChunkIndex]; }
};

//////////////////////////////////////////////////////////////////////
// FMassRawEntityInChunkData

struct FMassRawEntityInChunkData 
{
	FMassRawEntityInChunkData() = default;
	FMassRawEntityInChunkData(uint8* InChunkRawMemory, const int32 InIndexWithinChunk)
        : ChunkRawMemory(InChunkRawMemory), IndexWithinChunk(InIndexWithinChunk)
	{}
	bool IsValid() const { return ChunkRawMemory != nullptr && IndexWithinChunk != INDEX_NONE; }
	bool operator==(const FMassRawEntityInChunkData & Other) const { return ChunkRawMemory == Other.ChunkRawMemory && IndexWithinChunk == Other.IndexWithinChunk; }

	uint8* ChunkRawMemory = nullptr;
	int32 IndexWithinChunk = INDEX_NONE;
};

//////////////////////////////////////////////////////////////////////
// FMassQueryRequirementIndicesMapping

using FMassFragmentIndicesMapping = TArray<int32, TInlineAllocator<16>>;

struct FMassQueryRequirementIndicesMapping
{
	FMassQueryRequirementIndicesMapping() = default;

	FMassFragmentIndicesMapping EntityFragments;
	FMassFragmentIndicesMapping ChunkFragments;
	FMassFragmentIndicesMapping ConstSharedFragments;
	FMassFragmentIndicesMapping SharedFragments;
	FORCEINLINE bool IsEmpty() const
	{
		return EntityFragments.Num() == 0 || ChunkFragments.Num() == 0;
	}
};

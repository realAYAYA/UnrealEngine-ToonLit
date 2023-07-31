// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassArchetypeTypes.h"
#include "MassEntityManager.h"
#include "MassArchetypeData.h"
#include "MassCommandBuffer.h"
#include "MassEntityUtils.h"

//////////////////////////////////////////////////////////////////////
// FMassArchetypeHandle

uint32 GetTypeHash(const FMassArchetypeHandle& Instance)
{
	return GetTypeHash(Instance.DataPtr.Get());
}

//////////////////////////////////////////////////////////////////////
// FMassArchetypeEntityCollection 

FMassArchetypeEntityCollection::FMassArchetypeEntityCollection(const FMassArchetypeHandle& InArchetype, TConstArrayView<FMassEntityHandle> InEntities, EDuplicatesHandling DuplicatesHandling)
	: Archetype(InArchetype)
{
	if (InEntities.Num() <= 0)
	{
		return;
	}

	const FMassArchetypeData* ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandle(InArchetype);
	const int32 NumEntitiesPerChunk = ArchetypeData ? ArchetypeData->GetNumEntitiesPerChunk() : MAX_int32;

	// InEntities has a real chance of not being sorted by AbsoluteIndex. We gotta fix that to optimize how we process the data 
	TArray<int32> TrueIndices;
	TrueIndices.AddUninitialized(InEntities.Num());
	int32 i = 0;
	if (ArchetypeData)
	{
		for (const FMassEntityHandle& Entity : InEntities)
		{
			TrueIndices[i++] = ArchetypeData->GetInternalIndexForEntity(Entity.Index);
		}
	}
	else
	{
		// special case, where we have a bunch of entities that have been built but not assigned an archetype yet.
		// we use their base index for the sake of sorting here. Will still get some perf benefits and we can keep using
		// FMassArchetypeEntityCollection as the generic batched API wrapper for entities
		for (const FMassEntityHandle& Entity : InEntities)
		{
			TrueIndices[i++] = Entity.Index;
		}
	}

	TrueIndices.Sort();

#if DO_GUARD_SLOW
	if (DuplicatesHandling == NoDuplicates)
	{
		// ensure there are no duplicates. 
		int32 PrevIndex = TrueIndices[0];
		for (int j = 1; j < TrueIndices.Num(); ++j)
		{
			checkf(TrueIndices[j] != PrevIndex, TEXT("InEntities contains duplicate while DuplicatesHandling is set to NoDuplicates"));
			if (TrueIndices[j] == PrevIndex)
			{
				// fix it, for development's sake
				DuplicatesHandling = FoldDuplicates;
				break;
			}
			PrevIndex = TrueIndices[j];
		}
	}
#endif // DO_GUARD_SLOW

	if (DuplicatesHandling == FoldDuplicates)
	{
		int32 PrevIndex = TrueIndices[0];
		for (int j = 1; j < TrueIndices.Num(); ++j)
		{
			if (TrueIndices[j] == PrevIndex)
			{
				const int32 Num = TrueIndices.Num();
				int Skip = 0;
				while ((j + ++Skip) < Num && TrueIndices[j + Skip] == PrevIndex);
				
				TrueIndices.RemoveAt(j, Skip, /*bAllowShrinking=*/false);
				--j;
				continue;
			}
			PrevIndex = TrueIndices[j];
		}
	}

	BuildEntityRanges(MakeStridedView<const int32>(TrueIndices));
}

void FMassArchetypeEntityCollection::BuildEntityRanges(TStridedView<const int32> TrueIndices)
{
	checkf(Ranges.Num() == 0, TEXT("Calling %s is valid only for initial configuration"), ANSI_TO_TCHAR(__FUNCTION__));

	const FMassArchetypeData* ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandle(Archetype);
	const int32 NumEntitiesPerChunk = ArchetypeData ? ArchetypeData->GetNumEntitiesPerChunk() : MAX_int32;

	// the following block of code is splitting up sorted AbsoluteIndices into 
	// continuous chunks
	int32 ChunkEnd = INDEX_NONE;
	FArchetypeEntityRange DummyChunk;
	FArchetypeEntityRange* SubChunkPtr = &DummyChunk;
	int32 SubchunkLen = 0;
	int32 PrevAbsoluteIndex = INDEX_NONE;
	for (const int32 Index : TrueIndices)
	{
		// if run across a chunk border or run into an index discontinuity 
		if (Index >= ChunkEnd || Index != (PrevAbsoluteIndex + 1))
		{
			SubChunkPtr->Length = SubchunkLen;
			// note that both ChunkIndex and ChunkEnd will change only if AbsoluteIndex >= ChunkEnd
			const int32 ChunkIndex = Index / NumEntitiesPerChunk;
			ChunkEnd = (ChunkIndex + 1) * NumEntitiesPerChunk;
			SubchunkLen = 0;
			// new subchunk
			const int32 SubchunkStart = Index % NumEntitiesPerChunk;
			SubChunkPtr = &Ranges.Add_GetRef(FArchetypeEntityRange(ChunkIndex, SubchunkStart));
		}
		++SubchunkLen;
		PrevAbsoluteIndex = Index;
	}

	SubChunkPtr->Length = SubchunkLen;
}

FMassArchetypeEntityCollection::FMassArchetypeEntityCollection(const FMassArchetypeHandle& InArchetypeHandle, const EInitializationType Initialization)
	: Archetype(InArchetypeHandle)
{
	if (Initialization == EInitializationType::GatherAll)
	{
		check(InArchetypeHandle.IsValid());
		GatherChunksFromArchetype();
	}
}

FMassArchetypeEntityCollection::FMassArchetypeEntityCollection(TSharedPtr<FMassArchetypeData>& InArchetype, const EInitializationType Initialization)
	: Archetype(FMassArchetypeHelper::ArchetypeHandleFromData(InArchetype))
{	
	if (Initialization == EInitializationType::GatherAll)
	{
		check(InArchetype.IsValid());
		GatherChunksFromArchetype();
	}
}

void FMassArchetypeEntityCollection::GatherChunksFromArchetype()
{
	if (const FMassArchetypeData* ArchetypePtr = FMassArchetypeHelper::ArchetypeDataFromHandle(Archetype))
	{
		const int32 ChunkCount = ArchetypePtr->GetChunkCount();
		Ranges.Reset(ChunkCount);
		for (int32 i = 0; i < ChunkCount; ++i)
		{
			Ranges.Add(FArchetypeEntityRange(i));
		}
	}
}

bool FMassArchetypeEntityCollection::IsSame(const FMassArchetypeEntityCollection& Other) const
{
	if (Archetype != Other.Archetype || Ranges.Num() != Other.Ranges.Num())
	{
		return false;
	}

	for (int i = 0; i < Ranges.Num(); ++i)
	{
		if (Ranges[i] != Other.Ranges[i])
		{
			return false;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
// FMassArchetypeEntityCollectionWithPayload

void FMassArchetypeEntityCollectionWithPayload::CreateEntityRangesWithPayload(const FMassEntityManager& EntityManager, const TConstArrayView<FMassEntityHandle> Entities
	, const FMassArchetypeEntityCollection::EDuplicatesHandling DuplicatesHandling, FMassGenericPayloadView Payload
	, TArray<FMassArchetypeEntityCollectionWithPayload>& OutEntityCollections)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("Mass_CreateEntityRangesWithPayload");

	check(Payload.Num() > 0);
	for (const FStructArrayView& Element : Payload.Content)
	{
		check(Entities.Num() == Element.Num());
	}

	struct FEntityInArchetype
	{
		int32 ArchetypeIndex = INDEX_NONE;
		int32 TrueIndex = INDEX_NONE;

		bool operator<(const FEntityInArchetype& Other) const
		{
			return ArchetypeIndex < Other.ArchetypeIndex || (ArchetypeIndex == Other.ArchetypeIndex && TrueIndex < Other.TrueIndex);
		}
	};

	TArray<FEntityInArchetype> EntityData;
	EntityData.AddUninitialized(Entities.Num());

	struct FArchetypeInfo
	{
		// @todo using a handle here is temporary. Once ArchetypeHandle switches to using an index we'll use that instead
		FMassArchetypeHandle Archetype;
		int32 Count = 0;

		bool operator==(const FMassArchetypeHandle& InArchetype) const
		{
			return Archetype == InArchetype;
		}
		bool operator==(const FArchetypeInfo& Other) const
		{
			return Archetype == Other.Archetype;
		}
	};
	TArray<FArchetypeInfo> Archetypes;

	for (int32 i = 0; i < Entities.Num(); ++i)
	{
		const FMassEntityHandle& Entity = Entities[i];
		const FMassArchetypeHandle ArchetypeHandle = EntityManager.GetArchetypeForEntity(Entity);
		const FMassArchetypeData* ArchetypePtr = FMassArchetypeHelper::ArchetypeDataFromHandle(ArchetypeHandle);
		
		// @todo if FMassArchetypeHandle used indices the look up would be a lot faster
		int32 ArchetypeIndex = INDEX_NONE;
		
		if (Archetypes.Find(FArchetypeInfo{ ArchetypeHandle, 0 }, ArchetypeIndex) == false)
		{
			ArchetypeIndex = Archetypes.Add({ ArchetypeHandle, 0 });
		}
		++Archetypes[ArchetypeIndex].Count;
		EntityData[i] = { ArchetypeIndex, ArchetypePtr ? ArchetypePtr->GetInternalIndexForEntity(Entity.Index) : Entity.Index };
	}

	UE::Mass::Utils::AbstractSort(Entities.Num(), [&EntityData](const int32 LHS, const int32 RHS)
		{
			return EntityData[LHS] < EntityData[RHS];
		}
		, [&EntityData, &Payload](const int32 A, const int32 B)
		{
			::Swap(EntityData[A], EntityData[B]);
			Payload.Swap(A, B);
		});

	int32 ProcessedEntitiesCount = 0;
	int32 ArchetypeIndex = 0;
	for (FArchetypeInfo& ArchetypeInfo : Archetypes)
	{		
		TArrayView<FEntityInArchetype> EntityDataSubset = MakeArrayView(&EntityData[ProcessedEntitiesCount], ArchetypeInfo.Count);
		ensure(EntityDataSubset[0].ArchetypeIndex == ArchetypeIndex);
		ensure(EntityDataSubset.Last().ArchetypeIndex == ArchetypeIndex);
		TStridedView<int32> TrueIndices = MakeStridedView(EntityDataSubset, &FEntityInArchetype::TrueIndex);

		FMassGenericPayloadViewSlice PayloadSubView(Payload, ProcessedEntitiesCount, ArchetypeInfo.Count);

		OutEntityCollections.Add(FMassArchetypeEntityCollectionWithPayload(ArchetypeInfo.Archetype, TrueIndices, MoveTemp(PayloadSubView)));

		ProcessedEntitiesCount += ArchetypeInfo.Count;
		++ArchetypeIndex;
	}
}

FMassArchetypeEntityCollectionWithPayload::FMassArchetypeEntityCollectionWithPayload(const FMassArchetypeHandle& InArchetype, TStridedView<const int32> TrueIndices, FMassGenericPayloadViewSlice&& InPayloadSlice)
	: Entities(InArchetype, FMassArchetypeEntityCollection::DoNothing)
	, PayloadSlice(InPayloadSlice)
{
	Entities.BuildEntityRanges(TrueIndices);
}

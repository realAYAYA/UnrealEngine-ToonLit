// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassArchetypeData.h"
#include "MassEntityTypes.h"
#include "Misc/StringBuilder.h"

//////////////////////////////////////////////////////////////////////
// FMassArchetypeData

void FMassArchetypeData::ForEachFragmentType(TFunction< void(const UScriptStruct* /*Fragment*/)> Function) const
{
	for (const FMassArchetypeFragmentConfig& FragmentData : FragmentConfigs)
	{
		Function(FragmentData.FragmentType);
	}
}

bool FMassArchetypeData::HasFragmentType(const UScriptStruct* FragmentType) const
{
	return (FragmentType && CompositionDescriptor.Fragments.Contains(*FragmentType));
}

void FMassArchetypeData::Initialize(const FMassArchetypeCompositionDescriptor& InCompositionDescriptor, const uint32 ArchetypeDataVersion)
{
	CreatedArchetypeDataVersion = ArchetypeDataVersion;
	CompositionDescriptor.Fragments = InCompositionDescriptor.Fragments;
	ConfigureFragments();

	// Tags
	CompositionDescriptor.Tags = InCompositionDescriptor.Tags;

	// Chunk fragments
	CompositionDescriptor.ChunkFragments = InCompositionDescriptor.ChunkFragments;
	TArray<const UScriptStruct*, TInlineAllocator<16>> ChunkFragmentList;
	CompositionDescriptor.ChunkFragments.ExportTypes(ChunkFragmentList);
	ChunkFragmentList.Sort(FScriptStructSortOperator());
	for (const UScriptStruct* ChunkFragmentType : ChunkFragmentList)
	{
		check(ChunkFragmentType);
		ChunkFragmentsTemplate.Emplace(ChunkFragmentType);
	}

	// Share fragments
	CompositionDescriptor.SharedFragments = InCompositionDescriptor.SharedFragments;

	EntityListOffsetWithinChunk = 0;
}

void FMassArchetypeData::InitializeWithSimilar(const FMassArchetypeData& BaseArchetype, FMassArchetypeCompositionDescriptor&& NewComposition, const uint32 ArchetypeDataVersion)
{
	checkf(IsInitialized() == false, TEXT("Trying to %s but this archetype has already been initialized"));

	CreatedArchetypeDataVersion = ArchetypeDataVersion;

	// note that we're calling this function rarely, so we can be a little bit inefficient here.
	CompositionDescriptor = MoveTemp(NewComposition);
	if (CompositionDescriptor.Fragments != NewComposition.Fragments)
	{
		ConfigureFragments();
	}
	else
	{
		FragmentConfigs = BaseArchetype.FragmentConfigs;
		FragmentIndexMap = BaseArchetype.FragmentIndexMap;
		TotalBytesPerEntity = BaseArchetype.TotalBytesPerEntity;
		NumEntitiesPerChunk = BaseArchetype.NumEntitiesPerChunk;
	}
	ChunkFragmentsTemplate = BaseArchetype.ChunkFragmentsTemplate;

	EntityListOffsetWithinChunk = 0;
}

void FMassArchetypeData::ConfigureFragments()
{
	TArray<const UScriptStruct*, TInlineAllocator<16>> SortedFragmentList;
	CompositionDescriptor.Fragments.ExportTypes(SortedFragmentList);

	SortedFragmentList.Sort(FScriptStructSortOperator());

	// Figure out how many bytes all of the individual fragments (and metadata) will cost per entity
	int32 FragmentSizeTallyBytes = 0;

	// Alignment padding computation is currently very conservative and over-estimated.
	int32 AlignmentPadding = 0;
	
	// Save room for the 'metadata' (entity array)
	FragmentSizeTallyBytes += sizeof(FMassEntityHandle);

	// Tally up the fragment sizes and place them in the index map
	FragmentConfigs.AddDefaulted(SortedFragmentList.Num());
	FragmentIndexMap.Reserve(SortedFragmentList.Num());

	for (int32 FragmentIndex = 0; FragmentIndex < SortedFragmentList.Num(); ++FragmentIndex)
	{
		const UScriptStruct* FragmentType = SortedFragmentList[FragmentIndex];
		checkSlow(FragmentType);
		FragmentConfigs[FragmentIndex].FragmentType = FragmentType;

		AlignmentPadding += FragmentType->GetMinAlignment();
		FragmentSizeTallyBytes += FragmentType->GetStructureSize();

		FragmentIndexMap.Add(FragmentType, FragmentIndex);
	}

	TotalBytesPerEntity = FragmentSizeTallyBytes;
	int32 ChunkAvailableSize = GetChunkAllocSize() - AlignmentPadding;
	check(TotalBytesPerEntity <= ChunkAvailableSize);

	NumEntitiesPerChunk = ChunkAvailableSize / TotalBytesPerEntity;

	// Set up the offsets for each fragment into the chunk data
	int32 CurrentOffset = NumEntitiesPerChunk * sizeof(FMassEntityHandle);
	for (FMassArchetypeFragmentConfig& FragmentData : FragmentConfigs)
	{
		CurrentOffset = Align(CurrentOffset, FragmentData.FragmentType->GetMinAlignment());
		FragmentData.ArrayOffsetWithinChunk = CurrentOffset;
		const int32 SizeOfThisFragmentArray = NumEntitiesPerChunk * FragmentData.FragmentType->GetStructureSize();
		CurrentOffset += SizeOfThisFragmentArray;
	}
}

void FMassArchetypeData::AddEntity(FMassEntityHandle Entity, const FMassArchetypeSharedFragmentValues& SharedFragmentValues)
{
	const int32 AbsoluteIndex = AddEntityInternal(Entity, SharedFragmentValues);

	// Initialize fragments
	const int32 ChunkIndex = AbsoluteIndex / NumEntitiesPerChunk;
	const int32 IndexWithinChunk = AbsoluteIndex % NumEntitiesPerChunk;
	FMassArchetypeChunk& Chunk = Chunks[ChunkIndex];
	for (const FMassArchetypeFragmentConfig& FragmentConfig : FragmentConfigs)
	{
		void* FragmentPtr = FragmentConfig.GetFragmentData(Chunk.GetRawMemory(), IndexWithinChunk);
		FragmentConfig.FragmentType->InitializeStruct(FragmentPtr);
	}
}

int32 FMassArchetypeData::AddEntityInternal(FMassEntityHandle Entity, const FMassArchetypeSharedFragmentValues& SharedFragmentValues)
{
	checkf(SharedFragmentValues.IsSorted(), TEXT("Expecting shared fragment values to be previously sorted"));
	checkfSlow(SharedFragmentValues.HasExactFragmentTypesMatch(CompositionDescriptor.SharedFragments), TEXT("Expecting values for every specified shared fragment in the archetype and only those"))

	int32 IndexWithinChunk = 0;
	int32 AbsoluteIndex = 0;
	int32 ChunkIndex = 0;
	int32 EmptyChunkIndex = INDEX_NONE;
	int32 EmptyAbsoluteIndex = INDEX_NONE;

	FMassArchetypeChunk* DestinationChunk = nullptr;
	// Check chunks for a free spot (trying to reuse the earlier ones first so later ones might get freed up) 
	//@TODO: This could be accelerated to include a cached index to the first chunk with free spots or similar
	for (FMassArchetypeChunk& Chunk : Chunks)
	{
		if (Chunk.GetNumInstances() == 0)
		{
			// Remember first empty chunk but continue looking for a chunk that has space and same group tag
			if (EmptyChunkIndex == INDEX_NONE)
			{
				EmptyChunkIndex = ChunkIndex;
				EmptyAbsoluteIndex = AbsoluteIndex;
			}
		}
		else if (Chunk.GetNumInstances() < NumEntitiesPerChunk && Chunk.GetSharedFragmentValues().IsEquivalent(SharedFragmentValues))
		{
			IndexWithinChunk = Chunk.GetNumInstances();
			AbsoluteIndex += IndexWithinChunk;

			Chunk.AddInstance();

			DestinationChunk = &Chunk;
			break;
		}
		AbsoluteIndex += NumEntitiesPerChunk;
		++ChunkIndex;
	}

	if (DestinationChunk == nullptr)
	{
		// Check if it is a recycled chunk
		if (EmptyChunkIndex != INDEX_NONE)
		{
			DestinationChunk = &Chunks[EmptyChunkIndex];
			DestinationChunk->Recycle(ChunkFragmentsTemplate, SharedFragmentValues);
			AbsoluteIndex = EmptyAbsoluteIndex;
		}
		else
		{
			DestinationChunk = &Chunks.Emplace_GetRef(GetChunkAllocSize(), ChunkFragmentsTemplate, SharedFragmentValues);
		}

		check(DestinationChunk);
		DestinationChunk->AddInstance();
	}

	// Add to the table and map
	EntityMap.Add(Entity.Index, AbsoluteIndex);
	DestinationChunk->GetEntityArrayElementRef(EntityListOffsetWithinChunk, IndexWithinChunk) = Entity;

	return AbsoluteIndex;
}

void FMassArchetypeData::RemoveEntity(FMassEntityHandle Entity)
{
	const int32 AbsoluteIndex = EntityMap.FindAndRemoveChecked(Entity.Index);

	// Destroy fragments
	const int32 ChunkIndex = AbsoluteIndex / NumEntitiesPerChunk;
	const int32 IndexWithinChunk = AbsoluteIndex % NumEntitiesPerChunk;
	FMassArchetypeChunk& Chunk = Chunks[ChunkIndex];

	for (const FMassArchetypeFragmentConfig& FragmentConfig : FragmentConfigs)
	{
		// Destroy the fragment data
		void* DyingFragmentPtr = FragmentConfig.GetFragmentData(Chunk.GetRawMemory(), IndexWithinChunk);
		FragmentConfig.FragmentType->DestroyStruct(DyingFragmentPtr);
	}

	RemoveEntityInternal(AbsoluteIndex);
}

void FMassArchetypeData::RemoveEntityInternal(const int32 AbsoluteIndex)
{
	const int32 ChunkIndex = AbsoluteIndex / NumEntitiesPerChunk;
	const int32 IndexWithinChunk = AbsoluteIndex % NumEntitiesPerChunk;

	FMassArchetypeChunk& Chunk = Chunks[ChunkIndex];

	const int32 IndexToSwapFrom = Chunk.GetNumInstances() - 1;

	// Remove and swap the last entry in the chunk to the location of the removed item (if it's not the same as the dying entry)
	if (IndexToSwapFrom != IndexWithinChunk)
	{
		for (const FMassArchetypeFragmentConfig& FragmentConfig : FragmentConfigs)
		{
			void* DyingFragmentPtr = FragmentConfig.GetFragmentData(Chunk.GetRawMemory(), IndexWithinChunk);
			void* MovingFragmentPtr = FragmentConfig.GetFragmentData(Chunk.GetRawMemory(), IndexToSwapFrom);

			// Move last entry
			FMemory::Memcpy(DyingFragmentPtr, MovingFragmentPtr, FragmentConfig.FragmentType->GetStructureSize());
		}

		// Update the entity table and map
		const FMassEntityHandle EntityBeingSwapped = Chunk.GetEntityArrayElementRef(EntityListOffsetWithinChunk, IndexToSwapFrom);
		Chunk.GetEntityArrayElementRef(EntityListOffsetWithinChunk, IndexWithinChunk) = EntityBeingSwapped;
		EntityMap.FindChecked(EntityBeingSwapped.Index) = AbsoluteIndex;
	}
	
	Chunk.RemoveInstance();

	// If the chunk itself is empty now, see if we can remove it entirely
	// Note: This is only possible for trailing chunks, to avoid messing up the absolute indices in the entities map
	while ((Chunks.Num() > 0) && (Chunks.Last().GetNumInstances() == 0))
	{
		Chunks.RemoveAt(Chunks.Num() - 1, 1, /*bAllowShrinking=*/ false);
	}
}

void FMassArchetypeData::BatchDestroyEntityChunks(FMassArchetypeEntityCollection::FConstEntityRangeArrayView EntityRangeContainer, TArray<FMassEntityHandle>& OutEntitiesRemoved)
{
	const int32 InitialOutEntitiesCount = OutEntitiesRemoved.Num();

	// Sorting the subchunks info so that subchunks of a given chunk are processed "from the back". Otherwise removing 
	// a subchunk from the front of the chunk would inevitably invalidate following subchunks' information.
	FMassArchetypeEntityCollection::FEntityRangeArray SortedRangeCollection(EntityRangeContainer);
	SortedRangeCollection.Sort([](const FMassArchetypeEntityCollection::FArchetypeEntityRange& A, const FMassArchetypeEntityCollection::FArchetypeEntityRange& B) 
		{ 
			return A.ChunkIndex < B.ChunkIndex || (A.ChunkIndex == B.ChunkIndex && A.SubchunkStart > B.SubchunkStart);
		});

	for (const FMassArchetypeEntityCollection::FArchetypeEntityRange EntityRange : SortedRangeCollection)
	{ 
		FMassArchetypeChunk& Chunk = Chunks[EntityRange.ChunkIndex];

		// gather entities we're about to remove
		FMassEntityHandle* DyingEntityPtr = &Chunk.GetEntityArrayElementRef(EntityListOffsetWithinChunk, EntityRange.SubchunkStart);
		OutEntitiesRemoved.Append(DyingEntityPtr, EntityRange.Length);

		for (const FMassArchetypeFragmentConfig& FragmentConfig : FragmentConfigs)
		{
			// Destroy the fragment data
			void* DyingFragmentPtr = FragmentConfig.GetFragmentData(Chunk.GetRawMemory(), EntityRange.SubchunkStart);
			FragmentConfig.FragmentType->DestroyStruct(DyingFragmentPtr, EntityRange.Length);
		}

		BatchRemoveEntitiesInternal(EntityRange.ChunkIndex, EntityRange.SubchunkStart, EntityRange.Length);
	}

	for (int i = InitialOutEntitiesCount; i < OutEntitiesRemoved.Num(); ++i)
	{
		EntityMap.FindAndRemoveChecked(OutEntitiesRemoved[i].Index);
	}

	// If the chunk itself is empty now, see if we can remove it entirely
	// Note: This is only possible for trailing chunks, to avoid messing up the absolute indices in the entities map
	while ((Chunks.Num() > 0) && (Chunks.Last().GetNumInstances() == 0))
	{
		Chunks.RemoveAt(Chunks.Num() - 1, 1, /*bAllowShrinking=*/ false);
	}
}

bool FMassArchetypeData::HasFragmentDataForEntity(const UScriptStruct* FragmentType, int32 EntityIndex) const
{
	return (FragmentType && CompositionDescriptor.Fragments.Contains(*FragmentType));
}

void* FMassArchetypeData::GetFragmentDataForEntityChecked(const UScriptStruct* FragmentType, int32 EntityIndex) const
{
	const FMassRawEntityInChunkData InternalIndex = MakeEntityHandle(EntityIndex);
	
	// failing the below Find means given entity's archetype is missing given FragmentType
	const int32 FragmentIndex = FragmentIndexMap.FindChecked(FragmentType);
	return GetFragmentData(FragmentIndex, InternalIndex);
}

void* FMassArchetypeData::GetFragmentDataForEntity(const UScriptStruct* FragmentType, int32 EntityIndex) const
{
	if (const int32* FragmentIndex = FragmentIndexMap.Find(FragmentType))
	{
		FMassRawEntityInChunkData InternalIndex = MakeEntityHandle(EntityIndex);
		// failing the below Find means given entity's archetype is missing given FragmentType
		return GetFragmentData(*FragmentIndex, InternalIndex);
	}
	return nullptr;
}

void FMassArchetypeData::SetFragmentsData(const FMassEntityHandle Entity, TArrayView<const FInstancedStruct> FragmentInstances)
{
	FMassRawEntityInChunkData InternalIndex = MakeEntityHandle(Entity);

	for (const FInstancedStruct& Instance : FragmentInstances)
	{
		const UScriptStruct* FragmentType = Instance.GetScriptStruct();
		check(FragmentType);
		const int32 FragmentIndex = FragmentIndexMap.FindChecked(FragmentType);
		void* FragmentMemory = GetFragmentData(FragmentIndex, InternalIndex);
		FragmentType->CopyScriptStruct(FragmentMemory, Instance.GetMemory());
	}
}

void FMassArchetypeData::SetFragmentData(FMassArchetypeEntityCollection::FConstEntityRangeArrayView EntityRangeContainer, const FInstancedStruct& FragmentSource)
{
	check(FragmentSource.IsValid());
	const UScriptStruct* FragmentType = FragmentSource.GetScriptStruct();
	check(FragmentType);
	const int32 FragmentIndex = FragmentIndexMap.FindChecked(FragmentType);
	const int32 FragmentTypeSize = FragmentType->GetStructureSize();
	const uint8* FragmentSourceMemory = FragmentSource.GetMemory();
	check(FragmentSourceMemory);
	
	for (FMassArchetypeChunkIterator ChunkIterator(EntityRangeContainer); ChunkIterator; ++ChunkIterator)
	{
		uint8* FragmentMemory = (uint8*)FragmentConfigs[FragmentIndex].GetFragmentData(Chunks[ChunkIterator->ChunkIndex].GetRawMemory(), ChunkIterator->SubchunkStart);
		for (int i = ChunkIterator->Length; i; --i, FragmentMemory += FragmentTypeSize)
		{
			FragmentType->CopyScriptStruct(FragmentMemory, FragmentSourceMemory);
		}
	}
}

void FMassArchetypeData::MoveEntityToAnotherArchetype(const FMassEntityHandle Entity, FMassArchetypeData& NewArchetype)
{
	check(&NewArchetype != this);

	const int32 AbsoluteIndex = EntityMap.FindAndRemoveChecked(Entity.Index);
	const int32 ChunkIndex = AbsoluteIndex / NumEntitiesPerChunk;
	const int32 IndexWithinChunk = AbsoluteIndex % NumEntitiesPerChunk;
	FMassArchetypeChunk& Chunk = Chunks[ChunkIndex];

	const int32 NewAbsoluteIndex = NewArchetype.AddEntityInternal(Entity, Chunk.GetSharedFragmentValues());
	const int32 NewChunkIndex = NewAbsoluteIndex / NewArchetype.NumEntitiesPerChunk;
	const int32 NewIndexWithinChunk = NewAbsoluteIndex % NewArchetype.NumEntitiesPerChunk;
	FMassArchetypeChunk& NewChunk = NewArchetype.Chunks[NewChunkIndex];

	MoveFragmentsToAnotherArchetypeInternal(NewArchetype, { NewChunk.GetRawMemory(), NewIndexWithinChunk }, { Chunk.GetRawMemory(), IndexWithinChunk }, /*Count=*/1);

	RemoveEntityInternal(AbsoluteIndex);
}

void FMassArchetypeData::ExecuteFunction(FMassExecutionContext& RunContext, const FMassExecuteFunction& Function, const FMassQueryRequirementIndicesMapping& RequirementMapping, FMassArchetypeEntityCollection::FConstEntityRangeArrayView EntityRangeContainer)
{
	if (GetNumEntities() == 0)
	{
		return;
	}

	// mz@todo to be removed
	RunContext.SetCurrentArchetypesTagBitSet(GetTagBitSet());

	uint32 PrevSharedFragmentValuesHash = UINT32_MAX;
	for (FMassArchetypeChunkIterator ChunkIterator(EntityRangeContainer); ChunkIterator; ++ChunkIterator)
	{
		FMassArchetypeChunk& Chunk = Chunks[ChunkIterator->ChunkIndex];

		const int32 ChunkLength = ChunkIterator->Length > 0 ? ChunkIterator->Length : (Chunk.GetNumInstances() - ChunkIterator->SubchunkStart);
		if (ChunkLength)
		{
			const uint32 SharedFragmentValuesHash = GetTypeHash(Chunk.GetSharedFragmentValues());
			if (PrevSharedFragmentValuesHash != SharedFragmentValuesHash)
			{
				PrevSharedFragmentValuesHash = SharedFragmentValuesHash;
				BindConstSharedFragmentRequirements(RunContext, Chunk.GetSharedFragmentValues(), RequirementMapping.ConstSharedFragments);
				BindSharedFragmentRequirements(RunContext, Chunk.GetSharedFragmentValues(), RequirementMapping.SharedFragments);
			}

			checkf((ChunkIterator->SubchunkStart + ChunkLength) <= Chunk.GetNumInstances() && ChunkLength > 0, TEXT("Invalid subchunk, it is going over the number of instances in the chunk or it is empty."));

			RunContext.SetCurrentChunkSerialModificationNumber(Chunk.GetSerialModificationNumber());
			BindChunkFragmentRequirements(RunContext, RequirementMapping.ChunkFragments, Chunk);
			BindEntityRequirements(RunContext, RequirementMapping.EntityFragments, Chunk, ChunkIterator->SubchunkStart, ChunkLength);

			Function(RunContext);
		}
	}
}

void FMassArchetypeData::ExecuteFunction(FMassExecutionContext& RunContext, const FMassExecuteFunction& Function, const FMassQueryRequirementIndicesMapping& RequirementMapping, const FMassChunkConditionFunction& ChunkCondition)
{
	if (GetNumEntities() == 0)
	{
		return;
	}

	// mz@todo to be removed
	RunContext.SetCurrentArchetypesTagBitSet(GetTagBitSet());

	uint32 PrevSharedFragmentValuesHash = UINT32_MAX;
	for (FMassArchetypeChunk& Chunk : Chunks)
	{
		if (Chunk.GetNumInstances())
		{
			const uint32 SharedFragmentValuesHash = GetTypeHash(Chunk.GetSharedFragmentValues());
			if (PrevSharedFragmentValuesHash != SharedFragmentValuesHash)
			{
				PrevSharedFragmentValuesHash = SharedFragmentValuesHash;
				BindConstSharedFragmentRequirements(RunContext, Chunk.GetSharedFragmentValues(), RequirementMapping.ConstSharedFragments);
				BindSharedFragmentRequirements(RunContext, Chunk.GetSharedFragmentValues(), RequirementMapping.SharedFragments);
			}

			RunContext.SetCurrentChunkSerialModificationNumber(Chunk.GetSerialModificationNumber());
			BindChunkFragmentRequirements(RunContext, RequirementMapping.ChunkFragments, Chunk);

			if (!ChunkCondition || ChunkCondition(RunContext))
			{
				BindEntityRequirements(RunContext, RequirementMapping.EntityFragments, Chunk, 0, Chunk.GetNumInstances());
				Function(RunContext);
			}
		}
	}
}

void FMassArchetypeData::ExecutionFunctionForChunk(FMassExecutionContext RunContext, const FMassExecuteFunction& Function, const FMassQueryRequirementIndicesMapping& RequirementMapping, const FMassArchetypeEntityCollection::FArchetypeEntityRange& EntityRange, const FMassChunkConditionFunction& ChunkCondition)
{
	FMassArchetypeChunk& Chunk = Chunks[EntityRange.ChunkIndex];
	const int32 ChunkLength = EntityRange.Length > 0 ? EntityRange.Length : (Chunk.GetNumInstances() - EntityRange.SubchunkStart);

	if (ChunkLength)
	{
		BindConstSharedFragmentRequirements(RunContext, Chunk.GetSharedFragmentValues(), RequirementMapping.ChunkFragments);
		BindSharedFragmentRequirements(RunContext, Chunk.GetSharedFragmentValues(), RequirementMapping.ChunkFragments);

		RunContext.SetCurrentArchetypesTagBitSet(GetTagBitSet());
		RunContext.SetCurrentChunkSerialModificationNumber(Chunk.GetSerialModificationNumber());
		BindChunkFragmentRequirements(RunContext, RequirementMapping.ChunkFragments, Chunk);

		if (!ChunkCondition || ChunkCondition(RunContext))
		{
			BindEntityRequirements(RunContext, RequirementMapping.EntityFragments, Chunk, EntityRange.SubchunkStart, ChunkLength);
			Function(RunContext);
		}
	}
}

void FMassArchetypeData::CompactEntities(const double TimeAllowed)
{
	const double TimeAllowedEnd = FPlatformTime::Seconds() + TimeAllowed;

	TMap<uint32, TArray<FMassArchetypeChunk*>> SortedChunksBySharedValues;
	for (FMassArchetypeChunk& Chunk : Chunks)
	{
		// Skip already full chunks
		const int32 NumInstances = Chunk.GetNumInstances();
		if (NumInstances > 0 && NumInstances < NumEntitiesPerChunk)
		{
			const uint32 SharedFragmentHash = GetTypeHash(Chunk.GetSharedFragmentValues());
			TArray<FMassArchetypeChunk*>& SortedChunks = SortedChunksBySharedValues.FindOrAddByHash(SharedFragmentHash, SharedFragmentHash, TArray<FMassArchetypeChunk*>());
			SortedChunks.Add(&Chunk);
		}
	}

	for (TPair<uint32, TArray<FMassArchetypeChunk*>>& Pair : SortedChunksBySharedValues)
	{
		TArray<FMassArchetypeChunk*>& SortedChunks = Pair.Value;

		// Check if there is anything to compact at all
		if (SortedChunks.Num() <= 1)
		{
			continue;
		}

		SortedChunks.Sort([](const FMassArchetypeChunk& LHS, const FMassArchetypeChunk& RHS)
		{
			return LHS.GetNumInstances() < RHS.GetNumInstances();
		});

		int32 ChunkToFillSortedIdx = 0;
		int32 ChunkToEmptySortedIdx = SortedChunks.Num() - 1;
		while (ChunkToFillSortedIdx < ChunkToEmptySortedIdx && FPlatformTime::Seconds() < TimeAllowedEnd)
		{
			while (ChunkToFillSortedIdx < SortedChunks.Num() && SortedChunks[ChunkToFillSortedIdx]->GetNumInstances() == NumEntitiesPerChunk)
			{
				ChunkToFillSortedIdx++;
			}
			while (ChunkToEmptySortedIdx >= 0 && SortedChunks[ChunkToEmptySortedIdx]->GetNumInstances() == 0)
			{
				ChunkToEmptySortedIdx--;
			}
			if (ChunkToFillSortedIdx >= ChunkToEmptySortedIdx)
			{
				break;
			}

			FMassArchetypeChunk* ChunkToFill = SortedChunks[ChunkToFillSortedIdx];
			FMassArchetypeChunk* ChunkToEmpty = SortedChunks[ChunkToEmptySortedIdx];
			const int32 NumberOfEntitiesToMove = FMath::Min(NumEntitiesPerChunk - ChunkToFill->GetNumInstances(), ChunkToEmpty->GetNumInstances());
			const int32 FromIndex = ChunkToEmpty->GetNumInstances() - NumberOfEntitiesToMove;
			const int32 ToIndex = ChunkToFill->GetNumInstances();
			check(NumberOfEntitiesToMove > 0);

			for (const FMassArchetypeFragmentConfig& FragmentConfig : FragmentConfigs)
			{
				void* FromFragmentPtr = FragmentConfig.GetFragmentData(ChunkToEmpty->GetRawMemory(), FromIndex);
				void* ToFragmentPtr = FragmentConfig.GetFragmentData(ChunkToFill->GetRawMemory(), ToIndex);
				// Move all entries
				FMemory::Memcpy(ToFragmentPtr, FromFragmentPtr, FragmentConfig.FragmentType->GetStructureSize() * NumberOfEntitiesToMove);
			}

			FMassEntityHandle* FromEntity = &ChunkToEmpty->GetEntityArrayElementRef(EntityListOffsetWithinChunk, FromIndex);
			FMassEntityHandle* ToEntity = &ChunkToFill->GetEntityArrayElementRef(EntityListOffsetWithinChunk, ToIndex);
			FMemory::Memcpy(ToEntity, FromEntity, NumberOfEntitiesToMove * sizeof(FMassEntityHandle));
			ChunkToFill->AddMultipleInstances(NumberOfEntitiesToMove);
			ChunkToEmpty->RemoveMultipleInstances(NumberOfEntitiesToMove);

			const int32 ChunkToFillIdx = UE_PTRDIFF_TO_INT32(ChunkToFill - &Chunks[0]);
			check(ChunkToFillIdx >= 0 && ChunkToFillIdx < Chunks.Num());
			const int32 AbsoluteIndex = ChunkToFillIdx * NumEntitiesPerChunk + ToIndex;

			for (int32 i = 0; i < NumberOfEntitiesToMove; i++, ++ToEntity)
			{
				EntityMap.FindChecked(ToEntity->Index) = AbsoluteIndex + i;
			}
		}
	}
}

void FMassArchetypeData::GetRequirementsFragmentMapping(TConstArrayView<FMassFragmentRequirementDescription> Requirements, FMassFragmentIndicesMapping& OutFragmentIndices) const
{
	OutFragmentIndices.Reset(Requirements.Num());
	for (const FMassFragmentRequirementDescription& Requirement : Requirements)
	{
		if (Requirement.RequiresBinding())
		{
			const int32* FragmentIndex = FragmentIndexMap.Find(Requirement.StructType);
			check(FragmentIndex != nullptr || Requirement.IsOptional());
			OutFragmentIndices.Add(FragmentIndex ? *FragmentIndex : INDEX_NONE);
		}
	}
}

void FMassArchetypeData::GetRequirementsChunkFragmentMapping(TConstArrayView<FMassFragmentRequirementDescription> ChunkRequirements, FMassFragmentIndicesMapping& OutFragmentIndices) const
{
	int32 LastFoundFragmentIndex = -1;
	OutFragmentIndices.Reset(ChunkRequirements.Num());
	for (const FMassFragmentRequirementDescription& Requirement : ChunkRequirements)
	{
		if (Requirement.RequiresBinding())
		{
			int32 FragmentIndex = INDEX_NONE;
			// mz@todo Add comment here as this code seems to be assuming a certain order for chunk fragments, please explain
			for (int32 i = LastFoundFragmentIndex + 1; i < ChunkFragmentsTemplate.Num(); ++i)
			{
				if (ChunkFragmentsTemplate[i].GetScriptStruct()->IsChildOf(Requirement.StructType))
				{
					FragmentIndex = i;
					break;
				}
			}

			check(FragmentIndex != INDEX_NONE || Requirement.IsOptional());
			OutFragmentIndices.Add(FragmentIndex);
			LastFoundFragmentIndex = FragmentIndex;
		}
	}
}

void FMassArchetypeData::GetRequirementsConstSharedFragmentMapping(TConstArrayView<FMassFragmentRequirementDescription> Requirements, FMassFragmentIndicesMapping& OutFragmentIndices) const
{
	if (Chunks.Num() == 0)
	{
		return;
	}
	// All shared fragment values for this archetype should have deterministic indices, so anyone will work to calculate them
	const FMassArchetypeSharedFragmentValues& SharedFragmentValues = Chunks[0].GetSharedFragmentValues();

	OutFragmentIndices.Reset(Requirements.Num());
	for (const FMassFragmentRequirementDescription& Requirement : Requirements)
	{
		if (Requirement.RequiresBinding())
		{
			const int32 FragmentIndex = SharedFragmentValues.GetConstSharedFragments().IndexOfByPredicate(FStructTypeEqualOperator(Requirement.StructType));
			check(FragmentIndex != INDEX_NONE || Requirement.IsOptional());
			OutFragmentIndices.Add(FragmentIndex);
		}
	}
}

void FMassArchetypeData::GetRequirementsSharedFragmentMapping(TConstArrayView<FMassFragmentRequirementDescription> Requirements, FMassFragmentIndicesMapping& OutFragmentIndices) const
{
	if (Chunks.Num() == 0)
	{
		return;
	}

	// All shared fragment values for this archetype should have deterministic indices, so anyone will work to calculate them
	const FMassArchetypeSharedFragmentValues& SharedFragmentValues = Chunks[0].GetSharedFragmentValues();

	OutFragmentIndices.Reset(Requirements.Num());
	for (const FMassFragmentRequirementDescription& Requirement : Requirements)
	{
		if (Requirement.RequiresBinding())
		{
			const int32 FragmentIndex = SharedFragmentValues.GetSharedFragments().IndexOfByPredicate(FStructTypeEqualOperator(Requirement.StructType));
			check(FragmentIndex != INDEX_NONE || Requirement.IsOptional());
			OutFragmentIndices.Add(FragmentIndex);
		}
	}
}

void FMassArchetypeData::BindEntityRequirements(FMassExecutionContext& RunContext, const FMassFragmentIndicesMapping& EntityFragmentsMapping, FMassArchetypeChunk& Chunk, const int32 SubchunkStart, const int32 SubchunkLength)
{
	// auto-correcting number of entities to process in case SubchunkStart +  SubchunkLength > Chunk.GetNumInstances()
	const int32 NumEntities = SubchunkLength >= 0 ? FMath::Min(SubchunkLength, Chunk.GetNumInstances() - SubchunkStart) : Chunk.GetNumInstances();
	check(SubchunkStart >= 0 && SubchunkStart < Chunk.GetNumInstances());

	if (EntityFragmentsMapping.Num() > 0)
	{
		check(RunContext.GetMutableRequirements().Num() == EntityFragmentsMapping.Num());

		for (int i = 0; i < EntityFragmentsMapping.Num(); ++i)
		{
			FMassExecutionContext::FFragmentView& Requirement = RunContext.FragmentViews[i];
			const int32 FragmentIndex = EntityFragmentsMapping[i];

			check(FragmentIndex != INDEX_NONE || Requirement.Requirement.IsOptional());
			if (FragmentIndex != INDEX_NONE)
			{
				Requirement.FragmentView = TArrayView<FMassFragment>((FMassFragment*)GetFragmentData(FragmentIndex, Chunk.GetRawMemory(), SubchunkStart), NumEntities);
			}
			else
			{
				// @todo this might not be needed
				Requirement.FragmentView = TArrayView<FMassFragment>();
			}
		}
	}
	else
	{
		// Map in the required data arrays from the current chunk to the array views
		for (FMassExecutionContext::FFragmentView& Requirement : RunContext.GetMutableRequirements())
		{
			const int32* FragmentIndex = FragmentIndexMap.Find(Requirement.Requirement.StructType);
			check(FragmentIndex != nullptr || Requirement.Requirement.IsOptional());
			if (FragmentIndex)
			{
				Requirement.FragmentView = TArrayView<FMassFragment>((FMassFragment*)GetFragmentData(*FragmentIndex, Chunk.GetRawMemory(), SubchunkStart), NumEntities);
			}
			else
			{
				Requirement.FragmentView = TArrayView<FMassFragment>();
			}
		}
	}

	RunContext.EntityListView = TArrayView<FMassEntityHandle>(&Chunk.GetEntityArrayElementRef(EntityListOffsetWithinChunk, SubchunkStart), NumEntities);
}

void FMassArchetypeData::BindChunkFragmentRequirements(FMassExecutionContext& RunContext, const FMassFragmentIndicesMapping& ChunkFragmentsMapping, FMassArchetypeChunk& Chunk)
{
	if (ChunkFragmentsMapping.Num() > 0)
	{
		check(RunContext.GetMutableChunkRequirements().Num() == ChunkFragmentsMapping.Num());

		for (int i = 0; i < ChunkFragmentsMapping.Num(); ++i)
		{
			FMassExecutionContext::FChunkFragmentView& ChunkRequirement = RunContext.ChunkFragmentViews[i];
			const int32 ChunkFragmentIndex = ChunkFragmentsMapping[i];

			check(ChunkFragmentIndex != INDEX_NONE || ChunkRequirement.Requirement.IsOptional());
			ChunkRequirement.FragmentView = ChunkFragmentIndex != INDEX_NONE ? Chunk.GetMutableChunkFragmentViewChecked(ChunkFragmentIndex) : FStructView();
		}
	}
	else
	{
		for (FMassExecutionContext::FChunkFragmentView& ChunkRequirement : RunContext.GetMutableChunkRequirements())
		{
			FInstancedStruct* ChunkFragmentInstance = Chunk.FindMutableChunkFragment(ChunkRequirement.Requirement.StructType);
			check(ChunkFragmentInstance != nullptr || ChunkRequirement.Requirement.IsOptional());
			ChunkRequirement.FragmentView = ChunkFragmentInstance ? FStructView(*ChunkFragmentInstance) : FStructView();
		}
	}
}

void FMassArchetypeData::BindConstSharedFragmentRequirements(FMassExecutionContext& RunContext, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FMassFragmentIndicesMapping& FragmentsMapping)
{
	if (FragmentsMapping.Num() > 0)
	{
		check(RunContext.GetMutableConstSharedRequirements().Num() == FragmentsMapping.Num());

		for (int i = 0; i < FragmentsMapping.Num(); ++i)
		{
			FMassExecutionContext::FConstSharedFragmentView& Requirement = RunContext.ConstSharedFragmentViews[i];
			const int32 FragmentIndex = FragmentsMapping[i];

			check(FragmentIndex != INDEX_NONE || Requirement.Requirement.IsOptional());
			Requirement.FragmentView = FragmentIndex != INDEX_NONE ? SharedFragmentValues.GetConstSharedFragments()[FragmentIndex] : FConstSharedStruct();
		}
	}
	else
	{
		for (FMassExecutionContext::FConstSharedFragmentView& Requirement : RunContext.GetMutableConstSharedRequirements())
		{
			const FConstSharedStruct* SharedFragment = SharedFragmentValues.GetConstSharedFragments().FindByPredicate(FStructTypeEqualOperator(Requirement.Requirement.StructType) );
			check(SharedFragment != nullptr || Requirement.Requirement.IsOptional());
			Requirement.FragmentView = SharedFragment ? *SharedFragment : FConstSharedStruct();
		}
	}
}

void FMassArchetypeData::BindSharedFragmentRequirements(FMassExecutionContext& RunContext, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FMassFragmentIndicesMapping& FragmentsMapping)
{
	if (FragmentsMapping.Num() > 0)
	{
		check(RunContext.GetMutableSharedRequirements().Num() == FragmentsMapping.Num());

		for (int i = 0; i < FragmentsMapping.Num(); ++i)
		{
			FMassExecutionContext::FSharedFragmentView& Requirement = RunContext.SharedFragmentViews[i];
			const int32 FragmentIndex = FragmentsMapping[i];

			check(FragmentIndex != INDEX_NONE || Requirement.Requirement.IsOptional());
			Requirement.FragmentView = FragmentIndex != INDEX_NONE ? SharedFragmentValues.GetSharedFragments()[FragmentIndex] : FSharedStruct();
		}
	}
	else
	{
		for (FMassExecutionContext::FSharedFragmentView& Requirement : RunContext.GetMutableSharedRequirements())
		{
			const FSharedStruct* SharedFragment = SharedFragmentValues.GetSharedFragments().FindByPredicate(FStructTypeEqualOperator(Requirement.Requirement.StructType));
			check(SharedFragment != nullptr || Requirement.Requirement.IsOptional());
			Requirement.FragmentView = SharedFragment ? *SharedFragment : FSharedStruct();
		}
	}
}

SIZE_T FMassArchetypeData::GetAllocatedSize() const
{
	int32 NumAllocatedChunkBuffers = 0;
	for (const FMassArchetypeChunk& Chunk : Chunks)
	{
		if (Chunk.GetRawMemory() != nullptr)
		{
			++NumAllocatedChunkBuffers;
		}
	}

	return sizeof(FMassArchetypeData) +
		ChunkFragmentsTemplate.GetAllocatedSize() +
		FragmentConfigs.GetAllocatedSize() +
		Chunks.GetAllocatedSize() +
		(NumAllocatedChunkBuffers * GetChunkAllocSize()) +
		EntityMap.GetAllocatedSize() +
		FragmentIndexMap.GetAllocatedSize();
}

FString FMassArchetypeData::DebugGetDescription() const
{
#if WITH_MASSENTITY_DEBUG
	FStringOutputDevice OutDescription;

	if (!DebugNames.IsEmpty())
	{
		OutDescription += TEXT("Name: ");
		OutDescription += GetCombinedDebugNamesAsString();
		OutDescription += TEXT("\n");
	}
	OutDescription += TEXT("Chunk fragments: ");
	CompositionDescriptor.ChunkFragments.DebugGetStringDesc(OutDescription);
	OutDescription += TEXT("\nTags: ");
	CompositionDescriptor.Tags.DebugGetStringDesc(OutDescription);
	OutDescription += TEXT("\nFragments: ");
	CompositionDescriptor.Fragments.DebugGetStringDesc(OutDescription);
	
	return static_cast<FString>(OutDescription);
#else
	return {};
#endif
}

FString FMassArchetypeData::GetCombinedDebugNamesAsString() const
{
	TStringBuilder<256> StringBuilder;
	for (int i = 0; i < DebugNames.Num(); i++)
	{
		if (i > 0)
		{
			StringBuilder.Append(TEXT(", "));;
		}
		StringBuilder.Append(DebugNames[i].ToString());
	}
	return StringBuilder.ToString();
}

#if WITH_MASSENTITY_DEBUG
void FMassArchetypeData::DebugPrintArchetype(FOutputDevice& Ar)
{
	Ar.Logf(ELogVerbosity::Log, TEXT("Name: %s"), *GetCombinedDebugNamesAsString());

	FStringOutputDevice TagsDecription;
	CompositionDescriptor.Tags.DebugGetStringDesc(TagsDecription);
	Ar.Logf(ELogVerbosity::Log, TEXT("Tags: %s"), *TagsDecription);
	Ar.Logf(ELogVerbosity::Log, TEXT("Fragments: %s"), *DebugGetDescription());
	Ar.Logf(ELogVerbosity::Log, TEXT("\tChunks: %d x %d KB = %d KB total"), Chunks.Num(), GetChunkAllocSize() / 1024, (GetChunkAllocSize()*Chunks.Num()) / 1024);
	
	int ChunkWithFragmentsCount = 0;
	for (FMassArchetypeChunk& Chunk : Chunks)
	{
		ChunkWithFragmentsCount += Chunk.DebugGetChunkFragmentCount() > 0 ? 1 : 0;
	}
	if (ChunkWithFragmentsCount)
	{
		Ar.Logf(ELogVerbosity::Log, TEXT("\tChunks with fragments: %d"), ChunkWithFragmentsCount);
	}

	const int32 CurrentEntityCapacity = Chunks.Num() * NumEntitiesPerChunk;
	Ar.Logf(ELogVerbosity::Log, TEXT("\tEntity Count    : %d"), EntityMap.Num());
	Ar.Logf(ELogVerbosity::Log, TEXT("\tEntity Capacity : %d"), CurrentEntityCapacity);
	if (Chunks.Num() > 1)
	{
		const float Scaler = 100.0f / (float)CurrentEntityCapacity;
		// count non-last chunks to see how occupied they are
		int EntitiesPerChunkMin = CurrentEntityCapacity;
		int EntitiesPerChunkMax = 0;
		for (int ChunkIndex = 0; ChunkIndex < Chunks.Num() - 1; ++ChunkIndex)
		{
			const int Population = Chunks[ChunkIndex].GetNumInstances();
			EntitiesPerChunkMin = FMath::Min(Population, EntitiesPerChunkMin);
			EntitiesPerChunkMax = FMath::Max(Population, EntitiesPerChunkMax);
		}
		Ar.Logf(ELogVerbosity::Log, TEXT("\tEntity Occupancy: %.1f%% (min: %.1f%%, max: %.1f%%)"), Scaler * EntityMap.Num(), Scaler * EntitiesPerChunkMin, Scaler * EntitiesPerChunkMax);
	}
	else 
	{
		Ar.Logf(ELogVerbosity::Log, TEXT("\tEntity Occupancy: %.1f%%"), CurrentEntityCapacity > 0 ? ((EntityMap.Num() * 100.0f) / (float)CurrentEntityCapacity) : 0.f);
	}
	Ar.Logf(ELogVerbosity::Log, TEXT("\tBytes / Entity  : %d"), TotalBytesPerEntity);
	Ar.Logf(ELogVerbosity::Log, TEXT("\tEntities / Chunk: %d"), NumEntitiesPerChunk);

	Ar.Logf(ELogVerbosity::Log, TEXT("\tOffset 0x%04X: Entity[] (%d bytes each)"), EntityListOffsetWithinChunk, sizeof(FMassEntityHandle));
	int32 TotalBytesOfValidData = sizeof(FMassEntityHandle) * NumEntitiesPerChunk;
	for (const FMassArchetypeFragmentConfig& FragmentConfig : FragmentConfigs)
	{
		TotalBytesOfValidData += FragmentConfig.FragmentType->GetStructureSize() * NumEntitiesPerChunk;
		Ar.Logf(ELogVerbosity::Log, TEXT("\tOffset 0x%04X: %s[] (%d bytes each)"), FragmentConfig.ArrayOffsetWithinChunk, *FragmentConfig.FragmentType->GetName(), FragmentConfig.FragmentType->GetStructureSize());
	}

	//@TODO: Print out padding in between things?

	const int32 UnusuablePaddingOffset = TotalBytesPerEntity * NumEntitiesPerChunk;
	const int32 UnusuablePaddingAmount = GetChunkAllocSize() - UnusuablePaddingOffset;
	if (UnusuablePaddingAmount > 0)
	{
		Ar.Logf(ELogVerbosity::Log, TEXT("\tOffset 0x%04X: WastePadding[] (%d bytes total)"), UnusuablePaddingOffset, UnusuablePaddingAmount);
	}

	if (GetChunkAllocSize() != TotalBytesOfValidData + UnusuablePaddingAmount)
	{
		Ar.Logf(ELogVerbosity::Log, TEXT("\t@TODO: EXTRA PADDING HERE:  TotalBytesOfValidData: %d (%d missing)"), TotalBytesOfValidData, GetChunkAllocSize() - TotalBytesOfValidData);
	}
}

void FMassArchetypeData::DebugPrintEntity(FMassEntityHandle Entity, FOutputDevice& Ar, const TCHAR* InPrefix) const
{
	for (const FMassArchetypeFragmentConfig& FragmentConfig : FragmentConfigs)
	{
		void* Data = GetFragmentDataForEntityChecked(FragmentConfig.FragmentType, Entity.Index);
		
		FString FragmentName = FragmentConfig.FragmentType->GetName();
		FragmentName.RemoveFromStart(InPrefix);

		FString ValueStr;
		FragmentConfig.FragmentType->ExportText(ValueStr, Data, /*Default*/nullptr, /*OwnerObject*/nullptr, EPropertyPortFlags::PPF_IncludeTransient, /*ExportRootScope*/nullptr);

		Ar.Logf(TEXT("%s: %s"), *FragmentName, *ValueStr);
	}
}

#endif // WITH_MASSENTITY_DEBUG

void FMassArchetypeData::REMOVEME_GetArrayViewForFragmentInChunk(int32 ChunkIndex, const UScriptStruct* FragmentType, void*& OutChunkBase, int32& OutNumEntities)
{
	const FMassArchetypeChunk& Chunk = Chunks[ChunkIndex];
	const int32 FragmentIndex = FragmentIndexMap.FindChecked(FragmentType);

	OutChunkBase = FragmentConfigs[FragmentIndex].GetFragmentData(Chunk.GetRawMemory(), 0);
	OutNumEntities = Chunk.GetNumInstances();
}

//////////////////////////////////////////////////////////////////////
// FMassArchetypeData batched api

void FMassArchetypeData::BatchAddEntities(TConstArrayView<FMassEntityHandle> Entities, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, TArray<FMassArchetypeEntityCollection::FArchetypeEntityRange>& OutNewRanges)
{
	FMassArchetypeEntityCollection::FArchetypeEntityRange ResultSubchunk;
	ResultSubchunk.ChunkIndex = 0;
	int32 NumberMoved = 0;
	do 
	{
		ResultSubchunk = PrepareNextEntitiesSpanInternal(MakeArrayView(Entities.GetData() + NumberMoved, Entities.Num() - NumberMoved), SharedFragmentValues, ResultSubchunk.ChunkIndex);
		check(Chunks.IsValidIndex(ResultSubchunk.ChunkIndex) && Chunks[ResultSubchunk.ChunkIndex].IsValidSubChunk(ResultSubchunk.SubchunkStart, ResultSubchunk.Length));
		
		for (const FMassArchetypeFragmentConfig& FragmentConfig : FragmentConfigs)
		{
			void* FragmentPtr = FragmentConfig.GetFragmentData(Chunks[ResultSubchunk.ChunkIndex].GetRawMemory(), ResultSubchunk.SubchunkStart);
			FragmentConfig.FragmentType->InitializeStruct(FragmentPtr, ResultSubchunk.Length);
		}

		NumberMoved += ResultSubchunk.Length;

		OutNewRanges.Add(ResultSubchunk);

	} while (NumberMoved < Entities.Num());
}

void FMassArchetypeData::BatchMoveEntitiesToAnotherArchetype(const FMassArchetypeEntityCollection& EntityCollection, FMassArchetypeData& NewArchetype, TArray<FMassEntityHandle>& OutEntitesBeingMoved, TArray<FMassArchetypeEntityCollection::FArchetypeEntityRange>* OutNewRanges)
{
	check(&NewArchetype != this);

	// Sorting the subchunks info so that subchunks of a given chunk are processed "from the back". Otherwise removing 
	// a subchunk from the front of the chunk would inevitably invalidate following subchunks' information.
	TArray<FMassArchetypeEntityCollection::FArchetypeEntityRange> Subchunks(EntityCollection.GetRanges());
	Subchunks.Sort([](const FMassArchetypeEntityCollection::FArchetypeEntityRange& A, const FMassArchetypeEntityCollection::FArchetypeEntityRange& B)
		{
			return A.ChunkIndex < B.ChunkIndex || (A.ChunkIndex == B.ChunkIndex && A.SubchunkStart > B.SubchunkStart);
		});

	const int32 InitialOutEntitiesCount = OutEntitesBeingMoved.Num();

	for (const FMassArchetypeEntityCollection::FArchetypeEntityRange EntityRange : Subchunks)
	{
		FMassArchetypeChunk& Chunk = Chunks[EntityRange.ChunkIndex];

		// 0 - consider compacting new archetype to ensure larger empty spaces
		// 1. find next free spot in the destination archetype
		// 2. min(amount of elements) to move

		// gather entities we're about to remove
		FMassEntityHandle* DyingEntityPtr = &Chunk.GetEntityArrayElementRef(EntityListOffsetWithinChunk, EntityRange.SubchunkStart);
		const int32 EntitesBeingMovedStartIndex = OutEntitesBeingMoved.Num();
		OutEntitesBeingMoved.Append(DyingEntityPtr, EntityRange.Length);

		FMassArchetypeEntityCollection::FArchetypeEntityRange ResultSubChunk;
		ResultSubChunk.ChunkIndex = 0;
		ResultSubChunk.Length = 0;
		int32 NumberMoved = 0;

		do
		{
			const int32 IndexWithinChunk = EntityRange.SubchunkStart + NumberMoved;

			ResultSubChunk = NewArchetype.PrepareNextEntitiesSpanInternal(MakeArrayView(DyingEntityPtr + NumberMoved, EntityRange.Length - NumberMoved)
				, Chunk.GetSharedFragmentValues()
				, ResultSubChunk.ChunkIndex);

			FMassArchetypeChunk& NewChunk = NewArchetype.Chunks[ResultSubChunk.ChunkIndex];
			MoveFragmentsToAnotherArchetypeInternal(NewArchetype, {NewChunk.GetRawMemory(), ResultSubChunk.SubchunkStart}, {Chunk.GetRawMemory(), IndexWithinChunk}, ResultSubChunk.Length);

			NumberMoved += ResultSubChunk.Length;

			// @todo consider adding a 'merge sequences' pass at the end of the function since we can end up with 
			// ranges being right next to each other
			if (OutNewRanges)
			{
				OutNewRanges->Add(ResultSubChunk);
			}

		} while (NumberMoved < EntityRange.Length);

		BatchRemoveEntitiesInternal(EntityRange.ChunkIndex, EntityRange.SubchunkStart, EntityRange.Length);
	}

	for (int i = InitialOutEntitiesCount; i < OutEntitesBeingMoved.Num(); ++i)
	{
		EntityMap.FindAndRemoveChecked(OutEntitesBeingMoved[i].Index);
	}
}

FMassArchetypeEntityCollection::FArchetypeEntityRange FMassArchetypeData::PrepareNextEntitiesSpanInternal(TConstArrayView<FMassEntityHandle> Entities, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, int32 StartingChunk)
{
	checkf(SharedFragmentValues.IsSorted(), TEXT("Expecting shared fragment values to be previously sorted"));
	checkfSlow(SharedFragmentValues.HasExactFragmentTypesMatch(CompositionDescriptor.SharedFragments), TEXT("Expecting values for every specified shared fragment in the archetype and only those"))

	int32 StartIndexWithinChunk = INDEX_NONE;
	int32 AbsoluteStartIndex = 0;

	FMassArchetypeChunk* DestinationChunk = nullptr;
	
	int32 ChunkIndex = StartingChunk;
	// find a chunk with any room left
	for (; ChunkIndex < Chunks.Num(); ++ChunkIndex)
	{
		FMassArchetypeChunk& Chunk = Chunks[ChunkIndex];
		if (Chunk.GetNumInstances() < NumEntitiesPerChunk && Chunk.GetSharedFragmentValues().IsEquivalent(SharedFragmentValues))
		{
			StartIndexWithinChunk = Chunk.GetNumInstances();
			AbsoluteStartIndex = ChunkIndex * NumEntitiesPerChunk + StartIndexWithinChunk;

			DestinationChunk = &Chunk;

			if (StartIndexWithinChunk == 0)
			{
				Chunk.Recycle(ChunkFragmentsTemplate, SharedFragmentValues);
			}
			break;
		}
	}

	// if no chunk found create one
	if (DestinationChunk == nullptr)
	{
		ChunkIndex = Chunks.Num();
		AbsoluteStartIndex = Chunks.Num() * NumEntitiesPerChunk;
		StartIndexWithinChunk = 0;

		DestinationChunk = &Chunks.Emplace_GetRef(GetChunkAllocSize(), ChunkFragmentsTemplate, SharedFragmentValues);
	}

	check(DestinationChunk);

	// we might be able to fit in less entitites than requested
	const int32 NumToAdd = FMath::Min(NumEntitiesPerChunk - StartIndexWithinChunk, Entities.Num());
	check(NumToAdd);
	DestinationChunk->AddMultipleInstances(NumToAdd);

	// Add to the table and map
	int32 AbsoluteIndex = AbsoluteStartIndex;
	for (int32 i = 0; i < NumToAdd; ++i)
	{
		EntityMap.Add(Entities[i].Index, AbsoluteIndex++);
	}

	FMassEntityHandle* FirstAddedEntity = &DestinationChunk->GetEntityArrayElementRef(EntityListOffsetWithinChunk, StartIndexWithinChunk);
	FMemory::Memcpy(FirstAddedEntity, Entities.GetData(), sizeof(FMassEntityHandle) * NumToAdd);

	return FMassArchetypeEntityCollection::FArchetypeEntityRange(ChunkIndex, StartIndexWithinChunk, NumToAdd);
}

void FMassArchetypeData::BatchRemoveEntitiesInternal(const int32 ChunkIndex, const int32 StartIndexWithinChunk, const int32 NumberToRemove)
{
	FMassArchetypeChunk& Chunk = Chunks[ChunkIndex];
	
	const int32 NumberToMove = FMath::Min(Chunk.GetNumInstances() - (StartIndexWithinChunk + NumberToRemove), NumberToRemove);
	checkf(NumberToMove >= 0, TEXT("Trying to move a negative number of elements indicates a problem with sub-chunk indicators, it's possibly out of date."));
	const int32 NumberToCut = FMath::Max(NumberToRemove - NumberToMove, 0);

	if (NumberToMove > 0)
	{
		FMassEntityHandle* DyingEntityPtr = &Chunk.GetEntityArrayElementRef(EntityListOffsetWithinChunk, StartIndexWithinChunk);

		const int32 SwapStartIndex = Chunk.GetNumInstances() - NumberToMove;
		checkf((StartIndexWithinChunk + NumberToMove - 1) < SwapStartIndex, TEXT("Remove and Move ranges overlap"));

		MoveFragmentsToNewLocationInternal({ Chunk.GetRawMemory(), StartIndexWithinChunk }, { Chunk.GetRawMemory(), SwapStartIndex }, NumberToMove);
		
		// Update the entity table and map
		const FMassEntityHandle* MovingEntityPtr = &Chunk.GetEntityArrayElementRef(EntityListOffsetWithinChunk, SwapStartIndex);
		int32 AbsoluteIndex = ChunkIndex * NumEntitiesPerChunk + StartIndexWithinChunk;

		for (int i = 0; i < NumberToMove; ++i)
		{
			DyingEntityPtr[i] = MovingEntityPtr[i];
			EntityMap.FindChecked(MovingEntityPtr[i].Index) = AbsoluteIndex++;
		}
	}

	Chunk.RemoveMultipleInstances(NumberToRemove);

	// If the chunk itself is empty now, see if we can remove it entirely
	// Note: This is only possible for trailing chunks, to avoid messing up the absolute indices in the entities map
	while ((Chunks.Num() > 0) && (Chunks.Last().GetNumInstances() == 0))
	{
		Chunks.RemoveAt(Chunks.Num() - 1, 1, /*bAllowShrinking=*/ false);
	}
}

void FMassArchetypeData::MoveFragmentsToAnotherArchetypeInternal(FMassArchetypeData& TargetArchetype, FMassArchetypeData::FTransientChunkLocation Target
	, const FMassArchetypeData::FTransientChunkLocation Source, const int32 ElementsNum)
{
	// for every TargetArchetype's fragment see if it was in the old archetype as well and if so copy it's value. 
	// If not then initialize the fragment.
	for (const FMassArchetypeFragmentConfig& TargetFragmentConfig : TargetArchetype.FragmentConfigs)
	{
		const int32* OldFragmentIndex = FragmentIndexMap.Find(TargetFragmentConfig.FragmentType);
		void* Dst = TargetFragmentConfig.GetFragmentData(Target.RawChunkMemory, Target.IndexWithinChunk);

		// Only copy if the fragment type exists in both archetypes
		if (OldFragmentIndex)
		{
			const void* Src = FragmentConfigs[*OldFragmentIndex].GetFragmentData(Source.RawChunkMemory, Source.IndexWithinChunk);
			FMemory::Memcpy(Dst, Src, TargetFragmentConfig.FragmentType->GetStructureSize() * ElementsNum);
		}
		else
		{
			// the fragment's unique to the TargetArchetype need to be initialized
			// @todo we're doing it for tags here as well. A tiny bit of perf lost. Probably not worth adding a check
			// but something to keep in mind. Will go away once tags are more of an archetype fragment than entity's
			TargetFragmentConfig.FragmentType->InitializeStruct(Dst, ElementsNum);
		}
	}

	// Delete fragments that were left behind
	for (const FMassArchetypeFragmentConfig& FragmentConfig : FragmentConfigs)
	{
		// If the fragment is not in the new archetype, destroy it.
		const int32* NewFragmentIndex = TargetArchetype.FragmentIndexMap.Find(FragmentConfig.FragmentType);
		if (NewFragmentIndex == nullptr)
		{
			void* DyingFragmentPtr = FragmentConfig.GetFragmentData(Source.RawChunkMemory, Source.IndexWithinChunk);
			FragmentConfig.FragmentType->DestroyStruct(DyingFragmentPtr, ElementsNum);
		}
	}
}

FORCEINLINE void FMassArchetypeData::MoveFragmentsToNewLocationInternal(FMassArchetypeData::FTransientChunkLocation Target, const FMassArchetypeData::FTransientChunkLocation Source, const int32 NumberToMove)
{
	for (const FMassArchetypeFragmentConfig& FragmentConfig : FragmentConfigs)
	{
		void* DyingFragmentPtr = FragmentConfig.GetFragmentData(Target.RawChunkMemory, Target.IndexWithinChunk);
		void* MovingFragmentPtr = FragmentConfig.GetFragmentData(Source.RawChunkMemory, Source.IndexWithinChunk); 

		// Swap fragments to the empty space just created.
		FMemory::Memcpy(DyingFragmentPtr, MovingFragmentPtr, FragmentConfig.FragmentType->GetStructureSize() * NumberToMove);
	}
}

void FMassArchetypeData::BatchSetFragmentValues(TConstArrayView<FMassArchetypeEntityCollection::FArchetypeEntityRange> EntityCollection, const FMassGenericPayloadViewSlice& Payload)
{
	int32 EntitiesHandled = 0;

	for (const FMassArchetypeEntityCollection::FArchetypeEntityRange EntityRange : EntityCollection)
	{
		FMassArchetypeChunk& Chunk = Chunks[EntityRange.ChunkIndex];

		for (int i = 0; i < Payload.Num(); ++i)
		{
			FStructArrayView FragmentPayload = Payload[i];
			check(FragmentPayload.Num() - EntitiesHandled >= EntityRange.Length);

			const UScriptStruct& FragmentType = FragmentPayload.GetFragmentType();

			const int32 FragmentIndex = FragmentIndexMap.FindChecked(&FragmentType);
			void* Dst = FragmentConfigs[FragmentIndex].GetFragmentData(Chunk.GetRawMemory(), EntityRange.SubchunkStart);
			const void* Src = FragmentPayload.GetDataAt(EntitiesHandled);

			FragmentType.CopyScriptStruct(Dst, Src, EntityRange.Length);
		}

		EntitiesHandled += EntityRange.Length;
	}
}

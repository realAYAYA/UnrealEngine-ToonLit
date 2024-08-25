// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityManager.h"
#include "MassArchetypeData.h"
#include "MassCommandBuffer.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"
#include "VisualLogger/VisualLogger.h"
#include "MassExecutionContext.h"
#include "MassDebugger.h"


const FMassEntityHandle FMassEntityManager::InvalidEntity;

namespace UE::Mass::Private
{
	// note: this function doesn't set EntityHandle.SerialNumber
	void ConvertArchetypelessSubchunksIntoEntityHandles(FMassArchetypeEntityCollection::FConstEntityRangeArrayView Subchunks, TArray<FMassEntityHandle>& OutEntityHandles)
	{
		int32 TotalCount = 0;
		for (const FMassArchetypeEntityCollection::FArchetypeEntityRange& Subchunk : Subchunks)
		{
			TotalCount += Subchunk.Length;
		}

		int32 Index = OutEntityHandles.Num();
		OutEntityHandles.AddDefaulted(TotalCount);

		for (const FMassArchetypeEntityCollection::FArchetypeEntityRange& Subchunk : Subchunks)
		{
			for (int i = Subchunk.SubchunkStart; i < Subchunk.SubchunkStart + Subchunk.Length; ++i)
			{
				OutEntityHandles[Index++].Index = i;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////
// FMassEntityManager

FMassEntityManager::FMassEntityManager(UObject* InOwner)
	: ObserverManager(*this)
	, Owner(InOwner)
{
#if WITH_MASSENTITY_DEBUG
	DebugName = InOwner ? (InOwner->GetName() + TEXT("_EntityManager")) : TEXT("Unset");
#endif
}

FMassEntityManager::~FMassEntityManager()
{
	if (bInitialized)
	{
		Deinitialize();
	}
}

void FMassEntityManager::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	const SIZE_T MyExtraSize = Entities.GetAllocatedSize() + 
		EntityFreeIndexList.GetAllocatedSize() +
		(DeferredCommandBuffer != nullptr ? DeferredCommandBuffer->GetAllocatedSize() : 0) +
		FragmentHashToArchetypeMap.GetAllocatedSize() +
		FragmentTypeToArchetypeMap.GetAllocatedSize();
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(MyExtraSize);

	for (const auto& KVP : FragmentHashToArchetypeMap)
	{
		for (const TSharedPtr<FMassArchetypeData>& ArchetypePtr : KVP.Value)
		{
			CumulativeResourceSize.AddDedicatedSystemMemoryBytes(ArchetypePtr->GetAllocatedSize());
		}
	}
}

void FMassEntityManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FConstSharedStruct& Struct : ConstSharedFragments)
	{
		Struct.AddStructReferencedObjects(Collector);
	}

	for (FSharedStruct& Struct : SharedFragments)
	{
		Struct.AddStructReferencedObjects(Collector);
	}

	const class UScriptStruct* ScriptStruct = FMassObserverManager::StaticStruct();
	TWeakObjectPtr<const UScriptStruct> ScriptStructPtr{ScriptStruct};
	Collector.AddReferencedObjects(ScriptStructPtr, &ObserverManager);
}

void FMassEntityManager::Initialize()
{
	if (bInitialized)
	{
		UE_LOG(LogMass, Log, TEXT("Calling %hs on already initialized entity manager owned by %s")
			, __FUNCTION__, *GetNameSafe(Owner.Get()));
		return;
	}

	// Index 0 is reserved so we can treat that index as an invalid entity handle
	Entities.Add();
	SerialNumberGenerator.fetch_add(FMath::Max(1,NumReservedEntities));

	DeferredCommandBuffer = MakeShareable(new FMassCommandBuffer());

	// creating these bitset instances to populate respective bitset types' StructTrackers
	FMassFragmentBitSet Fragments;
	FMassTagBitSet Tags;
	FMassChunkFragmentBitSet ChunkFragments;
	FMassSharedFragmentBitSet LocalSharedFragments;

	for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
	{
		if (StructIt->IsChildOf(FMassFragment::StaticStruct()))
		{
			if (*StructIt != FMassFragment::StaticStruct())
			{
				Fragments.Add(**StructIt);
			}
		}
		else if (StructIt->IsChildOf(FMassTag::StaticStruct()))
		{
			if (*StructIt != FMassTag::StaticStruct())
			{
				Tags.Add(**StructIt);
			}
		}
		else if (StructIt->IsChildOf(FMassChunkFragment::StaticStruct()))
		{
			if (*StructIt != FMassChunkFragment::StaticStruct())
			{
				ChunkFragments.Add(**StructIt);
			}
		}
		else if (StructIt->IsChildOf(FMassSharedFragment::StaticStruct()))
		{
			if (*StructIt != FMassSharedFragment::StaticStruct())
			{
				LocalSharedFragments.Add(**StructIt);
			}
		}
	}
#if WITH_MASSENTITY_DEBUG
	RequirementAccessDetector.Initialize();	
	FMassDebugger::RegisterEntityManager(*this);
#endif // WITH_MASSENTITY_DEBUG

	bInitialized = true;
	bFirstCommandFlush = true;
}

void FMassEntityManager::PostInitialize()
{
	ensure(bInitialized);
	// this needs to be done after all the subsystems have been initialized since some processors might want to access
	// them during processors' initialization
	ObserverManager.Initialize();
}

void FMassEntityManager::Deinitialize()
{
	if (bInitialized)
	{
		// closing down so no point in actually flushing commands, but need to clean them up to avoid warnings on destruction
		DeferredCommandBuffer->CleanUp();

#if WITH_MASSENTITY_DEBUG
		FMassDebugger::UnregisterEntityManager(*this);
#endif // WITH_MASSENTITY_DEBUG

		bInitialized = false;
	}
	else
	{
		UE_LOG(LogMass, Log, TEXT("Calling %hs on already deinitialized entity manager owned by %s")
			, __FUNCTION__, *GetNameSafe(Owner.Get()));
	}
}

FMassArchetypeHandle FMassEntityManager::CreateArchetype(TConstArrayView<const UScriptStruct*> FragmentsAndTagsList, const FMassArchetypeCreationParams& CreationParams)
{
	FMassArchetypeCompositionDescriptor Composition;
	InternalAppendFragmentsAndTagsToArchetypeCompositionDescriptor(Composition, FragmentsAndTagsList);
	return CreateArchetype(Composition, CreationParams);
}

FMassArchetypeHandle FMassEntityManager::CreateArchetype(FMassArchetypeHandle SourceArchetype,
	TConstArrayView<const UScriptStruct*> FragmentsAndTagsList, const FMassArchetypeCreationParams& CreationParams)
{
	const FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(SourceArchetype);
	FMassArchetypeCompositionDescriptor Composition = ArchetypeData.GetCompositionDescriptor();
	InternalAppendFragmentsAndTagsToArchetypeCompositionDescriptor(Composition, FragmentsAndTagsList);
	return CreateArchetype(Composition, CreationParams);
}

FMassArchetypeHandle FMassEntityManager::CreateArchetype(const TSharedPtr<FMassArchetypeData>& SourceArchetype, const FMassFragmentBitSet& AddedFragments, const FMassArchetypeCreationParams& CreationParams)
{
	check(SourceArchetype.IsValid());
	checkf(AddedFragments.IsEmpty() == false, TEXT("%hs Adding an empty fragment list to an archetype is not supported."), __FUNCTION__);

	const FMassArchetypeCompositionDescriptor Composition(AddedFragments + SourceArchetype->GetFragmentBitSet(), SourceArchetype->GetTagBitSet(), SourceArchetype->GetChunkFragmentBitSet(), SourceArchetype->GetSharedFragmentBitSet());
	return CreateArchetype(Composition, CreationParams);
}

FMassArchetypeHandle FMassEntityManager::CreateArchetype(const FMassArchetypeCompositionDescriptor& Composition, const FMassArchetypeCreationParams& CreationParams)
{
	const uint32 TypeHash = Composition.CalculateHash();

	TArray<TSharedPtr<FMassArchetypeData>>& HashRow = FragmentHashToArchetypeMap.FindOrAdd(TypeHash);

	TSharedPtr<FMassArchetypeData> ArchetypeDataPtr;
	for (const TSharedPtr<FMassArchetypeData>& Ptr : HashRow)
	{
		if (Ptr->IsEquivalent(Composition))
		{
#if WITH_MASSENTITY_DEBUG
			// Keep track of all names for this archetype.
			if (!CreationParams.DebugName.IsNone())
			{
				Ptr->AddUniqueDebugName(CreationParams.DebugName);
			}
#endif // WITH_MASSENTITY_DEBUG
			if (CreationParams.ChunkMemorySize > 0 && CreationParams.ChunkMemorySize != Ptr->GetChunkAllocSize())
			{
				UE_LOG(LogMass, Warning, TEXT("Reusing existing Archetype, but the requested ChunkMemorySize is different. Requested %d, existing: %llu")
					, CreationParams.ChunkMemorySize, Ptr->GetChunkAllocSize());
			}
			ArchetypeDataPtr = Ptr;
			break;
		}
	}

	if (!ArchetypeDataPtr.IsValid())
	{
		// Important to pre-increment the version as the queries will use this value to do incremental updates
		++ArchetypeDataVersion;

		// Create a new archetype
		FMassArchetypeData* NewArchetype = new FMassArchetypeData(CreationParams);
		NewArchetype->Initialize(Composition, ArchetypeDataVersion);
		ArchetypeDataPtr = HashRow.Add_GetRef(MakeShareable(NewArchetype));
		AllArchetypes.Add(ArchetypeDataPtr);
		ensure(AllArchetypes.Num() == ArchetypeDataVersion);

		for (const FMassArchetypeFragmentConfig& FragmentConfig : NewArchetype->GetFragmentConfigs())
		{
			checkSlow(FragmentConfig.FragmentType)
			FragmentTypeToArchetypeMap.FindOrAdd(FragmentConfig.FragmentType).Add(ArchetypeDataPtr);
		}

		OnNewArchetypeEvent.Broadcast(FMassArchetypeHandle(ArchetypeDataPtr));
	}

	return FMassArchetypeHelper::ArchetypeHandleFromData(ArchetypeDataPtr);
}

FMassArchetypeHandle FMassEntityManager::InternalCreateSimilarArchetype(const TSharedPtr<FMassArchetypeData>& SourceArchetype, const FMassTagBitSet& OverrideTags)
{
	checkSlow(SourceArchetype.IsValid());
	const FMassArchetypeData& SourceArchetypeRef = *SourceArchetype.Get();
	FMassArchetypeCompositionDescriptor NewComposition(SourceArchetypeRef.GetFragmentBitSet(), OverrideTags, SourceArchetypeRef.GetChunkFragmentBitSet(), SourceArchetypeRef.GetSharedFragmentBitSet());
	return InternalCreateSimilarArchetype(SourceArchetypeRef, MoveTemp(NewComposition));
}

FMassArchetypeHandle FMassEntityManager::InternalCreateSimilarArchetype(const TSharedPtr<FMassArchetypeData>& SourceArchetype, const FMassFragmentBitSet& OverrideFragments)
{
	checkSlow(SourceArchetype.IsValid());
	const FMassArchetypeData& SourceArchetypeRef = *SourceArchetype.Get();
	FMassArchetypeCompositionDescriptor NewComposition(OverrideFragments, SourceArchetypeRef.GetTagBitSet(), SourceArchetypeRef.GetChunkFragmentBitSet(), SourceArchetypeRef.GetSharedFragmentBitSet());
	return InternalCreateSimilarArchetype(SourceArchetypeRef, MoveTemp(NewComposition));
}

FMassArchetypeHandle FMassEntityManager::InternalCreateSimilarArchetype(const FMassArchetypeData& SourceArchetypeRef, FMassArchetypeCompositionDescriptor&& NewComposition)
{
	const uint32 TypeHash = NewComposition.CalculateHash();

	TArray<TSharedPtr<FMassArchetypeData>>& HashRow = FragmentHashToArchetypeMap.FindOrAdd(TypeHash);

	TSharedPtr<FMassArchetypeData> ArchetypeDataPtr;
	for (const TSharedPtr<FMassArchetypeData>& Ptr : HashRow)
	{
		if (Ptr->IsEquivalent(NewComposition))
		{
			ArchetypeDataPtr = Ptr;
			break;
		}
	}

	if (!ArchetypeDataPtr.IsValid())
	{
		// Important to pre-increment the version as the queries will use this value to do incremental updates
		++ArchetypeDataVersion;

		// Create a new archetype
		FMassArchetypeData* NewArchetype = new FMassArchetypeData();
		NewArchetype->InitializeWithSimilar(SourceArchetypeRef, MoveTemp(NewComposition), ArchetypeDataVersion);
		NewArchetype->CopyDebugNamesFrom(SourceArchetypeRef);

		ArchetypeDataPtr = HashRow.Add_GetRef(MakeShareable(NewArchetype));
		AllArchetypes.Add(ArchetypeDataPtr);
		ensure(AllArchetypes.Num() == ArchetypeDataVersion);

		for (const FMassArchetypeFragmentConfig& FragmentConfig : NewArchetype->GetFragmentConfigs())
		{
			checkSlow(FragmentConfig.FragmentType)
			FragmentTypeToArchetypeMap.FindOrAdd(FragmentConfig.FragmentType).Add(ArchetypeDataPtr);
		}

		OnNewArchetypeEvent.Broadcast(FMassArchetypeHandle(ArchetypeDataPtr));
	}

	return FMassArchetypeHelper::ArchetypeHandleFromData(ArchetypeDataPtr);
}

void FMassEntityManager::InternalAppendFragmentsAndTagsToArchetypeCompositionDescriptor(
	FMassArchetypeCompositionDescriptor& InOutComposition, TConstArrayView<const UScriptStruct*> FragmentsAndTagsList) const
{
	for (const UScriptStruct* Type : FragmentsAndTagsList)
	{
		if (Type->IsChildOf(FMassFragment::StaticStruct()))
		{
			InOutComposition.Fragments.Add(*Type);
		}
		else if (Type->IsChildOf(FMassTag::StaticStruct()))
		{
			InOutComposition.Tags.Add(*Type);
		}
		else if (Type->IsChildOf(FMassChunkFragment::StaticStruct()))
		{
			InOutComposition.ChunkFragments.Add(*Type);
		}
		else
		{
			UE_LOG(LogMass, Warning, TEXT("%hs: %s is not a valid fragment nor tag type. Ignoring.")
				, __FUNCTION__, *GetNameSafe(Type));
		}
	}
}

FMassArchetypeHandle FMassEntityManager::GetArchetypeForEntity(FMassEntityHandle Entity) const
{
	if (IsEntityValid(Entity))
	{
		return FMassArchetypeHelper::ArchetypeHandleFromData(Entities[Entity.Index].CurrentArchetype);
	}
	return FMassArchetypeHandle();
}

FMassArchetypeHandle FMassEntityManager::GetArchetypeForEntityUnsafe(FMassEntityHandle Entity) const
{
	check(Entities.IsValidIndex(Entity.Index));
	return FMassArchetypeHelper::ArchetypeHandleFromData(Entities[Entity.Index].CurrentArchetype);
}

void FMassEntityManager::ForEachArchetypeFragmentType(const FMassArchetypeHandle& ArchetypeHandle, TFunction< void(const UScriptStruct* /*FragmentType*/)> Function)
{
	const FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	ArchetypeData.ForEachFragmentType(Function);
}

void FMassEntityManager::DoEntityCompaction(const double TimeAllowed)
{
	int32 TotalEntitiesMoved = 0;
	const double TimeAllowedEnd = FPlatformTime::Seconds() + TimeAllowed;

	bool bReachedTimeLimit = false;
	for (const auto& KVP : FragmentHashToArchetypeMap)
	{
		for (const TSharedPtr<FMassArchetypeData>& ArchetypePtr : KVP.Value)
		{
			const double TimeAllowedLeft = TimeAllowedEnd - FPlatformTime::Seconds();
			bReachedTimeLimit = TimeAllowedLeft <= 0.0;
			if (bReachedTimeLimit)
			{
 				break;
			}
			TotalEntitiesMoved += ArchetypePtr->CompactEntities(TimeAllowedLeft);
		}
		if (bReachedTimeLimit)
		{
			break;
		}
	}

	UE_CVLOG(TotalEntitiesMoved, GetOwner(), LogMass, Verbose, TEXT("Entity Compaction: moved %d entities"), TotalEntitiesMoved);
}

FMassEntityHandle FMassEntityManager::CreateEntity(const FMassArchetypeHandle& ArchetypeHandle, const FMassArchetypeSharedFragmentValues& SharedFragmentValues)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);
	check(ArchetypeHandle.IsValid());

	const FMassEntityHandle Entity = ReserveEntity();
	InternalBuildEntity(Entity, ArchetypeHandle, SharedFragmentValues);
	return Entity;
}

FMassEntityHandle FMassEntityManager::CreateEntity(TConstArrayView<FInstancedStruct> FragmentInstanceList, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FMassArchetypeCreationParams& CreationParams)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);
	check(FragmentInstanceList.Num() > 0);

	const FMassArchetypeHandle& ArchetypeHandle = CreateArchetype(FMassArchetypeCompositionDescriptor(FragmentInstanceList,
		FMassTagBitSet(), FMassChunkFragmentBitSet(), FMassSharedFragmentBitSet()), CreationParams);
	check(ArchetypeHandle.IsValid());

	const FMassEntityHandle Entity = ReserveEntity();
	InternalBuildEntity(Entity, ArchetypeHandle, SharedFragmentValues);

	const FEntityData& EntityData = Entities[Entity.Index];
	EntityData.CurrentArchetype->SetFragmentsData(Entity, FragmentInstanceList);

	return Entity;
}

FMassEntityHandle FMassEntityManager::ReserveEntity()
{
	// @todo: Need to add thread safety to the reservation of an entity
	FMassEntityHandle Result;
	Result.Index = (EntityFreeIndexList.Num() > 0) ? EntityFreeIndexList.Pop(EAllowShrinking::No) : Entities.Add();
	Result.SerialNumber = SerialNumberGenerator.fetch_add(1);
	Entities[Result.Index].SerialNumber = Result.SerialNumber;

	return Result;
}

void FMassEntityManager::ReleaseReservedEntity(FMassEntityHandle Entity)
{
	checkf(!IsEntityBuilt(Entity), TEXT("Entity is already built, use DestroyEntity() instead"));

	InternalReleaseEntity(Entity);
}

void FMassEntityManager::BuildEntity(FMassEntityHandle Entity, const FMassArchetypeHandle& ArchetypeHandle, const FMassArchetypeSharedFragmentValues& SharedFragmentValues)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);
	checkf(!IsEntityBuilt(Entity), TEXT("Expecting an entity that is not already built"));
	check(ArchetypeHandle.IsValid());

	InternalBuildEntity(Entity, ArchetypeHandle, SharedFragmentValues);
}

void FMassEntityManager::BuildEntity(FMassEntityHandle Entity, TConstArrayView<FInstancedStruct> FragmentInstanceList, const FMassArchetypeSharedFragmentValues& SharedFragmentValues)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);
	check(FragmentInstanceList.Num() > 0);
	checkf(!IsEntityBuilt(Entity), TEXT("Expecting an entity that is not already built"));

	checkf(SharedFragmentValues.IsSorted(), TEXT("Expecting shared fragment values to be previously sorted"));
	FMassArchetypeCompositionDescriptor Composition(FragmentInstanceList, FMassTagBitSet(), FMassChunkFragmentBitSet(), FMassSharedFragmentBitSet());
	for (const FConstSharedStruct& SharedFragment : SharedFragmentValues.GetConstSharedFragments())
	{
		Composition.SharedFragments.Add(*SharedFragment.GetScriptStruct());
	}
	for (const FSharedStruct& SharedFragment : SharedFragmentValues.GetSharedFragments())
	{
		Composition.SharedFragments.Add(*SharedFragment.GetScriptStruct());
	}

	const FMassArchetypeHandle& ArchetypeHandle = CreateArchetype(Composition);
	check(ArchetypeHandle.IsValid());

	InternalBuildEntity(Entity, ArchetypeHandle, SharedFragmentValues);

	const FEntityData& EntityData = Entities[Entity.Index];
	EntityData.CurrentArchetype->SetFragmentsData(Entity, FragmentInstanceList);
}

void FMassEntityManager::BatchBuildEntities(const FMassArchetypeEntityCollectionWithPayload& EncodedEntitiesWithPayload
	, const FMassFragmentBitSet& FragmentsAffected, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FMassArchetypeCreationParams& CreationParams)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);
	check(SharedFragmentValues.IsSorted());

	FMassArchetypeCompositionDescriptor Composition(FragmentsAffected, FMassTagBitSet(), FMassChunkFragmentBitSet(), FMassSharedFragmentBitSet());
	for (const FConstSharedStruct& SharedFragment : SharedFragmentValues.GetConstSharedFragments())
	{
		Composition.SharedFragments.Add(*SharedFragment.GetScriptStruct());
	}
	for (const FSharedStruct& SharedFragment : SharedFragmentValues.GetSharedFragments())
	{
		Composition.SharedFragments.Add(*SharedFragment.GetScriptStruct());
	}

	BatchBuildEntities(EncodedEntitiesWithPayload, MoveTemp(Composition), SharedFragmentValues, CreationParams);
}

void FMassEntityManager::BatchBuildEntities(const FMassArchetypeEntityCollectionWithPayload& EncodedEntitiesWithPayload, FMassArchetypeCompositionDescriptor&& Composition
	, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FMassArchetypeCreationParams& CreationParams)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);

	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchBuildEntities);

	FMassArchetypeEntityCollection::FEntityRangeArray TargetArchetypeEntityRanges;

	// "built" entities case, this is verified during FMassArchetypeEntityCollectionWithPayload construction
	FMassArchetypeHandle TargetArchetypeHandle = CreateArchetype(Composition, CreationParams);
	check(TargetArchetypeHandle.IsValid());

	// there are some extra steps in creating EncodedEntities from the original given entity handles and then back
	// to handles here, but this way we're consistent in how stuff is handled, and there are some slight benefits 
	// to having entities ordered by their index (like accessing the Entities data below).
	TArray<FMassEntityHandle> EntityHandles;
	UE::Mass::Private::ConvertArchetypelessSubchunksIntoEntityHandles(EncodedEntitiesWithPayload.GetEntityCollection().GetRanges(), EntityHandles);

	// since the handles encoded via FMassArchetypeEntityCollectionWithPayload miss the SerialNumber we need to update it
	// before passing over the the new archetype. Thankfully we need to iterate over all the entity handles anyway
	// to update the manager's information on these entities (stored in FMassEntityManager::Entities)
	for (FMassEntityHandle& Entity : EntityHandles)
	{
		check(Entities.IsValidIndex(Entity.Index));

		FEntityData& EntityData = Entities[Entity.Index];
		checkf(EntityData.SerialNumber != 0, TEXT("Failing this check means we're trying to build entities that have not been reserved yet. Consider using BatchCreateEntities"));
		Entity.SerialNumber = EntityData.SerialNumber;
		EntityData.CurrentArchetype = TargetArchetypeHandle.DataPtr;
	}

	TargetArchetypeHandle.DataPtr->BatchAddEntities(EntityHandles, SharedFragmentValues, TargetArchetypeEntityRanges);

	if (EncodedEntitiesWithPayload.GetPayload().IsEmpty() == false)
	{
		// at this point all the entities are in the target archetype, we can set the values
		// note that even though the "subchunk" information could have changed the order of entities is the same and 
		// corresponds to the order in FMassArchetypeEntityCollectionWithPayload's payload
		TargetArchetypeHandle.DataPtr->BatchSetFragmentValues(TargetArchetypeEntityRanges, EncodedEntitiesWithPayload.GetPayload());
	}

	if (ObserverManager.HasObserversForBitSet(Composition.Fragments, EMassObservedOperation::Add) || ObserverManager.HasObserversForBitSet(Composition.Tags, EMassObservedOperation::Add))
	{
		ObserverManager.OnCompositionChanged(
			FMassArchetypeEntityCollection(TargetArchetypeHandle, MoveTemp(TargetArchetypeEntityRanges))
			, Composition
			, EMassObservedOperation::Add);
	}
}

TSharedRef<FMassEntityManager::FEntityCreationContext> FMassEntityManager::BatchCreateReservedEntities(const FMassArchetypeHandle& ArchetypeHandle
	, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, TConstArrayView<FMassEntityHandle> ReservedEntities)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchCreateReservedEntities);

	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);
	checkf(!ReservedEntities.IsEmpty(), TEXT("No reserved entities given to batch create."));

	// verify that SharedFragmentValues contains all the data needed for the archetype indicated by ArchetypeHandle
	FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	if (!ensureMsgf(SharedFragmentValues.HasAllRequiredFragmentTypes(ArchetypeData.GetSharedFragmentBitSet())
		, TEXT("Trying to create entities with mismatching shared fragments collection. This would have lead to crashes, so we're bailing out. Make sure you pass in values for all the shared fragments declared in archetype's composition.")))
	{
		return MakeShareable(new FEntityCreationContext(0));
	}

	return InternalBatchCreateReservedEntities(ArchetypeHandle, SharedFragmentValues, ReservedEntities);
}

TSharedRef<FMassEntityManager::FEntityCreationContext> FMassEntityManager::BatchCreateEntities(const FMassArchetypeHandle& ArchetypeHandle
	, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const int32 Count, TArray<FMassEntityHandle>& OutEntities)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchCreateEntities);

	FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	if (!ensureMsgf(SharedFragmentValues.HasAllRequiredFragmentTypes(ArchetypeData.GetSharedFragmentBitSet())
		, TEXT("Trying to create entities with mismatching shared fragments collection. This would have lead to crashes, so we're bailing out. Make sure you pass in values for all the shared fragments declared in archetype's composition.")))
	{
		return MakeShareable(new FEntityCreationContext(0));
	}

	int32 Index = OutEntities.Num();
	OutEntities.Reserve(Index + Count);
	for (int32 Counter = 0; Counter < Count; ++Counter)
	{
		OutEntities.Add(ReserveEntity());
	}
	
	return InternalBatchCreateReservedEntities(ArchetypeHandle, SharedFragmentValues, MakeArrayView(OutEntities.GetData() + Index, Count));
}

TSharedRef<FMassEntityManager::FEntityCreationContext> FMassEntityManager::InternalBatchCreateReservedEntities(const FMassArchetypeHandle& ArchetypeHandle
	, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, TConstArrayView<FMassEntityHandle> ReservedEntities)
{
	// Functions calling into this one are required to verify that the archetype handle is valid
	FMassArchetypeData* ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandle(ArchetypeHandle);
	checkf(ArchetypeData, TEXT("Functions calling into this one are required to verify that the archetype handle is valid"));

	for (FMassEntityHandle Entity : ReservedEntities)
	{
		check(IsEntityValid(Entity));
		FEntityData& EntityData = Entities[Entity.Index];
		checkf(!EntityData.CurrentArchetype.IsValid(), TEXT("Batch creating reserved entities can only use entities that have not been constructed yet."));
		checkf(EntityData.SerialNumber != 0, TEXT("Failing this check means we're trying to build entities that have not been reserved yet"));
		EntityData.CurrentArchetype = ArchetypeHandle.DataPtr;
		EntityData.SerialNumber = Entity.SerialNumber;
	}

	FMassArchetypeEntityCollection::FEntityRangeArray TargetArchetypeEntityRanges;
	ArchetypeData->BatchAddEntities(ReservedEntities, SharedFragmentValues, TargetArchetypeEntityRanges);

	FEntityCreationContext* CreationContext = new FEntityCreationContext(ReservedEntities.Num());
	new (&CreationContext->EntityCollection) FMassArchetypeEntityCollection(ArchetypeHandle, MoveTemp(TargetArchetypeEntityRanges));

	if (ObserverManager.HasObserversForBitSet(ArchetypeData->GetCompositionDescriptor().Fragments, EMassObservedOperation::Add)
		|| ObserverManager.HasObserversForBitSet(ArchetypeData->GetCompositionDescriptor().Tags, EMassObservedOperation::Add))
	{
		CreationContext->OnSpawningFinished = [this](FEntityCreationContext& Context) {
			ObserverManager.OnPostEntitiesCreated(Context.EntityCollection);
		};
	}

	return MakeShareable(CreationContext);
}

void FMassEntityManager::DestroyEntity(FMassEntityHandle Entity)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);
	
	CheckIfEntityIsActive(Entity);

	const FEntityData& EntityData = Entities[Entity.Index];
	FMassArchetypeData* Archetype = EntityData.CurrentArchetype.Get();

	if (Archetype)
	{
		ObserverManager.OnPreEntityDestroyed(Archetype->GetCompositionDescriptor(), Entity);
		Archetype->RemoveEntity(Entity);
	}

	InternalReleaseEntity(Entity);
}

void FMassEntityManager::BatchDestroyEntities(TConstArrayView<FMassEntityHandle> InEntities)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchDestroyEntities);

	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);
	
	EntityFreeIndexList.Reserve(EntityFreeIndexList.Num() + InEntities.Num());

	// @todo optimize, we can make savings by implementing Archetype->RemoveEntities()
	for (const FMassEntityHandle Entity : InEntities)
	{
		if (Entities.IsValidIndex(Entity.Index) == false)
		{
			continue;
		}

		FEntityData& EntityData = Entities[Entity.Index];
		if (EntityData.SerialNumber != Entity.SerialNumber)
		{
			continue;
		}

		FMassArchetypeData* Archetype = EntityData.CurrentArchetype.Get();
		check(Archetype);
		ObserverManager.OnPreEntityDestroyed(Archetype->GetCompositionDescriptor(), Entity);
		Archetype->RemoveEntity(Entity);

		EntityData.Reset();
		EntityFreeIndexList.Add(Entity.Index);
	}
}

void FMassEntityManager::BatchDestroyEntityChunks(const FMassArchetypeEntityCollection& EntityCollection)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchDestroyEntityChunks);

	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);

	TArray<FMassEntityHandle> EntitiesRemoved;
	// note that it's important to place the context instance in the same scope as the loop below that updates 
	// FMassEntityManager.EntityData, otherwise, if there are commands flushed as part of FMassProcessingContext's 
	// destruction the commands will work on outdated information (which might result in crashes).
	FMassProcessingContext ProcessingContext(*this, /*TimeDelta=*/0.0f);

	bool bValidArchetype = EntityCollection.GetArchetype().IsValid();
	if (bValidArchetype)
	{
		ProcessingContext.bFlushCommandBuffer = false;
		ProcessingContext.CommandBuffer = MakeShareable(new FMassCommandBuffer());
		ObserverManager.OnPreEntitiesDestroyed(ProcessingContext, EntityCollection);

		FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(EntityCollection.GetArchetype());
		ArchetypeData.BatchDestroyEntityChunks(EntityCollection.GetRanges(), EntitiesRemoved);

		EntityFreeIndexList.Reserve(EntityFreeIndexList.Num() + EntitiesRemoved.Num());
	}
	else
	{
		UE::Mass::Private::ConvertArchetypelessSubchunksIntoEntityHandles(EntityCollection.GetRanges(), EntitiesRemoved);
	}

	for (const FMassEntityHandle& Entity : EntitiesRemoved)
	{
		check(Entities.IsValidIndex(Entity.Index));

		FEntityData& EntityData = Entities[Entity.Index];
		if (!bValidArchetype || EntityData.SerialNumber == Entity.SerialNumber)
		{
			EntityData.Reset();
			EntityFreeIndexList.Add(Entity.Index);
		}
	}
}

void FMassEntityManager::AddFragmentToEntity(FMassEntityHandle Entity, const UScriptStruct* FragmentType)
{
	checkf(FragmentType, TEXT("Null fragment type passed in to %hs"), __FUNCTION__);
	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);

	CheckIfEntityIsActive(Entity);

	const FMassArchetypeCompositionDescriptor Descriptor(InternalAddFragmentListToEntityChecked(Entity, FMassFragmentBitSet(*FragmentType)));
	ObserverManager.OnPostCompositionAdded(Entity, Descriptor);
}

void FMassEntityManager::AddFragmentToEntity(FMassEntityHandle Entity, const UScriptStruct* FragmentType, const FStructInitializationCallback& Initializer)
{
	checkf(FragmentType, TEXT("Null fragment type passed in to %hs"), __FUNCTION__);
	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);

	CheckIfEntityIsActive(Entity);

	FMassFragmentBitSet Fragments = InternalAddFragmentListToEntityChecked(Entity, FMassFragmentBitSet(*FragmentType));
	FEntityData& EntityData = Entities[Entity.Index];
	void* FragmentData = EntityData.CurrentArchetype->GetFragmentDataForEntity(FragmentType, Entity.Index);
	Initializer(FragmentData, *FragmentType);

	const FMassArchetypeCompositionDescriptor Descriptor(MoveTemp(Fragments));
	ObserverManager.OnPostCompositionAdded(Entity, Descriptor);
}

void FMassEntityManager::AddFragmentListToEntity(FMassEntityHandle Entity, TConstArrayView<const UScriptStruct*> FragmentList)
{
	CheckIfEntityIsActive(Entity);

	const FMassArchetypeCompositionDescriptor Descriptor(InternalAddFragmentListToEntityChecked(Entity, FMassFragmentBitSet(FragmentList)));
	ObserverManager.OnPostCompositionAdded(Entity, Descriptor);
}

void FMassEntityManager::AddCompositionToEntity_GetDelta(FMassEntityHandle Entity, FMassArchetypeCompositionDescriptor& InDescriptor)
{
	CheckIfEntityIsActive(Entity);

	FEntityData& EntityData = Entities[Entity.Index];
	FMassArchetypeData* OldArchetype = EntityData.CurrentArchetype.Get();
	check(OldArchetype);

	InDescriptor.Fragments -= OldArchetype->GetCompositionDescriptor().Fragments;
	InDescriptor.Tags -= OldArchetype->GetCompositionDescriptor().Tags;

	ensureMsgf(InDescriptor.ChunkFragments.IsEmpty(), TEXT("Adding new chunk fragments is not supported"));

	if (InDescriptor.IsEmpty() == false)
	{
		FMassArchetypeCompositionDescriptor NewDescriptor = OldArchetype->GetCompositionDescriptor();
		NewDescriptor.Fragments += InDescriptor.Fragments;
		NewDescriptor.Tags += InDescriptor.Tags;

		const FMassArchetypeHandle NewArchetypeHandle = CreateArchetype(NewDescriptor);

		if (ensure(NewArchetypeHandle.DataPtr != EntityData.CurrentArchetype))
		{
			// Move the entity over
			FMassArchetypeData& NewArchetype = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(NewArchetypeHandle);
			NewArchetype.CopyDebugNamesFrom(*OldArchetype);
			EntityData.CurrentArchetype->MoveEntityToAnotherArchetype(Entity, NewArchetype);
			EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;

			ObserverManager.OnPostCompositionAdded(Entity, InDescriptor);
		}
	}
}

void FMassEntityManager::RemoveCompositionFromEntity(FMassEntityHandle Entity, const FMassArchetypeCompositionDescriptor& InDescriptor)
{
	CheckIfEntityIsActive(Entity);

	if(InDescriptor.IsEmpty() == false)
	{
		FEntityData& EntityData = Entities[Entity.Index];
		FMassArchetypeData* OldArchetype = EntityData.CurrentArchetype.Get();
		check(OldArchetype);

		FMassArchetypeCompositionDescriptor NewDescriptor = OldArchetype->GetCompositionDescriptor();
		NewDescriptor.Fragments -= InDescriptor.Fragments;
		NewDescriptor.Tags -= InDescriptor.Tags;

		ensureMsgf(InDescriptor.ChunkFragments.IsEmpty(), TEXT("Removing chunk fragments is not supported"));
		ensureMsgf(InDescriptor.SharedFragments.IsEmpty(), TEXT("Removing shared fragments is not supported"));

		if (NewDescriptor.IsEquivalent(OldArchetype->GetCompositionDescriptor()) == false)
		{
			ensureMsgf(OldArchetype->GetCompositionDescriptor().HasAll(InDescriptor), TEXT("Some of the elements being removed are already missing from entity\'s composition."));
			ObserverManager.OnPreCompositionRemoved(Entity, InDescriptor);

			const FMassArchetypeHandle NewArchetypeHandle = CreateArchetype(NewDescriptor);

			if (ensure(NewArchetypeHandle.DataPtr != EntityData.CurrentArchetype))
			{
				// Move the entity over
				FMassArchetypeData& NewArchetype = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(NewArchetypeHandle);
				NewArchetype.CopyDebugNamesFrom(*OldArchetype);
				EntityData.CurrentArchetype->MoveEntityToAnotherArchetype(Entity, NewArchetype);
				EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;
			}
		}
	}
}

const FMassArchetypeCompositionDescriptor& FMassEntityManager::GetArchetypeComposition(const FMassArchetypeHandle& ArchetypeHandle) const
{
	const FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	return ArchetypeData.GetCompositionDescriptor();
}

void FMassEntityManager::InternalBuildEntity(FMassEntityHandle Entity, const FMassArchetypeHandle& ArchetypeHandle, const FMassArchetypeSharedFragmentValues& SharedFragmentValues)
{
	FEntityData& EntityData = Entities[Entity.Index];
	EntityData.CurrentArchetype = ArchetypeHandle.DataPtr;
	EntityData.CurrentArchetype->AddEntity(Entity, SharedFragmentValues);

	ObserverManager.OnPostCompositionAdded(Entity, EntityData.CurrentArchetype->GetCompositionDescriptor());
}

void FMassEntityManager::InternalReleaseEntity(FMassEntityHandle Entity)
{
	FEntityData& EntityData = Entities[Entity.Index];
	EntityData.Reset();
	EntityFreeIndexList.Add(Entity.Index);
}

FMassFragmentBitSet FMassEntityManager::InternalAddFragmentListToEntityChecked(FMassEntityHandle Entity, const FMassFragmentBitSet& InFragments)
{
	const FEntityData& EntityData = Entities[Entity.Index];
	FMassArchetypeData* OldArchetype = EntityData.CurrentArchetype.Get();
	check(OldArchetype);

	UE_CLOG(OldArchetype->GetFragmentBitSet().HasAny(InFragments), LogMass, Log
		, TEXT("Trying to add a new fragment type to an entity, but it already has some of them. (%s)")
		, *InFragments.GetOverlap(OldArchetype->GetFragmentBitSet()).DebugGetStringDesc());

	FMassFragmentBitSet NewFragments = InFragments - OldArchetype->GetFragmentBitSet();
	if (NewFragments.IsEmpty() == false)
	{
		InternalAddFragmentListToEntity(Entity, NewFragments);
	}
	return MoveTemp(NewFragments);
}

void FMassEntityManager::InternalAddFragmentListToEntity(FMassEntityHandle Entity, const FMassFragmentBitSet& InFragments)
{
	checkf(InFragments.IsEmpty() == false, TEXT("%hs is intended for internal calls with non empty NewFragments parameter"), __FUNCTION__);
	check(Entities.IsValidIndex(Entity.Index));
	FEntityData& EntityData = Entities[Entity.Index];
	check(EntityData.CurrentArchetype.IsValid());
	const FMassArchetypeData* OldArchetype = EntityData.CurrentArchetype.Get();

	// fetch or create the new archetype
	const FMassArchetypeHandle NewArchetypeHandle = CreateArchetype(EntityData.CurrentArchetype, InFragments);
	checkf(NewArchetypeHandle.DataPtr != EntityData.CurrentArchetype, TEXT("%hs is intended for internal calls with non overlapping fragment list."), __FUNCTION__);

	// Move the entity over
	FMassArchetypeData& NewArchetype = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(NewArchetypeHandle);
	NewArchetype.CopyDebugNamesFrom(*OldArchetype);
	EntityData.CurrentArchetype->MoveEntityToAnotherArchetype(Entity, NewArchetype);
	EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;
}

void FMassEntityManager::AddFragmentInstanceListToEntity(FMassEntityHandle Entity, TConstArrayView<FInstancedStruct> FragmentInstanceList)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);

	CheckIfEntityIsActive(Entity);
	checkf(FragmentInstanceList.Num() > 0, TEXT("Need to specify at least one fragment instances for this operation"));

	const FMassArchetypeCompositionDescriptor Descriptor(InternalAddFragmentListToEntityChecked(Entity, FMassFragmentBitSet(FragmentInstanceList)));

	const FEntityData& EntityData = Entities[Entity.Index];
	EntityData.CurrentArchetype->SetFragmentsData(Entity, FragmentInstanceList);

	ObserverManager.OnPostCompositionAdded(Entity, Descriptor);
}

void FMassEntityManager::RemoveFragmentFromEntity(FMassEntityHandle Entity, const UScriptStruct* FragmentType)
{
	RemoveFragmentListFromEntity(Entity, MakeArrayView(&FragmentType, 1));
}

void FMassEntityManager::RemoveFragmentListFromEntity(FMassEntityHandle Entity, TConstArrayView<const UScriptStruct*> FragmentList)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);

	CheckIfEntityIsActive(Entity);
	
	FEntityData& EntityData = Entities[Entity.Index];
	FMassArchetypeData* OldArchetype = EntityData.CurrentArchetype.Get();
	check(OldArchetype);

	const FMassFragmentBitSet FragmentsToRemove(FragmentList);

	if (OldArchetype->GetFragmentBitSet().HasAny(FragmentsToRemove))
	{
		// If all the fragments got removed this will result in fetching of the empty archetype
		const FMassArchetypeCompositionDescriptor NewComposition(OldArchetype->GetFragmentBitSet() - FragmentsToRemove, OldArchetype->GetTagBitSet(), OldArchetype->GetChunkFragmentBitSet(), OldArchetype->GetSharedFragmentBitSet());
		const FMassArchetypeHandle NewArchetypeHandle = CreateArchetype(NewComposition);

		FMassArchetypeCompositionDescriptor CompositionDelta;
		// Find overlap.  It isn't guaranteed that the old archtype has all of the fragments being removed.
		CompositionDelta.Fragments = OldArchetype->GetFragmentBitSet().GetOverlap(FragmentsToRemove);
		ObserverManager.OnPreCompositionRemoved(Entity, CompositionDelta);

		// Move the entity over
		FMassArchetypeData& NewArchetype = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(NewArchetypeHandle);
		NewArchetype.CopyDebugNamesFrom(*OldArchetype);
		OldArchetype->MoveEntityToAnotherArchetype(Entity, NewArchetype);
		EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;
	}
}

void FMassEntityManager::SwapTagsForEntity(FMassEntityHandle Entity, const UScriptStruct* OldTagType, const UScriptStruct* NewTagType)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %hs called during mass processing. Use asynchronous API instead."), __FUNCTION__);

	CheckIfEntityIsActive(Entity);

	checkf((OldTagType != nullptr) && OldTagType->IsChildOf(FMassTag::StaticStruct()), TEXT("%hs works only with tags while '%s' is not one."), __FUNCTION__, *GetPathNameSafe(OldTagType));
	checkf((NewTagType != nullptr) && NewTagType->IsChildOf(FMassTag::StaticStruct()), TEXT("%hs works only with tags while '%s' is not one."), __FUNCTION__, *GetPathNameSafe(NewTagType));

	FEntityData& EntityData = Entities[Entity.Index];
	FMassArchetypeData* CurrentArchetype = EntityData.CurrentArchetype.Get();
	check(CurrentArchetype);

	FMassTagBitSet NewTagBitSet = CurrentArchetype->GetTagBitSet();
	NewTagBitSet.Remove(*OldTagType);
	NewTagBitSet.Add(*NewTagType);
	
	if (NewTagBitSet != CurrentArchetype->GetTagBitSet())
	{
		const FMassArchetypeHandle NewArchetypeHandle = InternalCreateSimilarArchetype(EntityData.CurrentArchetype, NewTagBitSet);
		checkSlow(NewArchetypeHandle.IsValid());

		// Move the entity over
		EntityData.CurrentArchetype->MoveEntityToAnotherArchetype(Entity, *NewArchetypeHandle.DataPtr.Get());
		EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;
	}
}

void FMassEntityManager::AddTagToEntity(FMassEntityHandle Entity, const UScriptStruct* TagType)
{
	checkf((TagType != nullptr) && TagType->IsChildOf(FMassTag::StaticStruct()), TEXT("%hs works only with tags while '%s' is not one."), __FUNCTION__, *GetPathNameSafe(TagType));

	CheckIfEntityIsActive(Entity);

	FEntityData& EntityData = Entities[Entity.Index];
	FMassArchetypeData* CurrentArchetype = EntityData.CurrentArchetype.Get();
	check(CurrentArchetype);

	if (CurrentArchetype->HasTagType(TagType) == false)
	{
		//FMassTagBitSet NewTags = CurrentArchetype->GetTagBitSet() - *TagType;
		FMassTagBitSet NewTags = CurrentArchetype->GetTagBitSet();
		NewTags.Add(*TagType);
		const FMassArchetypeHandle NewArchetypeHandle = InternalCreateSimilarArchetype(EntityData.CurrentArchetype, NewTags);
		checkSlow(NewArchetypeHandle.IsValid());

		// Move the entity over
		EntityData.CurrentArchetype->MoveEntityToAnotherArchetype(Entity, *NewArchetypeHandle.DataPtr.Get());
		EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;

		FMassArchetypeCompositionDescriptor CompositionDelta;
		FMassTagBitSet TagDelta;
		TagDelta.Add(*TagType);
		CompositionDelta.Tags = TagDelta;
		ObserverManager.OnPostCompositionAdded(Entity, CompositionDelta);
	}
}
	
void FMassEntityManager::RemoveTagFromEntity(FMassEntityHandle Entity, const UScriptStruct* TagType)
{
	checkf((TagType != nullptr) && TagType->IsChildOf(FMassTag::StaticStruct()), TEXT("%hs works only with tags while '%s' is not one."), __FUNCTION__, *GetPathNameSafe(TagType));

	CheckIfEntityIsActive(Entity);

	FEntityData& EntityData = Entities[Entity.Index];
	FMassArchetypeData* CurrentArchetype = EntityData.CurrentArchetype.Get();
	check(CurrentArchetype);

	if (CurrentArchetype->HasTagType(TagType))
	{
		FMassArchetypeCompositionDescriptor CompositionDelta;
		FMassTagBitSet TagDelta;
		TagDelta.Add(*TagType);
		CompositionDelta.Tags = TagDelta;
		ObserverManager.OnPreCompositionRemoved(Entity, CompositionDelta);
		
		// CurrentArchetype->GetTagBitSet() -  *TagType
		const FMassTagBitSet NewTagComposition = CurrentArchetype->GetTagBitSet() - TagDelta;
		const FMassArchetypeHandle NewArchetypeHandle = InternalCreateSimilarArchetype(EntityData.CurrentArchetype, NewTagComposition);
		checkSlow(NewArchetypeHandle.IsValid());

		// Move the entity over
		EntityData.CurrentArchetype->MoveEntityToAnotherArchetype(Entity, *NewArchetypeHandle.DataPtr.Get());
		EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;
	}
}

void FMassEntityManager::BatchChangeTagsForEntities(TConstArrayView<FMassArchetypeEntityCollection> EntityCollections, const FMassTagBitSet& TagsToAdd, const FMassTagBitSet& TagsToRemove)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchChangeTagsForEntities);

	for (const FMassArchetypeEntityCollection& Collection : EntityCollections)
	{
		FMassArchetypeData* CurrentArchetype = Collection.GetArchetype().DataPtr.Get();
		const FMassTagBitSet NewTagComposition = CurrentArchetype
			? (CurrentArchetype->GetTagBitSet() + TagsToAdd - TagsToRemove)
			: (TagsToAdd - TagsToRemove);

		if (ensure(CurrentArchetype) && CurrentArchetype->GetTagBitSet() != NewTagComposition)
		{
			FMassTagBitSet TagsAdded = TagsToAdd - CurrentArchetype->GetTagBitSet();
			FMassTagBitSet TagsRemoved = TagsToRemove.GetOverlap(CurrentArchetype->GetTagBitSet());

			if (ObserverManager.HasObserversForBitSet(TagsRemoved, EMassObservedOperation::Remove))
			{
				ObserverManager.OnCompositionChanged(Collection, FMassArchetypeCompositionDescriptor(MoveTemp(TagsRemoved)), EMassObservedOperation::Remove);
			}
			const bool bTagsAddedAreObserved = ObserverManager.HasObserversForBitSet(TagsAdded, EMassObservedOperation::Add);

			FMassArchetypeHandle NewArchetypeHandle = InternalCreateSimilarArchetype(Collection.GetArchetype().DataPtr, NewTagComposition);
			checkSlow(NewArchetypeHandle.IsValid());

			// Move the entity over
			FMassArchetypeEntityCollection::FEntityRangeArray NewArchetypeEntityRanges;
			TArray<FMassEntityHandle> EntitiesBeingMoved;
			CurrentArchetype->BatchMoveEntitiesToAnotherArchetype(Collection, *NewArchetypeHandle.DataPtr.Get(), EntitiesBeingMoved
				, bTagsAddedAreObserved ? &NewArchetypeEntityRanges : nullptr);

			for (const FMassEntityHandle& Entity : EntitiesBeingMoved)
			{
				check(Entities.IsValidIndex(Entity.Index));

				FEntityData& EntityData = Entities[Entity.Index];
				EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;
			}

			if (bTagsAddedAreObserved)
			{
				ObserverManager.OnCompositionChanged(
					FMassArchetypeEntityCollection(NewArchetypeHandle, MoveTemp(NewArchetypeEntityRanges))
					, FMassArchetypeCompositionDescriptor(MoveTemp(TagsAdded))
					, EMassObservedOperation::Add);
			}
		}
	}
}

void FMassEntityManager::BatchChangeFragmentCompositionForEntities(TConstArrayView<FMassArchetypeEntityCollection> EntityCollections, const FMassFragmentBitSet& FragmentsToAdd, const FMassFragmentBitSet& FragmentsToRemove)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchChangeFragmentCompositionForEntities);

	for (const FMassArchetypeEntityCollection& Collection : EntityCollections)
	{
		FMassArchetypeData* CurrentArchetype = Collection.GetArchetype().DataPtr.Get();
		const FMassFragmentBitSet NewFragmentComposition = CurrentArchetype
			? (CurrentArchetype->GetFragmentBitSet() + FragmentsToAdd - FragmentsToRemove)
			: (FragmentsToAdd - FragmentsToRemove);

		if (CurrentArchetype)
		{
			if (CurrentArchetype->GetFragmentBitSet() != NewFragmentComposition)
			{
				FMassFragmentBitSet FragmentsAdded = FragmentsToAdd - CurrentArchetype->GetFragmentBitSet();
				const bool bFragmentsAddedAreObserved = ObserverManager.HasObserversForBitSet(FragmentsAdded, EMassObservedOperation::Add);
				FMassFragmentBitSet FragmentsRemoved = FragmentsToRemove.GetOverlap(CurrentArchetype->GetFragmentBitSet());
				if (ObserverManager.HasObserversForBitSet(FragmentsRemoved, EMassObservedOperation::Remove))
				{
					ObserverManager.OnCompositionChanged(Collection, FMassArchetypeCompositionDescriptor(MoveTemp(FragmentsRemoved)), EMassObservedOperation::Remove);
				}

				FMassArchetypeHandle NewArchetypeHandle = InternalCreateSimilarArchetype(Collection.GetArchetype().DataPtr, NewFragmentComposition);
				checkSlow(NewArchetypeHandle.IsValid());

				// Move the entity over
				FMassArchetypeEntityCollection::FEntityRangeArray NewArchetypeEntityRanges;
				TArray<FMassEntityHandle> EntitiesBeingMoved;
				CurrentArchetype->BatchMoveEntitiesToAnotherArchetype(Collection, *NewArchetypeHandle.DataPtr.Get(), EntitiesBeingMoved
					, bFragmentsAddedAreObserved ? &NewArchetypeEntityRanges : nullptr);

				for (const FMassEntityHandle& Entity : EntitiesBeingMoved)
				{
					check(Entities.IsValidIndex(Entity.Index));

					FEntityData& EntityData = Entities[Entity.Index];
					EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;
				}

				if (bFragmentsAddedAreObserved)
				{
					ObserverManager.OnCompositionChanged(
						FMassArchetypeEntityCollection(NewArchetypeHandle, MoveTemp(NewArchetypeEntityRanges))
						, FMassArchetypeCompositionDescriptor(MoveTemp(FragmentsAdded))
						, EMassObservedOperation::Add);
				}
			}
		}
		else
		{
			BatchBuildEntities(FMassArchetypeEntityCollectionWithPayload(Collection), NewFragmentComposition, FMassArchetypeSharedFragmentValues());
		}
	}
}

void FMassEntityManager::BatchAddFragmentInstancesForEntities(TConstArrayView<FMassArchetypeEntityCollectionWithPayload> EntityCollections, const FMassFragmentBitSet& FragmentsAffected)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchAddFragmentInstancesForEntities);

	// here's the scenario:
	// * we get entities from potentially different archetypes
	// * adding a fragment instance consists of two operations: A) add fragment type & B) set fragment value
	//		* some archetypes might already have the "added" fragments so no need for step A
	//		* there might be an "empty" archetype in the mix - then step A results in archetype creation and assigning
	//		* if step A is required then the initial FMassArchetypeEntityCollection instance is no longer valid
	// * setting value can be done uniformly for all entities, remembering some might be in different chunks already
	// * @todo note that after adding fragment type some entities originally in different archetypes end up in the same 
	//		archetype. This could be utilized as a basis for optimization. To be investigated.
	// 

	for (const FMassArchetypeEntityCollectionWithPayload& EntityRangesWithPayload : EntityCollections)
	{
		FMassArchetypeHandle TargetArchetypeHandle = EntityRangesWithPayload.GetEntityCollection().GetArchetype();
		FMassArchetypeData* CurrentArchetype = TargetArchetypeHandle.DataPtr.Get();

		if (CurrentArchetype)
		{
			FMassArchetypeEntityCollection::FEntityRangeArray TargetArchetypeEntityRanges;
			bool bFragmentsAddedAreObserved = false;
			FMassFragmentBitSet NewFragmentComposition = CurrentArchetype
				? (CurrentArchetype->GetFragmentBitSet() + FragmentsAffected)
				: FragmentsAffected;
			FMassFragmentBitSet FragmentsAdded;

			if (CurrentArchetype->GetFragmentBitSet() != NewFragmentComposition)
			{
				FragmentsAdded = FragmentsAffected - CurrentArchetype->GetFragmentBitSet();
				bFragmentsAddedAreObserved = ObserverManager.HasObserversForBitSet(FragmentsAdded, EMassObservedOperation::Add);

				FMassArchetypeHandle NewArchetypeHandle = InternalCreateSimilarArchetype(TargetArchetypeHandle.DataPtr, NewFragmentComposition);
				checkSlow(NewArchetypeHandle.IsValid());

				// Move the entity over
				TArray<FMassEntityHandle> EntitiesBeingMoved;
				CurrentArchetype->BatchMoveEntitiesToAnotherArchetype(EntityRangesWithPayload.GetEntityCollection(), *NewArchetypeHandle.DataPtr.Get()
					, EntitiesBeingMoved, &TargetArchetypeEntityRanges);

				for (const FMassEntityHandle& Entity : EntitiesBeingMoved)
				{
					check(Entities.IsValidIndex(Entity.Index));

					FEntityData& EntityData = Entities[Entity.Index];
					EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;
				}

				TargetArchetypeHandle = NewArchetypeHandle;
			}
			else
			{
				TargetArchetypeEntityRanges = EntityRangesWithPayload.GetEntityCollection().GetRanges();
			}

			// at this point all the entities are in the target archetype, we can set the values
			// note that even though the "subchunk" information could have changed the order of entities is the same and 
			// corresponds to the order in FMassArchetypeEntityCollectionWithPayload's payload
			TargetArchetypeHandle.DataPtr->BatchSetFragmentValues(TargetArchetypeEntityRanges, EntityRangesWithPayload.GetPayload());

			if (bFragmentsAddedAreObserved)
			{
				ObserverManager.OnCompositionChanged(
					FMassArchetypeEntityCollection(TargetArchetypeHandle, MoveTemp(TargetArchetypeEntityRanges))
					, FMassArchetypeCompositionDescriptor(MoveTemp(FragmentsAdded))
					, EMassObservedOperation::Add);
			}
		}
		else 
		{
			BatchBuildEntities(EntityRangesWithPayload, FragmentsAffected, FMassArchetypeSharedFragmentValues());
		}
	}
}

void FMassEntityManager::MoveEntityToAnotherArchetype(FMassEntityHandle Entity, FMassArchetypeHandle NewArchetypeHandle)
{
	CheckIfEntityIsActive(Entity);

	FMassArchetypeData& NewArchetype = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(NewArchetypeHandle);

	// Move the entity over
	FEntityData& EntityData = Entities[Entity.Index];
	EntityData.CurrentArchetype->MoveEntityToAnotherArchetype(Entity, NewArchetype);
	EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;
}

void FMassEntityManager::SetEntityFragmentsValues(FMassEntityHandle Entity, TArrayView<const FInstancedStruct> FragmentInstanceList)
{
	CheckIfEntityIsActive(Entity);

	const FEntityData& EntityData = Entities[Entity.Index];
	EntityData.CurrentArchetype->SetFragmentsData(Entity, FragmentInstanceList);
}

void FMassEntityManager::BatchSetEntityFragmentsValues(const FMassArchetypeEntityCollection& SparseEntities, TArrayView<const FInstancedStruct> FragmentInstanceList)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchSetEntityFragmentsValues);

	FMassArchetypeData* Archetype = SparseEntities.GetArchetype().DataPtr.Get();
	check(Archetype);

	for (const FInstancedStruct& FragmentTemplate : FragmentInstanceList)
	{
		Archetype->SetFragmentData(SparseEntities.GetRanges(), FragmentTemplate);
	}
}

void* FMassEntityManager::InternalGetFragmentDataChecked(FMassEntityHandle Entity, const UScriptStruct* FragmentType) const
{
	// note that FragmentType is guaranteed to be of valid type - it's either statically checked by the template versions
	// or `checkf`ed by the non-template one
	CheckIfEntityIsActive(Entity);
	const FEntityData& EntityData = Entities[Entity.Index];
	return EntityData.CurrentArchetype->GetFragmentDataForEntityChecked(FragmentType, Entity.Index);
}

void* FMassEntityManager::InternalGetFragmentDataPtr(FMassEntityHandle Entity, const UScriptStruct* FragmentType) const
{
	// note that FragmentType is guaranteed to be of valid type - it's either statically checked by the template versions
	// or `checkf`ed by the non-template one
	CheckIfEntityIsActive(Entity);
	const FEntityData& EntityData = Entities[Entity.Index];
	return EntityData.CurrentArchetype->GetFragmentDataForEntity(FragmentType, Entity.Index);
}

const FConstSharedStruct* FMassEntityManager::InternalGetConstSharedFragmentPtr(FMassEntityHandle Entity, const UScriptStruct* ConstSharedFragmentType) const
{
	// note that ConstSharedFragmentType is guaranteed to be of valid type - it's either statically checked by the template versions
	// or `checkf`ed by the non-template one
	CheckIfEntityIsActive(Entity);
	const FEntityData& EntityData = Entities[Entity.Index];
	const FConstSharedStruct* SharedFragment = EntityData.CurrentArchetype->GetSharedFragmentValues(Entity).GetConstSharedFragments().FindByPredicate(FStructTypeEqualOperator(ConstSharedFragmentType));
	return SharedFragment;
}

const FSharedStruct* FMassEntityManager::InternalGetSharedFragmentPtr(FMassEntityHandle Entity, const UScriptStruct* SharedFragmentType) const
{
	// note that SharedFragmentType is guaranteed to be of valid type - it's either statically checked by the template versions
	// or `checkf`ed by the non-template one
	CheckIfEntityIsActive(Entity);
	const FEntityData& EntityData = Entities[Entity.Index];
	const FSharedStruct* SharedFragment = EntityData.CurrentArchetype->GetSharedFragmentValues(Entity).GetSharedFragments().FindByPredicate(FStructTypeEqualOperator(SharedFragmentType));
	return SharedFragment;
}

bool FMassEntityManager::IsEntityValid(FMassEntityHandle Entity) const
{
	return (Entity.Index > 0) && Entities.IsValidIndex(Entity.Index) && (Entities[Entity.Index].SerialNumber == Entity.SerialNumber);
}

bool FMassEntityManager::IsEntityBuilt(FMassEntityHandle Entity) const
{
	CheckIfEntityIsValid(Entity);
	return Entities[Entity.Index].CurrentArchetype.IsValid();
}

void FMassEntityManager::CheckIfEntityIsValid(FMassEntityHandle Entity) const
{
	checkf(IsEntityValid(Entity), TEXT("Invalid entity (ID: %d, SN:%d, %s)"), Entity.Index, Entity.SerialNumber,
		   (Entity.Index == 0) ? TEXT("was never initialized") : TEXT("already destroyed"));
}

void FMassEntityManager::CheckIfEntityIsActive(FMassEntityHandle Entity) const
{
	checkf(IsEntityBuilt(Entity), TEXT("Entity not yet created(ID: %d, SN:%d)"), Entity.Index, Entity.SerialNumber);
}

void FMassEntityManager::GetMatchingArchetypes(const FMassFragmentRequirements& Requirements, TArray<FMassArchetypeHandle>& OutValidArchetypes, const uint32 FromArchetypeDataVersion) const
{
	//@TODO: Not optimized yet, but we call this rarely now, so not a big deal.

	// First get set of all archetypes that contain *any* fragment
	TSet<TSharedPtr<FMassArchetypeData>> AnyArchetypes;
	TConstArrayView<FMassFragmentRequirementDescription> FragmentRequirements = Requirements.GetFragmentRequirements();

	// We need to find out if the query does indeed strictly require any fragments to be there - it's for example 
	// possible to build a query consisting of only optional fragment requirements. In that case we need to test for 
	// tags as well, independently of fragments, because none of the fragments might be there and we still need to 
	// respect the tags.
	// A practical example would be a query asking for all entities that have a given fragment OR a given tag.
	bool bHasHardFragmentdRequirements = false;

	if (!FragmentRequirements.IsEmpty())
	{
		for (const FMassFragmentRequirementDescription& Requirement : FragmentRequirements)
		{
			check(Requirement.StructType);
			if (Requirement.Presence != EMassFragmentPresence::None)
			{
				if (const TArray<TSharedPtr<FMassArchetypeData>>* pData = FragmentTypeToArchetypeMap.Find(Requirement.StructType))
				{
					AnyArchetypes.Append(*pData);
					// only Any and All count as a "hard requirement" since other modes don't strictly require a fragment to be there
					bHasHardFragmentdRequirements = bHasHardFragmentdRequirements
						|| Requirement.Presence == EMassFragmentPresence::All
						|| Requirement.Presence == EMassFragmentPresence::Any;
				}
			}
		}
	}
	
	// If the query does not have any hard requirements for fragments, but has tag requirements.
	// We need to search all archetypes, because we don't have a Tag->Archetype Map (and we don't want to create one 
	// just for this one edge case) so we need to go through all archetypes and match tags. On the bright side,
	// we can ignore all archetypes with data version < FromArchetypeDataVersion
	// Note that we collect all archetypes even remotely matching to let the regular process below to filter out
	// the failing archetypes while justifying the reasons. 
	if (bHasHardFragmentdRequirements == false
		&& (Requirements.GetRequiredAllTags().IsEmpty() == false || Requirements.GetRequiredAnyTags().IsEmpty() == false))
	{
		const FMassTagBitSet RoughTagFilter = Requirements.GetRequiredAllTags() | Requirements.GetRequiredAnyTags();
		for (int32 ArchetypeIndex = FromArchetypeDataVersion; ArchetypeIndex < AllArchetypes.Num(); ++ArchetypeIndex)
		{
			const TSharedPtr<FMassArchetypeData>& ArchetypeData = AllArchetypes[ArchetypeIndex];
			if (ArchetypeData.IsValid())
			{
				if (ArchetypeData->GetTagBitSet().HasAny(RoughTagFilter))
				{
					AnyArchetypes.Add(ArchetypeData);
				}
			}
		}
	}

	// Then verify that they contain *all* required fragments
	for (TSharedPtr<FMassArchetypeData>& ArchetypePtr : AnyArchetypes)
	{
		FMassArchetypeData& Archetype = *(ArchetypePtr.Get());

		// Only return archetypes with a newer created version than the specified version, this is for incremental query updates
		if (Archetype.GetCreatedArchetypeDataVersion() <= FromArchetypeDataVersion)
		{
			continue;
		}

		if (Archetype.GetTagBitSet().HasAll(Requirements.GetRequiredAllTags()) == false)
		{
			// missing some required tags, skip.
#if WITH_MASSENTITY_DEBUG
			const FMassTagBitSet UnsatisfiedTags = Requirements.GetRequiredAllTags() - Archetype.GetTagBitSet();
			FStringOutputDevice Description;
			UnsatisfiedTags.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype did not match due to missing tags: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Archetype.GetTagBitSet().HasNone(Requirements.GetRequiredNoneTags()) == false)
		{
			// has some tags required to be absent
#if WITH_MASSENTITY_DEBUG
			const FMassTagBitSet UnwantedTags = Requirements.GetRequiredAllTags().GetOverlap(Archetype.GetTagBitSet());
			FStringOutputDevice Description;
			UnwantedTags.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype has tags required absent: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Requirements.GetRequiredAnyTags().IsEmpty() == false 
			&& Archetype.GetTagBitSet().HasAny(Requirements.GetRequiredAnyTags()) == false)
		{
#if WITH_MASSENTITY_DEBUG
			FStringOutputDevice Description;
			Requirements.GetRequiredAnyTags().DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype did not match due to missing \'any\' tags: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}
		
		if (Archetype.GetFragmentBitSet().HasAll(Requirements.GetRequiredAllFragments()) == false)
		{
			// missing some required fragments, skip.
#if WITH_MASSENTITY_DEBUG
			const FMassFragmentBitSet UnsatisfiedFragments = Requirements.GetRequiredAllFragments() - Archetype.GetFragmentBitSet();
			FStringOutputDevice Description;
			UnsatisfiedFragments.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype did not match due to missing Fragments: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Archetype.GetFragmentBitSet().HasNone(Requirements.GetRequiredNoneFragments()) == false)
		{
			// has some Fragments required to be absent
#if WITH_MASSENTITY_DEBUG
			const FMassFragmentBitSet UnwantedFragments = Requirements.GetRequiredAllFragments().GetOverlap(Archetype.GetFragmentBitSet());
			FStringOutputDevice Description;
			UnwantedFragments.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype has Fragments required absent: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Requirements.GetRequiredAnyFragments().IsEmpty() == false 
			&& Archetype.GetFragmentBitSet().HasAny(Requirements.GetRequiredAnyFragments()) == false)
		{
#if WITH_MASSENTITY_DEBUG
			FStringOutputDevice Description;
			Requirements.GetRequiredAnyFragments().DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype did not match due to missing \'any\' fragments: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Archetype.GetChunkFragmentBitSet().HasAll(Requirements.GetRequiredAllChunkFragments()) == false)
		{
			// missing some required fragments, skip.
#if WITH_MASSENTITY_DEBUG
			const FMassChunkFragmentBitSet UnsatisfiedFragments = Requirements.GetRequiredAllChunkFragments() - Archetype.GetChunkFragmentBitSet();
			FStringOutputDevice Description;
			UnsatisfiedFragments.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype did not match due to missing Chunk Fragments: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Archetype.GetChunkFragmentBitSet().HasNone(Requirements.GetRequiredNoneChunkFragments()) == false)
		{
			// has some Fragments required to be absent
#if WITH_MASSENTITY_DEBUG
			const FMassChunkFragmentBitSet UnwantedFragments = Requirements.GetRequiredNoneChunkFragments().GetOverlap(Archetype.GetChunkFragmentBitSet());
			FStringOutputDevice Description;
			UnwantedFragments.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype has Chunk Fragments required absent: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Archetype.GetSharedFragmentBitSet().HasAll(Requirements.GetRequiredAllSharedFragments()) == false)
		{
			// missing some required fragments, skip.
#if WITH_MASSENTITY_DEBUG
			const FMassSharedFragmentBitSet UnsatisfiedFragments = Requirements.GetRequiredAllSharedFragments() - Archetype.GetSharedFragmentBitSet();
			FStringOutputDevice Description;
			UnsatisfiedFragments.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype did not match due to missing Shared Fragments: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Archetype.GetSharedFragmentBitSet().HasNone(Requirements.GetRequiredNoneSharedFragments()) == false)
		{
			// has some Fragments required to be absent
#if WITH_MASSENTITY_DEBUG
			const FMassSharedFragmentBitSet UnwantedFragments = Requirements.GetRequiredNoneSharedFragments().GetOverlap(Archetype.GetSharedFragmentBitSet());
			FStringOutputDevice Description;
			UnwantedFragments.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype has Shared Fragments required absent: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}


		OutValidArchetypes.Add(ArchetypePtr);
	}
}

FMassExecutionContext FMassEntityManager::CreateExecutionContext(const float DeltaSeconds)
{
	FMassExecutionContext ExecutionContext(*this, DeltaSeconds);
	ExecutionContext.SetDeferredCommandBuffer(DeferredCommandBuffer);
	return MoveTemp(ExecutionContext);
}

void FMassEntityManager::FlushCommands(const TSharedPtr<FMassCommandBuffer>& InCommandBuffer)
{
	constexpr int32 MaxIterations = 5;

	if (InCommandBuffer)
	{
		if (InCommandBuffer->HasPendingCommands())
		{
			FlushedCommandBufferQueue.Enqueue(InCommandBuffer);
		}
	}
	else
	{
		if (DeferredCommandBuffer->HasPendingCommands())
		{
			FlushedCommandBufferQueue.Enqueue(DeferredCommandBuffer);
		}
	}

	if (bCommandBufferFlushingInProgress == false && IsProcessing() == false)
	{
		bCommandBufferFlushingInProgress = true;
		
		const int32 IterationsLimit = bFirstCommandFlush ? MAX_int32 : MaxIterations;
		int32 IterationsCounter = 0;
		TOptional<TSharedPtr<FMassCommandBuffer>> CurrentCommandBuffer = FlushedCommandBufferQueue.Dequeue();

		while (IterationsCounter < IterationsLimit && CurrentCommandBuffer.IsSet())
		{
			IterationsCounter++;
			(*CurrentCommandBuffer)->Flush(*this);
			CurrentCommandBuffer = FlushedCommandBufferQueue.Dequeue();
		}
		ensure(IterationsCounter >= IterationsLimit || CurrentCommandBuffer.IsSet() == false);
		UE_CVLOG_UELOG(IterationsCounter >= IterationsLimit, GetOwner(), LogMass, Error, TEXT("Reached loop count limit while flushing commands"));

		bCommandBufferFlushingInProgress = false;
	}
}

void FMassEntityManager::AppendCommands(TSharedPtr<FMassCommandBuffer>& InOutCommandBuffer)
{
	if (!ensureMsgf(InOutCommandBuffer != DeferredCommandBuffer, TEXT("We don't expect AppendCommands to be called with EntityManager's command buffer as the input parameter")))
	{
		return;
	}
	else if (DeferredCommandBuffer->IsFlushing())
	{
		// in this case we'll add InOutCommandBuffer to FlushedCommandBufferQueue
		FlushCommands(InOutCommandBuffer);
	}
	else
	{
		// otherwise we just move all the commands out of InOutCommandBuffer and into the main buffer
		DeferredCommandBuffer->MoveAppend(*InOutCommandBuffer.Get());
	}
}

void FMassEntityManager::SetDebugName(const FString& NewDebugGame) 
{ 
#if WITH_MASSENTITY_DEBUG
	DebugName = NewDebugGame; 
#endif // WITH_MASSENTITY_DEBUG
}

#if WITH_MASSENTITY_DEBUG
void FMassEntityManager::DebugPrintArchetypes(FOutputDevice& Ar, const bool bIncludeEmpty) const
{
	Ar.Logf(ELogVerbosity::Log, TEXT("Listing archetypes contained in EntityManager owned by %s"), *GetPathNameSafe(GetOwner()));

	int32 NumBuckets = 0;
	int32 NumArchetypes = 0;
	int32 LongestArchetypeBucket = 0;
	for (const auto& KVP : FragmentHashToArchetypeMap)
	{
		for (const TSharedPtr<FMassArchetypeData>& ArchetypePtr : KVP.Value)
		{
			if (ArchetypePtr.IsValid() && (bIncludeEmpty == true || ArchetypePtr->GetChunkCount() > 0))
			{
				ArchetypePtr->DebugPrintArchetype(Ar);
			}
		}

		const int32 NumArchetypesInBucket = KVP.Value.Num();
		LongestArchetypeBucket = FMath::Max(LongestArchetypeBucket, NumArchetypesInBucket);
		NumArchetypes += NumArchetypesInBucket;
		++NumBuckets;
	}

	Ar.Logf(ELogVerbosity::Log, TEXT("FragmentHashToArchetypeMap: %d archetypes across %d buckets, longest bucket is %d"),
		NumArchetypes, NumBuckets, LongestArchetypeBucket);
}

void FMassEntityManager::DebugGetArchetypesStringDetails(FOutputDevice& Ar, const bool bIncludeEmpty) const
{
	Ar.SetAutoEmitLineTerminator(true);
	for (auto Pair : FragmentHashToArchetypeMap)
	{
		Ar.Logf(ELogVerbosity::Log, TEXT("\n-----------------------------------\nHash: %u"), Pair.Key);
		for (TSharedPtr<FMassArchetypeData> Archetype : Pair.Value)
		{
			if (Archetype.IsValid() && (bIncludeEmpty == true || Archetype->GetChunkCount() > 0))
			{
				Archetype->DebugPrintArchetype(Ar);
				Ar.Logf(ELogVerbosity::Log, TEXT("+++++++++++++++++++++++++\n"));
			}
		}
	}
}

void FMassEntityManager::DebugGetArchetypeFragmentTypes(const FMassArchetypeHandle& Archetype, TArray<const UScriptStruct*>& InOutFragmentList) const
{
	if (Archetype.IsValid())
	{
		const FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(Archetype);
		ArchetypeData.GetCompositionDescriptor().Fragments.ExportTypes(InOutFragmentList);
	}
}

int32 FMassEntityManager::DebugGetArchetypeEntitiesCount(const FMassArchetypeHandle& Archetype) const
{
	return Archetype.IsValid() ? FMassArchetypeHelper::ArchetypeDataFromHandleChecked(Archetype).GetNumEntities() : 0;
}

int32 FMassEntityManager::DebugGetArchetypeEntitiesCountPerChunk(const FMassArchetypeHandle& Archetype) const
{
	return Archetype.IsValid() ? FMassArchetypeHelper::ArchetypeDataFromHandleChecked(Archetype).GetNumEntitiesPerChunk() : 0;
}

void FMassEntityManager::DebugRemoveAllEntities()
{
	for (int EntityIndex = NumReservedEntities; EntityIndex < Entities.Num(); ++EntityIndex)
	{
		FEntityData& EntityData = Entities[EntityIndex];
		if (EntityData.IsValid() == false)
		{
			// already dead
			continue;
		}
		const TSharedPtr<FMassArchetypeData>& Archetype = EntityData.CurrentArchetype;
		FMassEntityHandle Entity;
		Entity.Index = EntityIndex;
		Entity.SerialNumber = EntityData.SerialNumber;
		Archetype->RemoveEntity(Entity);

		EntityData.Reset();
		EntityFreeIndexList.Add(EntityIndex);
	}
}

void FMassEntityManager::DebugGetArchetypeStrings(const FMassArchetypeHandle& Archetype, TArray<FName>& OutFragmentNames, TArray<FName>& OutTagNames)
{
	if (Archetype.IsValid() == false)
	{
		return;
	}

	const FMassArchetypeData& ArchetypeRef = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(Archetype);
	
	OutFragmentNames.Reserve(ArchetypeRef.GetFragmentConfigs().Num());
	for (const FMassArchetypeFragmentConfig& FragmentConfig : ArchetypeRef.GetFragmentConfigs())
	{
		checkSlow(FragmentConfig.FragmentType);
		OutFragmentNames.Add(FragmentConfig.FragmentType->GetFName());
	}

	ArchetypeRef.GetTagBitSet().DebugGetIndividualNames(OutTagNames);
}

#endif // WITH_MASSENTITY_DEBUG


//-----------------------------------------------------------------------------
// DEPRECATED
//-----------------------------------------------------------------------------
FMassArchetypeHandle FMassEntityManager::CreateArchetype(TConstArrayView<const UScriptStruct*> FragmentsAndTagsList, const FName ArchetypeDebugName)
{
	FMassArchetypeCreationParams Params;
	Params.DebugName = ArchetypeDebugName;
	return CreateArchetype(FragmentsAndTagsList, Params);
}

FMassArchetypeHandle FMassEntityManager::CreateArchetype(FMassArchetypeHandle SourceArchetype, TConstArrayView<const UScriptStruct*> FragmentsAndTagsList, const FName ArchetypeDebugName)
{
	FMassArchetypeCreationParams Params;
	Params.DebugName = ArchetypeDebugName;
	return CreateArchetype(SourceArchetype, FragmentsAndTagsList, Params);
}

FMassArchetypeHandle FMassEntityManager::CreateArchetype(const FMassArchetypeCompositionDescriptor& Composition, const FName ArchetypeDebugName)
{
	FMassArchetypeCreationParams Params;
	Params.DebugName = ArchetypeDebugName;
	return CreateArchetype(Composition, Params);
}

FMassArchetypeHandle FMassEntityManager::CreateArchetype(const TSharedPtr<FMassArchetypeData>& SourceArchetype, const FMassFragmentBitSet& InFragments, const FName ArchetypeDebugName)
{
	FMassArchetypeCreationParams Params;
	Params.DebugName = ArchetypeDebugName;
	return CreateArchetype(SourceArchetype, InFragments, Params);
}

FMassEntityHandle FMassEntityManager::CreateEntity(TConstArrayView<FInstancedStruct> FragmentInstanceList, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FName ArchetypeDebugName)
{
	FMassArchetypeCreationParams Params;
	Params.DebugName = ArchetypeDebugName;
	return CreateEntity(FragmentInstanceList, SharedFragmentValues, Params);
}

void FMassEntityManager::BatchBuildEntities(const FMassArchetypeEntityCollectionWithPayload& EncodedEntitiesWithPayload, const FMassFragmentBitSet& FragmentsAffected
	, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FName ArchetypeDebugName)
{
	FMassArchetypeCreationParams Params;
	Params.DebugName = ArchetypeDebugName;
	BatchBuildEntities(EncodedEntitiesWithPayload, FragmentsAffected, SharedFragmentValues, Params);
}

void FMassEntityManager::BatchBuildEntities(const FMassArchetypeEntityCollectionWithPayload& EncodedEntitiesWithPayload, FMassArchetypeCompositionDescriptor&& Composition
	, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FName ArchetypeDebugName)
{
	FMassArchetypeCreationParams Params;
	Params.DebugName = ArchetypeDebugName;
	BatchBuildEntities(EncodedEntitiesWithPayload, MoveTemp(Composition), SharedFragmentValues, Params);
}

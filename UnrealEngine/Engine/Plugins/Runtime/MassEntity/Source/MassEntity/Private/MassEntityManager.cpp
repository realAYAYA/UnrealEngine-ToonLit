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
	Collector.AddReferencedObjects(ScriptStruct, &ObserverManager);
}

void FMassEntityManager::Initialize()
{
	if (bInitialized)
	{
		UE_LOG(LogMass, Log, TEXT("Calling %s on already initialized entity manager owned by %s")
			, ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Owner.Get()));
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
		UE_LOG(LogMass, Log, TEXT("Calling %s on already deinitialized entity manager owned by %s")
			, ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Owner.Get()));
	}
}

FMassArchetypeHandle FMassEntityManager::CreateArchetype(TConstArrayView<const UScriptStruct*> FragmentsAndTagsList, const FName ArchetypeDebugName)
{
	FMassChunkFragmentBitSet ChunkFragments;
	FMassTagBitSet Tags;
	TArray<const UScriptStruct*, TInlineAllocator<16>> FragmentList;
	FragmentList.Reserve(FragmentsAndTagsList.Num());

	for (const UScriptStruct* Type : FragmentsAndTagsList)
	{
		if (Type->IsChildOf(FMassFragment::StaticStruct()))
		{
			FragmentList.Add(Type);
		}
		else if (Type->IsChildOf(FMassTag::StaticStruct()))
		{
			Tags.Add(*Type);
		}
		else if (Type->IsChildOf(FMassChunkFragment::StaticStruct()))
		{
			ChunkFragments.Add(*Type);
		}
		else
		{
			UE_LOG(LogMass, Warning, TEXT("%s: %s is not a valid fragment nor tag type. Ignoring.")
				, ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Type));
		}
	}

	const FMassArchetypeCompositionDescriptor Composition(FMassFragmentBitSet(FragmentList), Tags, ChunkFragments, FMassSharedFragmentBitSet());
	return CreateArchetype(Composition, ArchetypeDebugName);
}

FMassArchetypeHandle FMassEntityManager::CreateArchetype(const TSharedPtr<FMassArchetypeData>& SourceArchetype, const FMassFragmentBitSet& AddedFragments, const FName ArchetypeDebugName)
{
	check(SourceArchetype.IsValid());
	checkf(AddedFragments.IsEmpty() == false, TEXT("%s Adding an empty fragment list to an archetype is not supported."), ANSI_TO_TCHAR(__FUNCTION__));

	const FMassArchetypeCompositionDescriptor Composition(AddedFragments + SourceArchetype->GetFragmentBitSet(), SourceArchetype->GetTagBitSet(), SourceArchetype->GetChunkFragmentBitSet(), SourceArchetype->GetSharedFragmentBitSet());
	return CreateArchetype(Composition, ArchetypeDebugName);
}

FMassArchetypeHandle FMassEntityManager::CreateArchetype(const FMassArchetypeCompositionDescriptor& Composition, const FName ArchetypeDebugName)
{
	const uint32 TypeHash = Composition.CalculateHash();

	TArray<TSharedPtr<FMassArchetypeData>>& HashRow = FragmentHashToArchetypeMap.FindOrAdd(TypeHash);

	TSharedPtr<FMassArchetypeData> ArchetypeDataPtr;
	for (const TSharedPtr<FMassArchetypeData>& Ptr : HashRow)
	{
		if (Ptr->IsEquivalent(Composition))
		{
			// Keep track of all names for this archetype.
			if (!ArchetypeDebugName.IsNone())
			{
				Ptr->AddUniqueDebugName(ArchetypeDebugName);
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
		FMassArchetypeData* NewArchetype = new FMassArchetypeData();
		NewArchetype->Initialize(Composition, ArchetypeDataVersion);
		if (!ArchetypeDebugName.IsNone())
		{
			NewArchetype->AddUniqueDebugName(ArchetypeDebugName);
		}
		ArchetypeDataPtr = HashRow.Add_GetRef(MakeShareable(NewArchetype));

		for (const FMassArchetypeFragmentConfig& FragmentConfig : NewArchetype->GetFragmentConfigs())
		{
			checkSlow(FragmentConfig.FragmentType)
			FragmentTypeToArchetypeMap.FindOrAdd(FragmentConfig.FragmentType).Add(ArchetypeDataPtr);
		}
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

		for (const FMassArchetypeFragmentConfig& FragmentConfig : NewArchetype->GetFragmentConfigs())
		{
			checkSlow(FragmentConfig.FragmentType)
			FragmentTypeToArchetypeMap.FindOrAdd(FragmentConfig.FragmentType).Add(ArchetypeDataPtr);
		}
	}

	return FMassArchetypeHelper::ArchetypeHandleFromData(ArchetypeDataPtr);
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
			ArchetypePtr->CompactEntities(TimeAllowedLeft);
		}
		if (bReachedTimeLimit)
		{
			break;
		}
	}
}

FMassEntityHandle FMassEntityManager::CreateEntity(const FMassArchetypeHandle& ArchetypeHandle, const FMassArchetypeSharedFragmentValues& SharedFragmentValues)
{
	check(ArchetypeHandle.IsValid());

	const FMassEntityHandle Entity = ReserveEntity();
	InternalBuildEntity(Entity, ArchetypeHandle, SharedFragmentValues);
	return Entity;
}

FMassEntityHandle FMassEntityManager::CreateEntity(TConstArrayView<FInstancedStruct> FragmentInstanceList, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FName ArchetypeDebugName)
{
	check(FragmentInstanceList.Num() > 0);

	const FMassArchetypeHandle& ArchetypeHandle = CreateArchetype(FMassArchetypeCompositionDescriptor(FragmentInstanceList,
		FMassTagBitSet(), FMassChunkFragmentBitSet(), FMassSharedFragmentBitSet()), ArchetypeDebugName);
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
	Result.Index = (EntityFreeIndexList.Num() > 0) ? EntityFreeIndexList.Pop(/*bAllowShrinking=*/ false) : Entities.Add();
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
	checkf(!IsEntityBuilt(Entity), TEXT("Expecting an entity that is not already built"));
	check(ArchetypeHandle.IsValid());

	InternalBuildEntity(Entity, ArchetypeHandle, SharedFragmentValues);
}

void FMassEntityManager::BuildEntity(FMassEntityHandle Entity, TConstArrayView<FInstancedStruct> FragmentInstanceList, const FMassArchetypeSharedFragmentValues& SharedFragmentValues)
{
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

void FMassEntityManager::BatchBuildEntities(const FMassArchetypeEntityCollectionWithPayload& EncodedEntitiesWithPayload, const FMassFragmentBitSet& FragmentsAffected, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FName ArchetypeDebugName)
{
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

	BatchBuildEntities(EncodedEntitiesWithPayload, MoveTemp(Composition), SharedFragmentValues, ArchetypeDebugName);
}

void FMassEntityManager::BatchBuildEntities(const FMassArchetypeEntityCollectionWithPayload& EncodedEntitiesWithPayload, FMassArchetypeCompositionDescriptor&& Composition, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const FName ArchetypeDebugName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchBuildEntities);

	FMassArchetypeEntityCollection::FEntityRangeArray TargetArchetypeEntityRanges;

	// "built" entities case, this is verified during FMassArchetypeEntityCollectionWithPayload construction
	FMassArchetypeHandle TargetArchetypeHandle = CreateArchetype(Composition, ArchetypeDebugName);
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

	if (ObserverManager.HasObserversForBitSet(Composition.Fragments, EMassObservedOperation::Add))
	{
		ObserverManager.OnCompositionChanged(
			FMassArchetypeEntityCollection(TargetArchetypeHandle, MoveTemp(TargetArchetypeEntityRanges))
			, Composition
			, EMassObservedOperation::Add);
	}
}

TSharedRef<FMassEntityManager::FEntityCreationContext> FMassEntityManager::BatchCreateEntities(const FMassArchetypeHandle& ArchetypeHandle, const FMassArchetypeSharedFragmentValues& SharedFragmentValues, const int32 Count, TArray<FMassEntityHandle>& OutEntities)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchCreateEntities);

	FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	check(Count > 0);
	
	int32 Index = OutEntities.Num();
	OutEntities.AddDefaulted(Count);

	// @todo optimize
	for (; Index < OutEntities.Num(); ++Index)
	{
		FMassEntityHandle& Result = OutEntities[Index];
		Result.Index = (EntityFreeIndexList.Num() > 0) ? EntityFreeIndexList.Pop(/*bAllowShrinking=*/ false) : Entities.Add();
		Result.SerialNumber = SerialNumberGenerator++;

		FEntityData& EntityData = Entities[Result.Index];
		EntityData.CurrentArchetype = ArchetypeHandle.DataPtr;
		EntityData.SerialNumber = Result.SerialNumber;
		
		ArchetypeData.AddEntity(Result, SharedFragmentValues);
	}
		
	FEntityCreationContext* CreationContext = new FEntityCreationContext(Count);
	// @todo this could probably be optimized since one would assume we're adding elements to OutEntities in order.
	// Then again, if that's the case, the sorting will be almost instant
	new (&CreationContext->EntityCollection) FMassArchetypeEntityCollection(ArchetypeHandle, MakeArrayView(&OutEntities[OutEntities.Num() - Count], Count), FMassArchetypeEntityCollection::NoDuplicates);
	if (ObserverManager.HasObserversForBitSet(ArchetypeData.GetCompositionDescriptor().Fragments, EMassObservedOperation::Add))
	{
		CreationContext->OnSpawningFinished = [this](FEntityCreationContext& Context){
			ObserverManager.OnPostEntitiesCreated(Context.EntityCollection);
		};
	}

	return MakeShareable(CreationContext);
}

void FMassEntityManager::DestroyEntity(FMassEntityHandle Entity)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %s called during mass processing. Use asynchronous API instead."), ANSI_TO_TCHAR(__FUNCTION__));
	
	CheckIfEntityIsActive(Entity);

	const FEntityData& EntityData = Entities[Entity.Index];
	FMassArchetypeData* Archetype = EntityData.CurrentArchetype.Get();
	check(Archetype);
	Archetype->RemoveEntity(Entity);

	InternalReleaseEntity(Entity);

	if (Archetype->GetNumEntities() == 0)
	{
		//@TODO: Drop our reference to the archetype?
	}
}

void FMassEntityManager::BatchDestroyEntities(TConstArrayView<FMassEntityHandle> InEntities)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchDestroyEntities);

	checkf(IsProcessing() == false, TEXT("Synchronous API function %s called during mass processing. Use asynchronous API instead."), ANSI_TO_TCHAR(__FUNCTION__));
	
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
		Archetype->RemoveEntity(Entity);

		EntityData.Reset();
		EntityFreeIndexList.Add(Entity.Index);
	}
}

void FMassEntityManager::BatchDestroyEntityChunks(const FMassArchetypeEntityCollection& EntityCollection)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mass_BatchDestroyEntityChunks);

	checkf(IsProcessing() == false, TEXT("Synchronous API function %s called during mass processing. Use asynchronous API instead."), ANSI_TO_TCHAR(__FUNCTION__));

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
	checkf(FragmentType, TEXT("Null fragment type passed in to %s"), ANSI_TO_TCHAR(__FUNCTION__));
	checkf(IsProcessing() == false, TEXT("Synchronous API function %s called during mass processing. Use asynchronous API instead."), ANSI_TO_TCHAR(__FUNCTION__));

	CheckIfEntityIsActive(Entity);

	InternalAddFragmentListToEntityChecked(Entity, FMassFragmentBitSet(*FragmentType));
}

void FMassEntityManager::AddFragmentListToEntity(FMassEntityHandle Entity, TConstArrayView<const UScriptStruct*> FragmentList)
{
	CheckIfEntityIsActive(Entity);

	InternalAddFragmentListToEntityChecked(Entity, FMassFragmentBitSet(FragmentList));
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
}

void FMassEntityManager::InternalReleaseEntity(FMassEntityHandle Entity)
{
	FEntityData& EntityData = Entities[Entity.Index];
	EntityData.Reset();
	EntityFreeIndexList.Add(Entity.Index);
}

void FMassEntityManager::InternalAddFragmentListToEntityChecked(FMassEntityHandle Entity, const FMassFragmentBitSet& InFragments)
{
	const FEntityData& EntityData = Entities[Entity.Index];
	FMassArchetypeData* OldArchetype = EntityData.CurrentArchetype.Get();
	check(OldArchetype);

	UE_CLOG(OldArchetype->GetFragmentBitSet().HasAny(InFragments), LogMass, Log
		, TEXT("Trying to add a new fragment type to an entity, but it already has some of them. (%s)")
		, *InFragments.GetOverlap(OldArchetype->GetFragmentBitSet()).DebugGetStringDesc());

	const FMassFragmentBitSet NewFragments = InFragments - OldArchetype->GetFragmentBitSet();
	if (NewFragments.IsEmpty() == false)
	{
		InternalAddFragmentListToEntity(Entity, NewFragments);
	}
}

void FMassEntityManager::InternalAddFragmentListToEntity(FMassEntityHandle Entity, const FMassFragmentBitSet& InFragments)
{
	checkf(InFragments.IsEmpty() == false, TEXT("%s is intended for internal calls with non empty NewFragments parameter"), ANSI_TO_TCHAR(__FUNCTION__));
	check(Entities.IsValidIndex(Entity.Index));
	FEntityData& EntityData = Entities[Entity.Index];
	check(EntityData.CurrentArchetype.IsValid());
	const FMassArchetypeData* OldArchetype = EntityData.CurrentArchetype.Get();

	// fetch or create the new archetype
	const FMassArchetypeHandle NewArchetypeHandle = CreateArchetype(EntityData.CurrentArchetype, InFragments);

	if (NewArchetypeHandle.DataPtr != EntityData.CurrentArchetype)
	{
		// Move the entity over
		FMassArchetypeData& NewArchetype = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(NewArchetypeHandle);
		NewArchetype.CopyDebugNamesFrom(*OldArchetype);
		EntityData.CurrentArchetype->MoveEntityToAnotherArchetype(Entity, NewArchetype);
		EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;
	}
}

void FMassEntityManager::AddFragmentInstanceListToEntity(FMassEntityHandle Entity, TConstArrayView<FInstancedStruct> FragmentInstanceList)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %s called during mass processing. Use asynchronous API instead."), ANSI_TO_TCHAR(__FUNCTION__));

	CheckIfEntityIsActive(Entity);
	checkf(FragmentInstanceList.Num() > 0, TEXT("Need to specify at least one fragment instances for this operation"));

	InternalAddFragmentListToEntityChecked(Entity, FMassFragmentBitSet(FragmentInstanceList));

	const FEntityData& EntityData = Entities[Entity.Index];
	EntityData.CurrentArchetype->SetFragmentsData(Entity, FragmentInstanceList);
}

void FMassEntityManager::RemoveFragmentFromEntity(FMassEntityHandle Entity, const UScriptStruct* FragmentType)
{
	RemoveFragmentListFromEntity(Entity, MakeArrayView(&FragmentType, 1));
}

void FMassEntityManager::RemoveFragmentListFromEntity(FMassEntityHandle Entity, TConstArrayView<const UScriptStruct*> FragmentList)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %s called during mass processing. Use asynchronous API instead."), ANSI_TO_TCHAR(__FUNCTION__));

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

		// Move the entity over
		FMassArchetypeData& NewArchetype = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(NewArchetypeHandle);
		NewArchetype.CopyDebugNamesFrom(*OldArchetype);
		OldArchetype->MoveEntityToAnotherArchetype(Entity, NewArchetype);
		EntityData.CurrentArchetype = NewArchetypeHandle.DataPtr;
	}
}

void FMassEntityManager::SwapTagsForEntity(FMassEntityHandle Entity, const UScriptStruct* OldTagType, const UScriptStruct* NewTagType)
{
	checkf(IsProcessing() == false, TEXT("Synchronous API function %s called during mass processing. Use asynchronous API instead."), ANSI_TO_TCHAR(__FUNCTION__));

	CheckIfEntityIsActive(Entity);

	checkf((OldTagType != nullptr) && OldTagType->IsChildOf(FMassTag::StaticStruct()), TEXT("%s works only with tags while '%s' is not one."), ANSI_TO_TCHAR(__FUNCTION__), *GetPathNameSafe(OldTagType));
	checkf((NewTagType != nullptr) && NewTagType->IsChildOf(FMassTag::StaticStruct()), TEXT("%s works only with tags while '%s' is not one."), ANSI_TO_TCHAR(__FUNCTION__), *GetPathNameSafe(NewTagType));

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
	checkf((TagType != nullptr) && TagType->IsChildOf(FMassTag::StaticStruct()), TEXT("%s works only with tags while '%s' is not one."), ANSI_TO_TCHAR(__FUNCTION__), *GetPathNameSafe(TagType));

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
	}
}
	
void FMassEntityManager::RemoveTagFromEntity(FMassEntityHandle Entity, const UScriptStruct* TagType)
{
	checkf((TagType != nullptr) && TagType->IsChildOf(FMassTag::StaticStruct()), TEXT("%s works only with tags while '%s' is not one."), ANSI_TO_TCHAR(__FUNCTION__), *GetPathNameSafe(TagType));

	CheckIfEntityIsActive(Entity);

	FEntityData& EntityData = Entities[Entity.Index];
	FMassArchetypeData* CurrentArchetype = EntityData.CurrentArchetype.Get();
	check(CurrentArchetype);

	if (CurrentArchetype->HasTagType(TagType))
	{
		// CurrentArchetype->GetTagBitSet() -  *TagType
		FMassTagBitSet NewTags = CurrentArchetype->GetTagBitSet();
		NewTags.Remove(*TagType);
		const FMassArchetypeHandle NewArchetypeHandle = InternalCreateSimilarArchetype(EntityData.CurrentArchetype, NewTags);
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
			TArray<FMassEntityHandle> EntitesBeingMoved;
			CurrentArchetype->BatchMoveEntitiesToAnotherArchetype(Collection, *NewArchetypeHandle.DataPtr.Get(), EntitesBeingMoved
				, bTagsAddedAreObserved ? &NewArchetypeEntityRanges : nullptr);

			for (const FMassEntityHandle& Entity : EntitesBeingMoved)
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

		if (CurrentArchetype && CurrentArchetype->GetFragmentBitSet() != NewFragmentComposition)
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
			TArray<FMassEntityHandle> EntitesBeingMoved;
			CurrentArchetype->BatchMoveEntitiesToAnotherArchetype(Collection, *NewArchetypeHandle.DataPtr.Get(), EntitesBeingMoved
				, bFragmentsAddedAreObserved ? &NewArchetypeEntityRanges : nullptr);

			for (const FMassEntityHandle& Entity : EntitesBeingMoved)
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
				TArray<FMassEntityHandle> EntitesBeingMoved;
				CurrentArchetype->BatchMoveEntitiesToAnotherArchetype(EntityRangesWithPayload.GetEntityCollection(), *NewArchetypeHandle.DataPtr.Get()
					, EntitesBeingMoved, &TargetArchetypeEntityRanges);

				for (const FMassEntityHandle& Entity : EntitesBeingMoved)
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
	CheckIfEntityIsActive(Entity);

	checkf((FragmentType != nullptr) && FragmentType->IsChildOf(FMassFragment::StaticStruct()), TEXT("InternalGetFragmentDataChecked called with an invalid fragment type '%s'"), *GetPathNameSafe(FragmentType));
	const FEntityData& EntityData = Entities[Entity.Index];
	return EntityData.CurrentArchetype->GetFragmentDataForEntityChecked(FragmentType, Entity.Index);
}

void* FMassEntityManager::InternalGetFragmentDataPtr(FMassEntityHandle Entity, const UScriptStruct* FragmentType) const
{
	CheckIfEntityIsActive(Entity);
	checkf((FragmentType != nullptr) && FragmentType->IsChildOf(FMassFragment::StaticStruct()), TEXT("InternalGetFragmentData called with an invalid fragment type '%s'"), *GetPathNameSafe(FragmentType));
	const FEntityData& EntityData = Entities[Entity.Index];
	return EntityData.CurrentArchetype->GetFragmentDataForEntity(FragmentType, Entity.Index);
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
	checkf(IsEntityBuilt(Entity), TEXT("Entity not yet created(ID: %d, SN:%d)"));
}

void FMassEntityManager::GetValidArchetypes(const FMassEntityQuery& Query, TArray<FMassArchetypeHandle>& OutValidArchetypes, const uint32 FromArchetypeDataVersion) const
{
	//@TODO: Not optimized yet, but we call this rarely now, so not a big deal.

	// First get set of all archetypes that contain *any* fragment
	TSet<TSharedPtr<FMassArchetypeData>> AnyArchetypes;
	for (const FMassFragmentRequirementDescription& Requirement : Query.GetFragmentRequirements())
	{
		check(Requirement.StructType);
		if (Requirement.Presence != EMassFragmentPresence::None)
		{
			if (const TArray<TSharedPtr<FMassArchetypeData>>* pData = FragmentTypeToArchetypeMap.Find(Requirement.StructType))
			{
				AnyArchetypes.Append(*pData);
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

		if (Archetype.GetTagBitSet().HasAll(Query.GetRequiredAllTags()) == false)
		{
			// missing some required tags, skip.
#if WITH_MASSENTITY_DEBUG
			const FMassTagBitSet UnsatisfiedTags = Query.GetRequiredAllTags() - Archetype.GetTagBitSet();
			FStringOutputDevice Description;
			UnsatisfiedTags.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype did not match due to missing tags: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Archetype.GetTagBitSet().HasNone(Query.GetRequiredNoneTags()) == false)
		{
			// has some tags required to be absent
#if WITH_MASSENTITY_DEBUG
			const FMassTagBitSet UnwantedTags = Query.GetRequiredAllTags().GetOverlap(Archetype.GetTagBitSet());
			FStringOutputDevice Description;
			UnwantedTags.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype has tags required absent: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Query.GetRequiredAnyTags().IsEmpty() == false 
			&& Archetype.GetTagBitSet().HasAny(Query.GetRequiredAnyTags()) == false)
		{
#if WITH_MASSENTITY_DEBUG
			FStringOutputDevice Description;
			Query.GetRequiredAnyTags().DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype did not match due to missing \'any\' tags: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}
		
		if (Archetype.GetFragmentBitSet().HasAll(Query.GetRequiredAllFragments()) == false)
		{
			// missing some required fragments, skip.
#if WITH_MASSENTITY_DEBUG
			const FMassFragmentBitSet UnsatisfiedFragments = Query.GetRequiredAllFragments() - Archetype.GetFragmentBitSet();
			FStringOutputDevice Description;
			UnsatisfiedFragments.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype did not match due to missing Fragments: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Archetype.GetFragmentBitSet().HasNone(Query.GetRequiredNoneFragments()) == false)
		{
			// has some Fragments required to be absent
#if WITH_MASSENTITY_DEBUG
			const FMassFragmentBitSet UnwantedFragments = Query.GetRequiredAllFragments().GetOverlap(Archetype.GetFragmentBitSet());
			FStringOutputDevice Description;
			UnwantedFragments.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype has Fragments required absent: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Query.GetRequiredAnyFragments().IsEmpty() == false 
			&& Archetype.GetFragmentBitSet().HasAny(Query.GetRequiredAnyFragments()) == false)
		{
#if WITH_MASSENTITY_DEBUG
			FStringOutputDevice Description;
			Query.GetRequiredAnyFragments().DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype did not match due to missing \'any\' fragments: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Archetype.GetChunkFragmentBitSet().HasAll(Query.GetRequiredAllChunkFragments()) == false)
		{
			// missing some required fragments, skip.
#if WITH_MASSENTITY_DEBUG
			const FMassChunkFragmentBitSet UnsatisfiedFragments = Query.GetRequiredAllChunkFragments() - Archetype.GetChunkFragmentBitSet();
			FStringOutputDevice Description;
			UnsatisfiedFragments.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype did not match due to missing Chunk Fragments: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Archetype.GetChunkFragmentBitSet().HasNone(Query.GetRequiredNoneChunkFragments()) == false)
		{
			// has some Fragments required to be absent
#if WITH_MASSENTITY_DEBUG
			const FMassChunkFragmentBitSet UnwantedFragments = Query.GetRequiredNoneChunkFragments().GetOverlap(Archetype.GetChunkFragmentBitSet());
			FStringOutputDevice Description;
			UnwantedFragments.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype has Chunk Fragments required absent: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Archetype.GetSharedFragmentBitSet().HasAll(Query.GetRequiredAllSharedFragments()) == false)
		{
			// missing some required fragments, skip.
#if WITH_MASSENTITY_DEBUG
			const FMassSharedFragmentBitSet UnsatisfiedFragments = Query.GetRequiredAllSharedFragments() - Archetype.GetSharedFragmentBitSet();
			FStringOutputDevice Description;
			UnsatisfiedFragments.DebugGetStringDesc(Description);
			UE_LOG(LogMass, VeryVerbose, TEXT("Archetype did not match due to missing Shared Fragments: %s")
				, *Description);
#endif // WITH_MASSENTITY_DEBUG
			continue;
		}

		if (Archetype.GetSharedFragmentBitSet().HasNone(Query.GetRequiredNoneSharedFragments()) == false)
		{
			// has some Fragments required to be absent
#if WITH_MASSENTITY_DEBUG
			const FMassSharedFragmentBitSet UnwantedFragments = Query.GetRequiredNoneSharedFragments().GetOverlap(Archetype.GetSharedFragmentBitSet());
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
	FMassExecutionContext ExecutionContext(AsShared(), DeltaSeconds);
	ExecutionContext.SetDeferredCommandBuffer(DeferredCommandBuffer);
	return MoveTemp(ExecutionContext);
}

void FMassEntityManager::FlushCommands(const TSharedPtr<FMassCommandBuffer>& InCommandBuffer)
{
	constexpr int MaxIterations = 3;

	if (InCommandBuffer)
	{
		FlushedCommandBufferQueue.Enqueue(InCommandBuffer);
	}
	else
	{
		FlushedCommandBufferQueue.Enqueue(DeferredCommandBuffer);
	}

	if (bCommandBufferFlushingInProgress == false && IsProcessing() == false)
	{
		bCommandBufferFlushingInProgress = true;
		
		int IterationsCounter = 0;
		TOptional<TSharedPtr<FMassCommandBuffer>> CurrentCommandBuffer = FlushedCommandBufferQueue.Dequeue();
		while (IterationsCounter++ < MaxIterations && CurrentCommandBuffer.IsSet())
		{
			(*CurrentCommandBuffer)->Flush(*this);
			CurrentCommandBuffer = FlushedCommandBufferQueue.Dequeue();
		}
		ensure(IterationsCounter >= MaxIterations || CurrentCommandBuffer.IsSet() == false);
		UE_CVLOG_UELOG(IterationsCounter >= MaxIterations, GetOwner(), LogMass, Error, TEXT("Reached loop count limit while flushing commands"));

		bCommandBufferFlushingInProgress = false;
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

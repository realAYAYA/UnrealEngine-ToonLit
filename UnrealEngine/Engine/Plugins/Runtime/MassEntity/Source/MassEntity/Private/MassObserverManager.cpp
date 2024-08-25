// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassObserverManager.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassExecutor.h"
#include "MassProcessingTypes.h"
#include "MassObserverRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MassObserverManager)

namespace UE::Mass::ObserverManager
{
	namespace Tweakables
	{
		// Used as a template parameter for TInlineAllocator that we use when gathering UScriptStruct* of the observed types to process.
		constexpr int InlineAllocatorElementsForOverlapTypes = 8;
	} // Tweakables

	namespace Private
	{
	// a helper function to reduce code duplication in FMassObserverManager::Initialize
	template<typename TBitSet, typename TPointerType>
	void SetUpObservers(FMassEntityManager& EntityManager, const EProcessorExecutionFlags WorldExecutionFlags, UObject& Owner
		, const TMap<TPointerType, FMassProcessorClassCollection>& RegisteredObserverTypes, TBitSet& ObservedBitSet, FMassObserversMap& Observers)
	{
		ObservedBitSet.Reset();

		for (auto It : RegisteredObserverTypes)
		{
			if (It.Value.ClassCollection.Num() == 0)
			{
				continue;
			}

			ObservedBitSet.Add(*It.Key);
			FMassRuntimePipeline& Pipeline = (*Observers).FindOrAdd(It.Key);

			for (const TSubclassOf<UMassProcessor>& ProcessorClass : It.Value.ClassCollection)
			{
				if (ProcessorClass->GetDefaultObject<UMassProcessor>()->ShouldExecute(WorldExecutionFlags))
				{
					Pipeline.AppendProcessor(ProcessorClass, Owner);
				}
			}
			Pipeline.Initialize(Owner);
		}
	};
	} // Private
} // UE::Mass::ObserverManager

//----------------------------------------------------------------------//
// FMassObserverManager
//----------------------------------------------------------------------//
FMassObserverManager::FMassObserverManager()
	: EntityManager(GetMutableDefault<UMassEntitySubsystem>()->GetMutableEntityManager())
{

}

FMassObserverManager::FMassObserverManager(FMassEntityManager& Owner)
	: EntityManager(Owner)
{

}

void FMassObserverManager::Initialize()
{
	// instantiate initializers
	const UMassObserverRegistry& Registry = UMassObserverRegistry::Get();

	UObject* Owner = EntityManager.GetOwner();
	check(Owner);
	const UWorld* World = Owner->GetWorld();
	const EProcessorExecutionFlags WorldExecutionFlags = UE::Mass::Utils::DetermineProcessorExecutionFlags(World);

	using UE::Mass::ObserverManager::Private::SetUpObservers;
	for (int i = 0; i < (int)EMassObservedOperation::MAX; ++i)
	{
		SetUpObservers(EntityManager, WorldExecutionFlags, *Owner, *Registry.FragmentObservers[i], ObservedFragments[i], FragmentObservers[i]);
		SetUpObservers(EntityManager, WorldExecutionFlags, *Owner, *Registry.TagObservers[i], ObservedTags[i], TagObservers[i]);
	}
}

bool FMassObserverManager::OnPostEntitiesCreated(const FMassArchetypeEntityCollection& EntityCollection)
{
	FMassProcessingContext ProcessingContext(EntityManager, /*DeltaSeconds=*/0.f);
	// requesting not to flush commands since handling creation of new entities can result in multiple collections of
	// processors being executed and flushing commands between these runs would ruin EntityCollection since entities could
	// get their composition changed and get moved to new archetypes
	ProcessingContext.bFlushCommandBuffer = false;
	ProcessingContext.CommandBuffer = MakeShareable(new FMassCommandBuffer());

	if (OnPostEntitiesCreated(ProcessingContext, EntityCollection))
	{
		EntityManager.FlushCommands(ProcessingContext.CommandBuffer);
		return true;
	}
	return false;
}

bool FMassObserverManager::OnPostEntitiesCreated(FMassProcessingContext& ProcessingContext, const FMassArchetypeEntityCollection& EntityCollection)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("OnPostEntitiesCreated")

	check(ProcessingContext.EntityManager);
	const FMassArchetypeCompositionDescriptor& ArchetypeComposition = ProcessingContext.EntityManager->GetArchetypeComposition(EntityCollection.GetArchetype());

	return OnCompositionChanged(ProcessingContext, EntityCollection, ArchetypeComposition, EMassObservedOperation::Add);
}

bool FMassObserverManager::OnPreEntitiesDestroyed(const FMassArchetypeEntityCollection& EntityCollection)
{
	FMassProcessingContext ProcessingContext(EntityManager, /*DeltaSeconds=*/0.f);
	ProcessingContext.bFlushCommandBuffer = false;
	ProcessingContext.CommandBuffer = MakeShareable(new FMassCommandBuffer());

	if (OnPreEntitiesDestroyed(ProcessingContext, EntityCollection))
	{
		EntityManager.FlushCommands(ProcessingContext.CommandBuffer);
		return true;
	}
	return false;
}

bool FMassObserverManager::OnPreEntitiesDestroyed(FMassProcessingContext& ProcessingContext, const FMassArchetypeEntityCollection& EntityCollection)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("OnPreEntitiesDestroyed")

	check(ProcessingContext.EntityManager);
	const FMassArchetypeCompositionDescriptor& ArchetypeComposition = ProcessingContext.EntityManager->GetArchetypeComposition(EntityCollection.GetArchetype());
	
	return OnCompositionChanged(ProcessingContext, EntityCollection, ArchetypeComposition, EMassObservedOperation::Remove);
}

bool FMassObserverManager::OnPreEntityDestroyed(const FMassArchetypeCompositionDescriptor& ArchetypeComposition, const FMassEntityHandle Entity)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("OnPreEntityDestroyed")
	return OnCompositionChanged(Entity, ArchetypeComposition, EMassObservedOperation::Remove);
}

bool FMassObserverManager::OnCompositionChanged(FMassProcessingContext& ProcessingContext, const FMassArchetypeEntityCollection& EntityCollection, const FMassArchetypeCompositionDescriptor& CompositionDelta, const EMassObservedOperation Operation)
{
	using UE::Mass::ObserverManager::Tweakables::InlineAllocatorElementsForOverlapTypes;

	const FMassFragmentBitSet FragmentOverlap = ObservedFragments[(uint8)Operation].GetOverlap(CompositionDelta.Fragments);
	const bool bHasFragmentsOverlap = !FragmentOverlap.IsEmpty();
	const FMassTagBitSet TagOverlap = ObservedTags[(uint8)Operation].GetOverlap(CompositionDelta.Tags);
	const bool bHasTagsOverlap = !TagOverlap.IsEmpty();

	if (bHasFragmentsOverlap || bHasTagsOverlap)
	{
		TArray<const UScriptStruct*, TInlineAllocator<InlineAllocatorElementsForOverlapTypes>> ObservedTypesOverlap;

		if (bHasFragmentsOverlap)
		{
			FragmentOverlap.ExportTypes(ObservedTypesOverlap);

			HandleFragmentsImpl(ProcessingContext, EntityCollection, ObservedTypesOverlap, FragmentObservers[(uint8)Operation]);
		}

		if (bHasTagsOverlap)
		{
			ObservedTypesOverlap.Reset();
			TagOverlap.ExportTypes(ObservedTypesOverlap);

			HandleFragmentsImpl(ProcessingContext, EntityCollection, ObservedTypesOverlap, TagObservers[(uint8)Operation]);
		}

		return true;
	}

	return false;
}

bool FMassObserverManager::OnCompositionChanged(const FMassEntityHandle Entity, const FMassArchetypeCompositionDescriptor& CompositionDelta, const EMassObservedOperation Operation)
{
	using UE::Mass::ObserverManager::Tweakables::InlineAllocatorElementsForOverlapTypes;

	const FMassFragmentBitSet FragmentOverlap = ObservedFragments[(uint8)Operation].GetOverlap(CompositionDelta.Fragments);
	const bool bHasFragmentsOverlap = !FragmentOverlap.IsEmpty();
	const FMassTagBitSet TagOverlap = ObservedTags[(uint8)Operation].GetOverlap(CompositionDelta.Tags);
	const bool bHasTagsOverlap = !TagOverlap.IsEmpty();

	if (bHasFragmentsOverlap || bHasTagsOverlap)
	{
		TArray<const UScriptStruct*, TInlineAllocator<InlineAllocatorElementsForOverlapTypes>> ObservedTypesOverlap;
		FMassProcessingContext ProcessingContext(EntityManager, /*DeltaSeconds=*/0.f);
		ProcessingContext.bFlushCommandBuffer = false;
		const FMassArchetypeHandle ArchetypeHandle = EntityManager.GetArchetypeForEntity(Entity);

		if (bHasFragmentsOverlap)
		{
			FragmentOverlap.ExportTypes(ObservedTypesOverlap);

			HandleFragmentsImpl(ProcessingContext, FMassArchetypeEntityCollection(ArchetypeHandle, MakeArrayView(&Entity, 1)
				, FMassArchetypeEntityCollection::NoDuplicates), ObservedTypesOverlap, FragmentObservers[(uint8)Operation]);
		}

		if (bHasTagsOverlap)
		{
			ObservedTypesOverlap.Reset();
			TagOverlap.ExportTypes(ObservedTypesOverlap);

			HandleFragmentsImpl(ProcessingContext, FMassArchetypeEntityCollection(ArchetypeHandle, MakeArrayView(&Entity, 1)
				, FMassArchetypeEntityCollection::NoDuplicates), ObservedTypesOverlap, TagObservers[(uint8)Operation]);
		}
	}

	return bHasFragmentsOverlap || bHasTagsOverlap;
}

void FMassObserverManager::OnFragmentOrTagOperation(const UScriptStruct& FragmentOrTagType, const FMassArchetypeEntityCollection& EntityCollection, const EMassObservedOperation Operation)
{
	check(FragmentOrTagType.IsChildOf(FMassFragment::StaticStruct()) || FragmentOrTagType.IsChildOf(FMassTag::StaticStruct()));

	if (FragmentOrTagType.IsChildOf(FMassFragment::StaticStruct()))
	{
		if (ObservedFragments[(uint8)Operation].Contains(FragmentOrTagType))
		{
			HandleSingleEntityImpl(FragmentOrTagType, EntityCollection, FragmentObservers[(uint8)Operation]);
		}
	}
	else if (ObservedTags[(uint8)Operation].Contains(FragmentOrTagType))
	{
		HandleSingleEntityImpl(FragmentOrTagType, EntityCollection, TagObservers[(uint8)Operation]);
	}
}

void FMassObserverManager::HandleFragmentsImpl(FMassProcessingContext& ProcessingContext, const FMassArchetypeEntityCollection& EntityCollection
	, TArrayView<const UScriptStruct*> ObservedTypes
	/*, const FMassFragmentBitSet& FragmentsBitSet*/, FMassObserversMap& HandlersContainer)
{	
	TRACE_CPUPROFILER_EVENT_SCOPE(MassObserver_HandleFragmentsImpl);

	check(ObservedTypes.Num() > 0);

	for (const UScriptStruct* Type : ObservedTypes)
	{		
		ProcessingContext.AuxData.InitializeAs(Type);
		FMassRuntimePipeline& Pipeline = (*HandlersContainer).FindChecked(Type);

		UE::Mass::Executor::RunProcessorsView(Pipeline.GetMutableProcessors(), ProcessingContext, &EntityCollection);
	}
}

void FMassObserverManager::HandleSingleEntityImpl(const UScriptStruct& FragmentType, const FMassArchetypeEntityCollection& EntityCollection, FMassObserversMap& HandlersContainer)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MassObserver_HandleSingleEntityImpl);

	FMassProcessingContext ProcessingContext(EntityManager, /*DeltaSeconds=*/0.f);
	ProcessingContext.bFlushCommandBuffer = false;
	ProcessingContext.AuxData.InitializeAs(&FragmentType);
	FMassRuntimePipeline& Pipeline = (*HandlersContainer).FindChecked(&FragmentType);

	UE::Mass::Executor::RunProcessorsView(Pipeline.GetMutableProcessors(), ProcessingContext, &EntityCollection);
}

void FMassObserverManager::AddObserverInstance(const UScriptStruct& FragmentOrTagType, const EMassObservedOperation Operation, UMassProcessor& ObserverProcessor)
{
	checkSlow(FragmentOrTagType.IsChildOf(FMassFragment::StaticStruct()) || FragmentOrTagType.IsChildOf(FMassTag::StaticStruct()));

	FMassRuntimePipeline* Pipeline = nullptr;

	if (FragmentOrTagType.IsChildOf(FMassFragment::StaticStruct()))
	{
		Pipeline = &(*FragmentObservers[(uint8)Operation]).FindOrAdd(&FragmentOrTagType);
		ObservedFragments[(uint8)Operation].Add(FragmentOrTagType);
	}
	else
	{
		Pipeline = &(*TagObservers[(uint8)Operation]).FindOrAdd(&FragmentOrTagType);
		ObservedTags[(uint8)Operation].Add(FragmentOrTagType);
	}
	Pipeline->AppendProcessor(ObserverProcessor);

	// calling initialize to ensure the given processor is related to the same EntityManager
	if (UObject* Owner = EntityManager.GetOwner())
	{	
		ObserverProcessor.Initialize(*Owner);
	}
}

void FMassObserverManager::RemoveObserverInstance(const UScriptStruct& FragmentOrTagType, const EMassObservedOperation Operation, UMassProcessor& ObserverProcessor)
{
	if (!ensure(FragmentOrTagType.IsChildOf(FMassFragment::StaticStruct()) || FragmentOrTagType.IsChildOf(FMassTag::StaticStruct())))
	{
		return;
	}

	bool bIsFragmentObserver = FragmentOrTagType.IsChildOf(FMassFragment::StaticStruct());

	TMap<TObjectPtr<const UScriptStruct>, FMassRuntimePipeline>& ObserversMap =
		bIsFragmentObserver ? *FragmentObservers[(uint8)Operation] : *TagObservers[(uint8)Operation];

	FMassRuntimePipeline* Pipeline = ObserversMap.Find(&FragmentOrTagType);
	if (!ensureMsgf(Pipeline, TEXT("Trying to remove an observer for a fragment/tag that does not seem to be observed.")))
	{
		return;
	}
	Pipeline->RemoveProcessor(ObserverProcessor);

	if (Pipeline->Num() == 0)
	{
		ObserversMap.Remove(&FragmentOrTagType);
		if (bIsFragmentObserver)
		{
			ObservedFragments[(uint8)Operation].Remove(FragmentOrTagType);
		}
		else
		{
			ObservedTags[(uint8)Operation].Remove(FragmentOrTagType);
		}
	}
}

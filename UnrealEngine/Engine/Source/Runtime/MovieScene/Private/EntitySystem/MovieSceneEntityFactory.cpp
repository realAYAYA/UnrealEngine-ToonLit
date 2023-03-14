// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneEntityFactory.h"
#include "EntitySystem/MovieSceneComponentRegistry.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntityManager.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntityFactoryTemplates.h"

namespace UE
{
namespace MovieScene
{

int32 FChildEntityFactory::Num() const
{
	return ParentEntityOffsets.Num();
}

int32 FChildEntityFactory::GetCurrentIndex() const
{
	if (const int32* CurrentOffset = CurrentEntityOffsets.GetData())
	{
		return CurrentOffset - ParentEntityOffsets.GetData();
	}
	return INDEX_NONE;
}

void FChildEntityFactory::Apply(UMovieSceneEntitySystemLinker* Linker, FEntityAllocationProxy ParentAllocationProxy)
{
	const FComponentMask ParentType = ParentAllocationProxy.GetAllocationType();

	FComponentMask DerivedEntityType;
	{
		GenerateDerivedType(DerivedEntityType);

		Linker->EntityManager.GetComponents()->Factories.ComputeChildComponents(ParentType, DerivedEntityType);
		Linker->EntityManager.GetComponents()->Factories.ComputeMutuallyInclusiveComponents(DerivedEntityType);
	}

	const bool bHasAnyType = DerivedEntityType.Find(true) != INDEX_NONE;
	if (!bHasAnyType)
	{
		return;
	}

	const int32 NumToAdd = Num();

	int32 CurrentParentOffset = 0;
	const FEntityAllocation* ParentAllocation = ParentAllocationProxy.GetAllocation();
	FEntityAllocationWriteContext WriteContext = FEntityAllocationWriteContext::NewAllocation();

	// We attempt to allocate all the linker entities contiguously in memory for efficient initialization,
	// but we may reach capacity constraints within allocations so we may have to run the factories more than once
	while(CurrentParentOffset < NumToAdd)
	{
		// Ask to allocate as many as possible - we may only manage to allocate a smaller number contiguously this iteration however
		int32 NumAdded = NumToAdd - CurrentParentOffset;

		FEntityDataLocation NewLinkerEntities = Linker->EntityManager.AllocateContiguousEntities(DerivedEntityType, &NumAdded);
		FEntityRange ChildRange{ NewLinkerEntities.Allocation, NewLinkerEntities.ComponentOffset, NumAdded };

		CurrentEntityOffsets = MakeArrayView(ParentEntityOffsets.GetData() + CurrentParentOffset, NumAdded);

		if (TOptionalComponentWriter<FMovieSceneEntityID> ParentEntityIDs =
			ChildRange.Allocation->TryWriteComponents(FBuiltInComponentTypes::Get()->ParentEntity, WriteContext))
		{
			TArrayView<const FMovieSceneEntityID> ParentIDs = ParentAllocation->GetEntityIDs();
			for (int32 Index = 0; Index < ChildRange.Num; ++Index)
			{
				const int32 ParentIndex = CurrentEntityOffsets[Index];
				const int32 ChildIndex  = ChildRange.ComponentStartOffset + Index;

				ParentEntityIDs[ChildIndex] = ParentIDs[ParentIndex];
			}
		}

		// Initialize the bound objects before we call child initializers
		InitializeAllocation(Linker, ParentType, DerivedEntityType, ParentAllocation, CurrentEntityOffsets, ChildRange);

		Linker->EntityManager.InitializeChildAllocation(ParentType, DerivedEntityType, ParentAllocation, CurrentEntityOffsets, ChildRange);

		CurrentParentOffset += NumAdded;
	}

	PostInitialize(Linker);
}


void FObjectFactoryBatch::Add(int32 EntityIndex, UObject* BoundObject)
{
	ParentEntityOffsets.Add(EntityIndex);
	ObjectsToAssign.Add(BoundObject);
}

void FObjectFactoryBatch::GenerateDerivedType(FComponentMask& OutNewEntityType)
{
	OutNewEntityType.Set(FBuiltInComponentTypes::Get()->BoundObject);
}

void FObjectFactoryBatch::InitializeAllocation(UMovieSceneEntitySystemLinker* Linker, const FComponentMask& ParentType, const FComponentMask& ChildType, const FEntityAllocation* ParentAllocation, TArrayView<const int32> ParentAllocationOffsets, const FEntityRange& InChildEntityRange)
{
	TSortedMap<UObject*, FMovieSceneEntityID, TInlineAllocator<8>> ChildMatchScratch;

	TComponentTypeID<UObject*>            BoundObject = FBuiltInComponentTypes::Get()->BoundObject;
	TComponentTypeID<FMovieSceneEntityID> ParentEntity = FBuiltInComponentTypes::Get()->ParentEntity;

	TArrayView<const FMovieSceneEntityID> ParentIDs = ParentAllocation->GetEntityIDs();

	FEntityAllocationWriteContext WriteContext = FEntityAllocationWriteContext::NewAllocation();

	int32 Index = GetCurrentIndex();
	for (TEntityPtr<const FMovieSceneEntityID, const FMovieSceneEntityID, UObject*> Tuple : FEntityTaskBuilder().ReadEntityIDs().Read(ParentEntity).Write(BoundObject).IterateRange(InChildEntityRange, WriteContext))
	{
		FMovieSceneEntityID Parent = Tuple.Get<1>();
		FMovieSceneEntityID Child = Tuple.Get<0>();

		UObject* Object = ObjectsToAssign[Index++];
		Tuple.Get<2>() = Object;

		if (FMovieSceneEntityID OldEntityToPreserve = StaleEntitiesToPreserve->FindRef(MakeTuple(Object, Parent)))
		{
			PreservedEntities.Add(Child, OldEntityToPreserve);
		}
		Linker->EntityManager.AddChild(Parent, Child);
	}
}

void FObjectFactoryBatch::PostInitialize(UMovieSceneEntitySystemLinker* InLinker)
{
	FComponentMask PreservationMask = InLinker->EntityManager.GetComponents()->GetPreservationMask();

	for (TTuple<FMovieSceneEntityID, FMovieSceneEntityID> Pair : PreservedEntities)
	{
		InLinker->EntityManager.CombineComponents(Pair.Key, Pair.Value, &PreservationMask);
	}
}

FBoundObjectTask::FBoundObjectTask(UMovieSceneEntitySystemLinker* InLinker)
	: Linker(InLinker)
{}

void FBoundObjectTask::ForEachAllocation(FEntityAllocationProxy AllocationProxy, FReadEntityIDs EntityIDs, TRead<FInstanceHandle> Instances, TRead<FGuid> ObjectBindings)
{
	const FEntityAllocation* Allocation = AllocationProxy.GetAllocation();
	const FComponentTypeID TagHasUnresolvedBinding = FBuiltInComponentTypes::Get()->Tags.HasUnresolvedBinding;

	// Check whether every binding in this allocation is currently unresolved
	const bool bWasUnresolvedBinding = Allocation->FindComponentHeader(TagHasUnresolvedBinding) != nullptr;

	FObjectFactoryBatch& Batch = AddBatch(AllocationProxy);
	Batch.StaleEntitiesToPreserve = &StaleEntitiesToPreserve;

	const int32 Num = Allocation->Num();

	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	// Keep track of existing bindings so we can preserve any components on them
	TComponentTypeID<UObject*> BoundObjectComponent = FBuiltInComponentTypes::Get()->BoundObject;

	for (int32 Index = 0; Index < Num; ++Index)
	{
		FMovieSceneEntityID ParentID = EntityIDs[Index];

		// Discard existing children
		const int32 StartNum = EntitiesToDiscard.Num();
		Linker->EntityManager.GetImmediateChildren(ParentID, EntitiesToDiscard);

		// Keep track of any existing object bindings so we can preserve components on them if they are resolved to the same thing
		for (int32 ChildIndex = StartNum; ChildIndex < EntitiesToDiscard.Num(); ++ChildIndex)
		{
			FMovieSceneEntityID ChildID = EntitiesToDiscard[ChildIndex];
			TOptionalComponentReader<UObject*> ObjectPtr = Linker->EntityManager.ReadComponent(ChildID,  BoundObjectComponent);
			if (ObjectPtr)
			{
				StaleEntitiesToPreserve.Add(MakeTuple(*ObjectPtr, ParentID), ChildID);
			}
		}

		const FObjectFactoryBatch::EResolveError Error = Batch.ResolveObjects(InstanceRegistry, Instances[Index], Index, ObjectBindings[Index]);
		if (Error == FObjectFactoryBatch::EResolveError::None)
		{
			// We have successfully resolved a binding, so remove the HasUnresolvedBinding tag
			if (bWasUnresolvedBinding)
			{
				constexpr bool bAddComponent = false;
				EntityMutations.Add(FEntityMutationData{ ParentID, TagHasUnresolvedBinding, bAddComponent });
			}
		}
		else if (Error == FObjectFactoryBatch::EResolveError::UnresolvedBinding)
		{
			if (!bWasUnresolvedBinding)
			{
				// Only bother attempting to add the HasUnresolvedBindingTag if it is not already tagged in such a way
				constexpr bool bAddComponent = true;
				EntityMutations.Add(FEntityMutationData{ ParentID, TagHasUnresolvedBinding, bAddComponent });
			}
		}
	}

}

void FBoundObjectTask::PostTask()
{
	Apply();

	FComponentTypeID NeedsUnlink = FBuiltInComponentTypes::Get()->Tags.NeedsUnlink;
	for (FMovieSceneEntityID Discard : EntitiesToDiscard)
	{
		Linker->EntityManager.AddComponent(Discard, NeedsUnlink, EEntityRecursion::Full);
	}

	for (FEntityMutationData Mutation : EntityMutations)
	{
		if (Mutation.bAddComponent)
		{
			Linker->EntityManager.AddComponent(Mutation.EntityID, Mutation.ComponentTypeID);
		}
		else
		{
			Linker->EntityManager.RemoveComponent(Mutation.EntityID, Mutation.ComponentTypeID);
		}
	}
}

void FEntityFactories::DefineChildComponent(TInlineValue<FChildEntityInitializer>&& InInitializer)
{
	check(InInitializer.IsValid());

	DefineChildComponent(InInitializer->GetParentComponent(), InInitializer->GetChildComponent());
	// Note: after this line, InInitializer is reset
	ChildInitializers.Add(MoveTemp(InInitializer));
}

void FEntityFactories::DefineMutuallyInclusiveComponent(FComponentTypeID InComponentA, FComponentTypeID InComponentB)
{
	MutualInclusivityGraph.AllocateNode(InComponentA.BitIndex());
	MutualInclusivityGraph.AllocateNode(InComponentB.BitIndex());
	MutualInclusivityGraph.MakeEdge(InComponentA.BitIndex(), InComponentB.BitIndex());
	Masks.AllMutualFirsts.Set(InComponentA);
}

void FEntityFactories::DefineMutuallyInclusiveComponent(TInlineValue<FMutualEntityInitializer>&& InInitializer)
{
	check(InInitializer.IsValid());

	DefineChildComponent(InInitializer->GetComponentA(), InInitializer->GetComponentB());
	// Note: after this line, InInitializer is reset
	MutualInitializers.Add(MoveTemp(InInitializer));
}

void FEntityFactories::DefineComplexInclusiveComponents(const FComplexInclusivityFilter& InFilter, FComponentTypeID InComponent)
{
	FComponentMask ComponentsToInclude { InComponent };
	FComplexInclusivity NewComplexInclusivity { InFilter, ComponentsToInclude };
	DefineComplexInclusiveComponents(NewComplexInclusivity);
}

void FEntityFactories::DefineComplexInclusiveComponents(const FComplexInclusivity& InInclusivity)
{
	ComplexInclusivity.Add(InInclusivity);
	Masks.AllComplexFirsts.CombineWithBitwiseOR(InInclusivity.Filter.Mask, EBitwiseOperatorFlags::MaxSize);
}

int32 FEntityFactories::ComputeChildComponents(const FComponentMask& ParentComponentMask, FComponentMask& ChildComponentMask)
{
	int32 NumNewComponents = 0;

	// Any child components keyed off an invalid parent component type are always relevant
	FComponentTypeID InvalidComponent = FComponentTypeID::Invalid();
	for (auto Child = ParentToChildComponentTypes.CreateConstKeyIterator(InvalidComponent); Child; ++Child)
	{
		if (!ChildComponentMask.Contains(Child.Value()))
		{
			ChildComponentMask.Set(Child.Value());
			++NumNewComponents;
		}
	}

	for (FComponentMaskIterator It = ParentComponentMask.Iterate(); It; ++It)
	{
		FComponentTypeID ParentComponent = FComponentTypeID::FromBitIndex(It.GetIndex());
		for (auto Child = ParentToChildComponentTypes.CreateConstKeyIterator(ParentComponent); Child; ++Child)
		{
			if (!ChildComponentMask.Contains(Child.Value()))
			{
				ChildComponentMask.Set(Child.Value());
				++NumNewComponents;
			}
		}
	}

	return NumNewComponents;
}

int32 FEntityFactories::ComputeMutuallyInclusiveComponents(FComponentMask& ComponentMask)
{
	int32 NumNewComponents = 0;

	// We have two things that can add components: filtered includes and mutual includes.
	//
	// Since a mutual include might add a component that will make a filter pass, and a passing filter
	// might add a component that has a mutual include, we need to loop over both until the whole
	// thing "stabilizes".
	//
	// To avoid always having to loop one extra time (with the last loop not doing anything), we check
	// if the previous loop added anything that can potentially make an additional loop useful. It won't
	// prevent doing a loop for nothing, but it will prevent it *most* of the time.
	//
	while (true)
	{
		int32 NumNewComponentsThisTime = 0;
		FComponentMask NewComponentsFromMutuals;

		// Complex includes.
		for (const FComplexInclusivity& Inclusivity : ComplexInclusivity)
		{
			if (Inclusivity.Filter.Match(ComponentMask))
			{
				// Only count the components that we are truly adding. Some of the components in ComponentsToInclude
				// could already be present in our mask, and wouldn't count as "new" here.
				const FComponentMask Added = FComponentMask::BitwiseAND(
						Inclusivity.ComponentsToInclude, FComponentMask::BitwiseNOT(ComponentMask),
						EBitwiseOperatorFlags::MaxSize);
				NumNewComponentsThisTime += Added.NumComponents();

				ComponentMask.CombineWithBitwiseOR(Inclusivity.ComponentsToInclude, EBitwiseOperatorFlags::MaxSize);
			}
		}

		// Mutual includes.
		FMovieSceneEntitySystemDirectedGraph::FBreadthFirstSearch BFS(&MutualInclusivityGraph);

		for (FComponentMaskIterator It = ComponentMask.Iterate(); It; ++It)
		{
			const uint16 NodeID = static_cast<uint16>(It.GetIndex());
			if (MutualInclusivityGraph.IsNodeAllocated(NodeID))
			{
				BFS.Search(NodeID);
			}
		}

		// Ideally would do a bitwise OR here
		for (TConstSetBitIterator<> It(BFS.GetVisited()); It; ++It)
		{
			FComponentTypeID ComponentType = FComponentTypeID::FromBitIndex(It.GetIndex());
			if (!ComponentMask.Contains(ComponentType))
			{
				NewComponentsFromMutuals.Set(ComponentType);
				++NumNewComponentsThisTime;

				ComponentMask.Set(ComponentType);
			}
		}

		// Accumulate our count of new components.
		NumNewComponents += NumNewComponentsThisTime;

		// We don't need to do another loop if:
		//
		// 1. We didn't add anything this loop... 
		//   OR
		// 2. We added something in the "mutuals" part that we know doesn't match
		//    any complex filter.
		if (
				(NumNewComponentsThisTime == 0) ||
				(!NewComponentsFromMutuals.ContainsAny(Masks.AllComplexFirsts))
			)
		{
			break;
		}
	}

	return NumNewComponents;
}

void FEntityFactories::RunInitializers(const FComponentMask& ParentType, const FComponentMask& ChildType, const FEntityAllocation* ParentAllocation, TArrayView<const int32> ParentAllocationOffsets, const FEntityRange& InChildEntityRange)
{
	// First off, run child initializers
	for (TInlineValue<FChildEntityInitializer>& ChildInit : ChildInitializers)
	{
		if (ChildInit->IsRelevant(ParentType, ChildType))
		{
			ChildInit->Run(InChildEntityRange, ParentAllocation, ParentAllocationOffsets);
		}
	}

	// First off, run child initializers
	for (TInlineValue<FMutualEntityInitializer>& MutualInit : MutualInitializers)
	{
		if (MutualInit->IsRelevant(ChildType))
		{
			MutualInit->Run(InChildEntityRange);
		}
	}
}

}	// using namespace MovieScene
}	// using namespace UE

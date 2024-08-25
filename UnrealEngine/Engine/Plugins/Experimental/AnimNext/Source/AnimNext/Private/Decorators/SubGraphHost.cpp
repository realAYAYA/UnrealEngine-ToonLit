// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/SubGraphHost.h"

#include "DecoratorBase/ExecutionContext.h"
#include "Graph/AnimNextGraphInstance.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FSubGraphHostDecorator)

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IDiscreteBlend) \
		GeneratorMacro(IGarbageCollection) \
		GeneratorMacro(IHierarchy) \
		GeneratorMacro(IUpdate) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FSubGraphHostDecorator, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR

	void FSubGraphHostDecorator::FInstanceData::Construct(const FExecutionContext& Context, const FDecoratorBinding& Binding)
	{
		FDecorator::FInstanceData::Construct(Context, Binding);

		IGarbageCollection::RegisterWithGC(Context, Binding);

		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		ReferencePoseChildPtr = Context.AllocateNodeInstance(Binding, SharedData->ReferencePoseChild);
	}

	void FSubGraphHostDecorator::FInstanceData::Destruct(const FExecutionContext& Context, const FDecoratorBinding& Binding)
	{
		FDecorator::FInstanceData::Destruct(Context, Binding);

		IGarbageCollection::UnregisterWithGC(Context, Binding);
	}

	uint32 FSubGraphHostDecorator::GetNumChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
		return InstanceData->SubGraphSlots.Num();
	}

	void FSubGraphHostDecorator::GetChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		for (const FSubGraphSlot& SubGraphEntry : InstanceData->SubGraphSlots)
		{
			if (SubGraphEntry.State == ESlotState::ActiveWithReferencePose)
			{
				Children.Add(InstanceData->ReferencePoseChildPtr);
			}
			else
			{
				// Even if the slot is inactive, we queue an empty handle
				Children.Add(SubGraphEntry.GraphInstance.GetGraphRootPtr());
			}
		}
	}

	void FSubGraphHostDecorator::PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		const bool bHasActiveSubGraph = InstanceData->CurrentlyActiveSubGraphIndex != INDEX_NONE;

		TObjectPtr<const UAnimNextGraph> CurrentActiveSubGraph;
		FName CurrentActiveEntryPoint = NAME_None;
		if (bHasActiveSubGraph)
		{
			const FSubGraphSlot& SubGraphSlot = InstanceData->SubGraphSlots[InstanceData->CurrentlyActiveSubGraphIndex];
			CurrentActiveSubGraph = SubGraphSlot.SubGraph;
			CurrentActiveEntryPoint = SubGraphSlot.EntryPoint;
		}

		const TObjectPtr<const UAnimNextGraph> DesiredSubGraph = SharedData->GetSubGraph(Context, Binding);
		const FName EntryPoint = SharedData->GetEntryPoint(Context, Binding);

		// Check for reentrancy and early-out if we are linking back to the current instance
		FAnimNextGraphInstance& GraphInstance = Context.GetGraphInstance();
		if(GraphInstance.UsesGraph(DesiredSubGraph) && GraphInstance.UsesEntryPoint(EntryPoint))
		{
			return;
		}

		if (!bHasActiveSubGraph || CurrentActiveSubGraph != DesiredSubGraph || CurrentActiveEntryPoint != EntryPoint)
		{
			// Find an empty slot we can use
			int32 FreeSlotIndex = INDEX_NONE;

			const int32 NumSubGraphSlots = InstanceData->SubGraphSlots.Num();
			for (int32 SlotIndex = 0; SlotIndex < NumSubGraphSlots; ++SlotIndex)
			{
				if (InstanceData->SubGraphSlots[SlotIndex].State == ESlotState::Inactive)
				{
					// This slot is inactive, we can re-use it
					FreeSlotIndex = SlotIndex;
					break;
				}
			}

			if (FreeSlotIndex == INDEX_NONE)
			{
				// All slots are in use, add a new one
				FreeSlotIndex = InstanceData->SubGraphSlots.AddDefaulted();
			}

			FSubGraphSlot& SubGraphSlot = InstanceData->SubGraphSlots[FreeSlotIndex];
			SubGraphSlot.SubGraph = DesiredSubGraph;
			SubGraphSlot.State = DesiredSubGraph ? ESlotState::ActiveWithGraph : ESlotState::ActiveWithReferencePose;
			SubGraphSlot.EntryPoint = EntryPoint;

			const int32 OldChildIndex = InstanceData->CurrentlyActiveSubGraphIndex;
			const int32 NewChildIndex = FreeSlotIndex;

			InstanceData->CurrentlyActiveSubGraphIndex = FreeSlotIndex;

			TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
			Context.GetInterface(Binding, DiscreteBlendDecorator);

			DiscreteBlendDecorator.OnBlendTransition(Context, OldChildIndex, NewChildIndex);
		}
	}

	void FSubGraphHostDecorator::QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState, FUpdateTraversalQueue& TraversalQueue) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		const int32 NumSubGraphs = InstanceData->SubGraphSlots.Num();
		if (NumSubGraphs == 0)
		{
			return;
		}

		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		for (int32 SubGraphIndex = 0; SubGraphIndex < NumSubGraphs; ++SubGraphIndex)
		{
			const float BlendWeight = DiscreteBlendDecorator.GetBlendWeight(Context, SubGraphIndex);

			FDecoratorUpdateState SubGraphDecoratorState = DecoratorState.WithWeight(BlendWeight);
			if (SubGraphIndex != InstanceData->CurrentlyActiveSubGraphIndex)
			{
				SubGraphDecoratorState = SubGraphDecoratorState.AsBlendingOut();
			}

			TraversalQueue.Push(InstanceData->SubGraphSlots[SubGraphIndex].GraphInstance.GetGraphRootPtr(), SubGraphDecoratorState);
		}
	}

	float FSubGraphHostDecorator::GetBlendWeight(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (ChildIndex == InstanceData->CurrentlyActiveSubGraphIndex)
		{
			return 1.0f;	// Active child has full weight
		}
		else if (InstanceData->SubGraphSlots.IsValidIndex(ChildIndex))
		{
			return 0.0f;	// Other children have no weight
		}
		else
		{
			// Invalid child index
			return -1.0f;
		}
	}

	int32 FSubGraphHostDecorator::GetBlendDestinationChildIndex(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		return InstanceData->CurrentlyActiveSubGraphIndex;
	}

	void FSubGraphHostDecorator::OnBlendTransition(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		// We initiate immediately when we transition
		DiscreteBlendDecorator.OnBlendInitiated(Context, NewChildIndex);

		// We terminate immediately when we transition
		DiscreteBlendDecorator.OnBlendTerminated(Context, OldChildIndex);
	}

	void FSubGraphHostDecorator::OnBlendInitiated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (InstanceData->SubGraphSlots.IsValidIndex(ChildIndex))
		{
			// Allocate our new sub-graph instance
			FSubGraphSlot& SubGraphEntry = InstanceData->SubGraphSlots[ChildIndex];

			if (SubGraphEntry.State == ESlotState::ActiveWithGraph)
			{
				SubGraphEntry.SubGraph->AllocateInstance(Context.GetGraphInstance(), SubGraphEntry.GraphInstance, SubGraphEntry.EntryPoint);
			}
		}
	}

	void FSubGraphHostDecorator::OnBlendTerminated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (InstanceData->SubGraphSlots.IsValidIndex(ChildIndex))
		{
			// Deallocate our sub-graph instance
			FSubGraphSlot& SubGraphEntry = InstanceData->SubGraphSlots[ChildIndex];

			if (SubGraphEntry.State == ESlotState::ActiveWithGraph)
			{
				InstanceData->SubGraphSlots[ChildIndex].GraphInstance.Release();
			}

			SubGraphEntry.State = ESlotState::Inactive;
		}
	}

	void FSubGraphHostDecorator::AddReferencedObjects(const FExecutionContext& Context, const TDecoratorBinding<IGarbageCollection>& Binding, FReferenceCollector& Collector) const
	{
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		for (FSubGraphSlot& SubGraphEntry : InstanceData->SubGraphSlots)
		{
			Collector.AddPropertyReferencesWithStructARO(FAnimNextGraphInstancePtr::StaticStruct(), &SubGraphEntry.GraphInstance);
		}
	}
}

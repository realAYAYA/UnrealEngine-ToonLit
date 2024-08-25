// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/BlendByBool.h"

#include "Animation/AnimTypes.h"
#include "DecoratorBase/ExecutionContext.h"
#include "EvaluationVM/Tasks/BlendKeyframes.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FBlendByBoolDecorator)

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IDiscreteBlend) \
		GeneratorMacro(IHierarchy) \
		GeneratorMacro(IUpdate) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FBlendByBoolDecorator, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR

	static constexpr int32 TRUE_CHILD_INDEX = 0;
	static constexpr int32 FALSE_CHILD_INDEX = 1;

	void FBlendByBoolDecorator::PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		const int32 DestinationChildIndex = DiscreteBlendDecorator.GetBlendDestinationChildIndex(Context);
		if (InstanceData->PreviousChildIndex != DestinationChildIndex)
		{
			DiscreteBlendDecorator.OnBlendTransition(Context, InstanceData->PreviousChildIndex, DestinationChildIndex);

			InstanceData->PreviousChildIndex = DestinationChildIndex;
		}
	}

	void FBlendByBoolDecorator::QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState, FUpdateTraversalQueue& TraversalQueue) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// The destination child index has been updated in PreUpdate, we can use the cached version
		const int32 DestinationChildIndex = InstanceData->PreviousChildIndex;

		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		const float BlendWeightTrue = DiscreteBlendDecorator.GetBlendWeight(Context, TRUE_CHILD_INDEX);
		if (InstanceData->TrueChild.IsValid() && FAnimWeight::IsRelevant(BlendWeightTrue))
		{
			FDecoratorUpdateState DecoratorStateTrue = DecoratorState.WithWeight(BlendWeightTrue);
			if (DestinationChildIndex != TRUE_CHILD_INDEX)
			{
				DecoratorStateTrue = DecoratorStateTrue.AsBlendingOut();
			}

			TraversalQueue.Push(InstanceData->TrueChild, DecoratorStateTrue);
		}

		const float BlendWeightFalse = 1.0f - BlendWeightTrue;
		if (InstanceData->FalseChild.IsValid() && FAnimWeight::IsRelevant(BlendWeightFalse))
		{
			FDecoratorUpdateState DecoratorStateFalse = DecoratorState.WithWeight(BlendWeightFalse);
			if (DestinationChildIndex != FALSE_CHILD_INDEX)
			{
				DecoratorStateFalse = DecoratorStateFalse.AsBlendingOut();
			}

			TraversalQueue.Push(InstanceData->FalseChild, DecoratorStateFalse);
		}
	}

	uint32 FBlendByBoolDecorator::GetNumChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding) const
	{
		return 2;
	}

	void FBlendByBoolDecorator::GetChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// Add the two children, even if the handles are empty
		Children.Add(InstanceData->TrueChild);
		Children.Add(InstanceData->FalseChild);
	}

	float FBlendByBoolDecorator::GetBlendWeight(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		const float DestinationChildIndex = DiscreteBlendDecorator.GetBlendDestinationChildIndex(Context);

		if (ChildIndex == TRUE_CHILD_INDEX)
		{
			return (DestinationChildIndex == TRUE_CHILD_INDEX) ? 1.0f : 0.0f;
		}
		else if (ChildIndex == FALSE_CHILD_INDEX)
		{
			return (DestinationChildIndex == FALSE_CHILD_INDEX) ? 1.0f : 0.0f;
		}
		else
		{
			// Invalid child index
			return -1.0f;
		}
	}

	int32 FBlendByBoolDecorator::GetBlendDestinationChildIndex(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();

		const bool bCondition = SharedData->GetbCondition(Context, Binding);
		return bCondition ? TRUE_CHILD_INDEX : FALSE_CHILD_INDEX;
	}

	void FBlendByBoolDecorator::OnBlendTransition(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		// We initiate immediately when we transition
		DiscreteBlendDecorator.OnBlendInitiated(Context, NewChildIndex);

		// We terminate immediately when we transition
		DiscreteBlendDecorator.OnBlendTerminated(Context, OldChildIndex);
	}

	void FBlendByBoolDecorator::OnBlendInitiated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// Allocate our new child instance
		if (ChildIndex == TRUE_CHILD_INDEX)
		{
			if (!InstanceData->TrueChild.IsValid())
			{
				InstanceData->TrueChild = Context.AllocateNodeInstance(Binding, SharedData->TrueChild);
			}
		}
		else if (ChildIndex == FALSE_CHILD_INDEX)
		{
			if (!InstanceData->FalseChild.IsValid())
			{
				InstanceData->FalseChild = Context.AllocateNodeInstance(Binding, SharedData->FalseChild);
			}
		}
	}

	void FBlendByBoolDecorator::OnBlendTerminated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// Deallocate our child instance
		if (ChildIndex == TRUE_CHILD_INDEX)
		{
			InstanceData->TrueChild.Reset();
		}
		else if (ChildIndex == FALSE_CHILD_INDEX)
		{
			InstanceData->FalseChild.Reset();
		}
	}
}

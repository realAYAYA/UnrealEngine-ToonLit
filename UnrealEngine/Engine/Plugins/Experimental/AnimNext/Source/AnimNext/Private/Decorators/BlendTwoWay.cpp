// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/BlendTwoWay.h"

#include "Animation/AnimTypes.h"
#include "DecoratorBase/ExecutionContext.h"
#include "EvaluationVM/Tasks/BlendKeyframes.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FBlendTwoWayDecorator)

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IContinuousBlend) \
		GeneratorMacro(IEvaluate) \
		GeneratorMacro(IHierarchy) \
		GeneratorMacro(IUpdate) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FBlendTwoWayDecorator, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR

	void FBlendTwoWayDecorator::PostEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (InstanceData->ChildA.IsValid() && InstanceData->ChildB.IsValid())
		{
			// We have two children, interpolate them

			TDecoratorBinding<IContinuousBlend> ContinuousBlendDecorator;
			Context.GetInterface(Binding, ContinuousBlendDecorator);

			const float BlendWeight = ContinuousBlendDecorator.GetBlendWeight(Context, 1);
			Context.AppendTask(FAnimNextBlendTwoKeyframesTask::Make(BlendWeight));
		}
		else
		{
			// We have only one child that is active, do nothing
		}
	}

	void FBlendTwoWayDecorator::PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TDecoratorBinding<IContinuousBlend> ContinuousBlendDecorator;
		Context.GetInterface(Binding, ContinuousBlendDecorator);

		const float BlendWeightB = ContinuousBlendDecorator.GetBlendWeight(Context, 1);
		if (!FAnimWeight::IsFullWeight(BlendWeightB))
		{
			if (!InstanceData->ChildA.IsValid())
			{
				// We need to blend a child that isn't instanced yet, allocate it
				InstanceData->ChildA = Context.AllocateNodeInstance(Binding, SharedData->ChildA);
			}

			if (!FAnimWeight::IsRelevant(BlendWeightB))
			{
				// We no longer need this child, release it
				InstanceData->ChildB.Reset();
			}
		}

		if (FAnimWeight::IsRelevant(BlendWeightB))
		{
			if (!InstanceData->ChildB.IsValid())
			{
				// We need to blend a child that isn't instanced yet, allocate it
				InstanceData->ChildB = Context.AllocateNodeInstance(Binding, SharedData->ChildB);
			}

			if (FAnimWeight::IsFullWeight(BlendWeightB))
			{
				// We no longer need this child, release it
				InstanceData->ChildA.Reset();
			}
		}
	}

	void FBlendTwoWayDecorator::QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState, FUpdateTraversalQueue& TraversalQueue) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TDecoratorBinding<IContinuousBlend> ContinuousBlendDecorator;
		Context.GetInterface(Binding, ContinuousBlendDecorator);

		const float BlendWeightB = ContinuousBlendDecorator.GetBlendWeight(Context, 1);
		if (InstanceData->ChildA.IsValid())
		{
			const float BlendWeightA = 1.0f - BlendWeightB;
			TraversalQueue.Push(InstanceData->ChildA, DecoratorState.WithWeight(BlendWeightA));
		}

		if (InstanceData->ChildB.IsValid())
		{
			TraversalQueue.Push(InstanceData->ChildB, DecoratorState.WithWeight(BlendWeightB));
		}
	}

	uint32 FBlendTwoWayDecorator::GetNumChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding) const
	{
		return 2;
	}

	void FBlendTwoWayDecorator::GetChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// Add the two children, even if the handles are empty
		Children.Add(InstanceData->ChildA);
		Children.Add(InstanceData->ChildB);
	}

	float FBlendTwoWayDecorator::GetBlendWeight(const FExecutionContext& Context, const TDecoratorBinding<IContinuousBlend>& Binding, int32 ChildIndex) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();

		const float BlendWeight = SharedData->GetBlendWeight(Context, Binding);
		const float ClampedWeight = FMath::Clamp(BlendWeight, 0.0f, 1.0f);

		if (ChildIndex == 0)
		{
			return 1.0f - ClampedWeight;
		}
		else if (ChildIndex == 1)
		{
			return ClampedWeight;
		}
		else
		{
			// Invalid child index
			return -1.0f;
		}
	}
}

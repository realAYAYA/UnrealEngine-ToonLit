// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/SequencePlayer.h"

#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	DEFINE_ANIM_DECORATOR_BEGIN(FSequencePlayerDecorator)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IEvaluate)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IUpdate)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(ITimeline)
	DEFINE_ANIM_DECORATOR_END(FSequencePlayerDecorator)

	void FSequencePlayerDecorator::PreEvaluate(FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		// TODO: Sample pose
		// FPose foo = ...
		// Context.PushPose(foo);
	}

	double FSequencePlayerDecorator::GetPlayRate(FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		return SharedData->PlayRate;
	}

	void FSequencePlayerDecorator::PreUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TDecoratorBinding<ITimeline> TimelineDecorator;
		Context.GetInterface(Binding, TimelineDecorator);

		FUpdateTraversalContext& TraversalContext = Context.GetTraversalContext<FUpdateTraversalContext>();

		double DeltaTime = TraversalContext.GetDeltaTime();

		// Combine the execution play rate along with the timeline play rate, possibly overriden by another decorator
		double PlayRate = TraversalContext.GetPlayRate() * TimelineDecorator.GetPlayRate(Context);

		// TODO: need an actual impl, for illustrative purposes
		InstanceData->CurrentTime += DeltaTime * PlayRate;
	}
}

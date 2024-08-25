// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/ReferencePoseDecorator.h"

#include "DecoratorBase/ExecutionContext.h"
#include "EvaluationVM/Tasks/PushReferenceKeyframe.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FReferencePoseDecorator)

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IEvaluate) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FReferencePoseDecorator, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR

	void FReferencePoseDecorator::PreEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();

		FAnimNextPushReferenceKeyframeTask Task;
		Task.bIsAdditive = SharedData->ReferencePoseType == EAnimNextReferencePoseType::AdditiveIdentity;

		Context.AppendTask(Task);
	}
}

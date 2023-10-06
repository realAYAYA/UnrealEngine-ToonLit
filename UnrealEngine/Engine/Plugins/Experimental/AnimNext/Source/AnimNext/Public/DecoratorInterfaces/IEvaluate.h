// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/EvaluationFlags.h"
#include "DecoratorBase/IDecoratorInterface.h"
#include "DecoratorBase/ITraversalContext.h"

namespace UE::AnimNext
{
	// TODO: Hook up our real pose container
	struct FPoseContainer final {};

	/**
	 * FEvaluateTraversalContext
	 *
	 * Contains all relevant transient data for an evaluate traversal.
	 */
	struct ANIMNEXT_API FEvaluateTraversalContext : ITraversalContext
	{
		explicit FEvaluateTraversalContext(EEvaluationFlags EvaluationFlags_);

		EEvaluationFlags GetEvaluationFlags() const { return EvaluationFlags; }

		//void PushPose(FPose&& Pose);
		//FPose&& PopPose();

	private:
		EEvaluationFlags EvaluationFlags;
	};

	/**
	 * IEvaluate
	 * 
	 * This interface is called during the evaluation traversal. It aims to produce some
	 * of the following: an output pose, its velocity, the root trajectory, scalar curves, attributes, etc.
	 * 
	 * When a node is decorator is visited, PreEvaluate is first called on it. It is responsible for forwarding
	 * the call to the next decorator that implements this interface on the decorator stack of the node. Once
	 * all decorators have had the chance to PreEvaluate, the children of the decorator are queried through
	 * the IHierarchy interface. The children will then evaluate and PostEvaluate will then be called afterwards
	 * on the original decorator.
	 * 
	 * The execution context contains what to evaluate.
	 * @see EEvaluationFlags
	 */
	struct ANIMNEXT_API IEvaluate : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IEvaluate, 0xa303e9e7)

		// Called before a decorator's children are evaluated
		virtual void PreEvaluate(FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const;

		// Called after a decorator's children have been evaluated
		virtual void PostEvaluate(FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<IEvaluate> : FDecoratorBinding
	{
		// @see IEvaluate::PreEvaluate
		void PreEvaluate(FExecutionContext& Context) const
		{
			GetInterface()->PreEvaluate(Context, *this);
		}

		// @see IEvaluate::PostEvaluate
		void PostEvaluate(FExecutionContext& Context) const
		{
			GetInterface()->PostEvaluate(Context, *this);
		}

	protected:
		const IEvaluate* GetInterface() const { return GetInterfaceTyped<IEvaluate>(); }
	};

	/**
	 * Evaluates a sub-graph starting at the graph root and produces its result.
	 * Evaluation starts at the top of the stack that includes the graph root decorator.
	 *
	 * For each node:
	 *     - We call PreEvaluate on all its decorators
	 *     - We call GetChildren on all its decorators
	 *     - We evaluate all children found
	 *     - We call PostEvaluate on all its decorators
	 *
	 * @see IEvaluate::PreEvaluate, IEvaluate::PostEvaluate, IHierarchy::GetChildren
	 */
	ANIMNEXT_API void EvaluateGraph(FExecutionContext& Context, FWeakDecoratorPtr GraphRootPtr, EEvaluationFlags EvaluationFlags, FPoseContainer& OutContainer);
}

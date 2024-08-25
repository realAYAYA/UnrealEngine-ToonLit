// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/ExecutionContextProxy.h"
#include "DecoratorBase/IDecoratorInterface.h"
#include "EvaluationVM/EvaluationProgram.h"
#include "EvaluationVM/KeyframeState.h"

namespace UE::AnimNext
{
	/**
	 * FEvaluateTraversalContext
	 *
	 * Contains all relevant transient data for an evaluate traversal and wraps the execution context.
	 */
	struct FEvaluateTraversalContext final : FExecutionContextProxy
	{
		// Appends a new task into the evaluation program, tasks mutate state in the order they have been appended in
		// This means that child nodes need to evaluate first, tasks will usually be appended in IEvaluate::PostEvaluate
		// Tasks are moved into their final memory location, caller can allocate the task anywhere, it is no longer needed after this operation
		// @see FEvaluationProgram, FEvaluationTask, FEvaluationVM
		template<class TaskType>
		void AppendTask(TaskType&& Task) { EvaluationProgram.AppendTask(MoveTemp(Task)); }

	private:
		FEvaluateTraversalContext(const FExecutionContext& InExecutionContext, FEvaluationProgram& InEvaluationProgram);

		FEvaluationProgram& EvaluationProgram;

		friend ANIMNEXT_API FEvaluationProgram EvaluateGraph(const FWeakDecoratorPtr& GraphRootPtr);
	};

	/**
	 * IEvaluate
	 * 
	 * This interface is called during the evaluation traversal. It aims to produce an evaluation program.
	 * 
	 * When a node is visited, PreEvaluate is first called on its top decorator. It is responsible for forwarding
	 * the call to the next decorator that implements this interface on the decorator stack of the node. Once
	 * all decorators have had the chance to PreEvaluate, the children of the decorator are queried through
	 * the IHierarchy interface. The children will then evaluate and PostEvaluate will then be called afterwards
	 * on the original decorator.
	 * 
	 * The execution context contains what to evaluate.
	 * @see FEvaluationProgram
	 */
	struct ANIMNEXT_API IEvaluate : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IEvaluate, 0xa303e9e7)

		// Called before a decorator's children are evaluated
		virtual void PreEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const;

		// Called after a decorator's children have been evaluated
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<IEvaluate> : FDecoratorBinding
	{
		// @see IEvaluate::PreEvaluate
		void PreEvaluate(FEvaluateTraversalContext& Context) const
		{
			GetInterface()->PreEvaluate(Context, *this);
		}

		// @see IEvaluate::PostEvaluate
		void PostEvaluate(FEvaluateTraversalContext& Context) const
		{
			GetInterface()->PostEvaluate(Context, *this);
		}

	protected:
		const IEvaluate* GetInterface() const { return GetInterfaceTyped<IEvaluate>(); }
	};

	/**
	 * Evaluates a sub-graph starting at its root and produces an evaluation program.
	 * Evaluation should be deterministic and repeated calls should yield the same evaluation program.
	 *
	 * For each node:
	 *     - We call PreEvaluate on all its decorators
	 *     - We call GetChildren on all its decorators
	 *     - We evaluate all children found
	 *     - We call PostEvaluate on all its decorators
	 *
	 * @see IEvaluate::PreEvaluate, IEvaluate::PostEvaluate, IHierarchy::GetChildren
	 */
	[[nodiscard]] ANIMNEXT_API FEvaluationProgram EvaluateGraph(FAnimNextGraphInstancePtr& GraphInstance);

	/**
	 * Evaluates a sub-graph starting at its root and produces an evaluation program.
	 * Evaluation starts at the top of the stack that includes the graph root decorator.
	 * Evaluation should be deterministic and repeated calls should yield the same evaluation program.
	 *
	 * For each node:
	 *     - We call PreEvaluate on all its decorators
	 *     - We call GetChildren on all its decorators
	 *     - We evaluate all children found
	 *     - We call PostEvaluate on all its decorators
	 *
	 * @see IEvaluate::PreEvaluate, IEvaluate::PostEvaluate, IHierarchy::GetChildren
	 */
	[[nodiscard]] ANIMNEXT_API FEvaluationProgram EvaluateGraph(const FWeakDecoratorPtr& GraphRootPtr);
}

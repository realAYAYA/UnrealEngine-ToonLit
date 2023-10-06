// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::AnimNext
{
	struct FExecutionContext;

	/**
	 * ITraversalContext
	 * 
	 * Base type for all traversal context derived types.
	 * A traversal context is a transient object that lives only during a particular
	 * traversal. It can be access through the FExecutionContext.
	 * 
	 * @see FExecutionContext
	 */
	struct ITraversalContext
	{
		virtual ~ITraversalContext() {}
	};

	/**
	 * FScopedTraversalContext
	 * 
	 * Pushes and pops a traversal context onto the execution context.
	 * During the lifetime of an instance of this object, the execution context will
	 * return the provided traversal context as the current traversal context.
	 * 
	 * @see FExecutionContext
	 */
	struct ANIMNEXT_API FScopedTraversalContext final
	{
		FScopedTraversalContext(FExecutionContext& ExecutionContext, ITraversalContext& TraversalContext);

		FScopedTraversalContext(const FScopedTraversalContext&) = delete;
		FScopedTraversalContext& operator=(const FScopedTraversalContext&) = delete;

		~FScopedTraversalContext();

	private:
		FExecutionContext& ExecutionContext;
		ITraversalContext* OldTraversalContext;
	};
}

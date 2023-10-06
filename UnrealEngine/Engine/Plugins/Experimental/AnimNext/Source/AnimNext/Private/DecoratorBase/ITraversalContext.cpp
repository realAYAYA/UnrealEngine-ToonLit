// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/ITraversalContext.h"

#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	FScopedTraversalContext::FScopedTraversalContext(FExecutionContext& ExecutionContext_, ITraversalContext& TraversalContext)
		: ExecutionContext(ExecutionContext_)
		, OldTraversalContext(ExecutionContext_.TraversalContext)
	{
		ExecutionContext_.TraversalContext = &TraversalContext;
	}

	FScopedTraversalContext::~FScopedTraversalContext()
	{
		ExecutionContext.TraversalContext = OldTraversalContext;
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionLayer.h"
#include "Templates/SharedPointer.h"

struct FAvaTransitionLayerComparator;

/**
 * Responsible for execution of Transition Behaviors and signaling when these Behaviors have completed.
 * NOTE: The Executor shared reference should be kept alive until it completes the behaviors
 */
class IAvaTransitionExecutor : public TSharedFromThis<IAvaTransitionExecutor>
{
public:
	virtual ~IAvaTransitionExecutor() = default;

	/** Gets all the Behavior Instances matching the Layer Comparator */
	virtual TArray<const FAvaTransitionBehaviorInstance*> GetBehaviorInstances(const FAvaTransitionLayerComparator& InComparator) const = 0;

	/** Start Transition. Should only be called once */
	virtual void Start() = 0;

	/** Stop the current Execution */
	virtual void Stop() = 0;
};

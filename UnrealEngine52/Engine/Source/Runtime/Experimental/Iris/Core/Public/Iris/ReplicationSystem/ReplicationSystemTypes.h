// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

namespace UE::Net
{

enum class ENetObjectDeltaCompressionStatus : unsigned
{
	Disallow,
	Allow,
};

enum class EDependentObjectSchedulingHint : uint8
{
	// Default behavior, dependent object will be scheduled to replicate if parent is replicated, if the dependent object has not yet been replicated it will be replicated in the same batch as the parent
	Default = 0,

	// Dependent object will be scheduled to replicate before parent is replicated, if the dependent has data to send and has not yet been replicated the parent will only be scheduled if they both fit in same packet
	ScheduleBeforeParent,

	// Not yet replicated dependent object will behave as ReplicateBeforeParent otherwise it will be scheduled to replicate if the parent is replicated and scheduled after the parent
	ScheduleBeforeParentIfInitialState,
};

}

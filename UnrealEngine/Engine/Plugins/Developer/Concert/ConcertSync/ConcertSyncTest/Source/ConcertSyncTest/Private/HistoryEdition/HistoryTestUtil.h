// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HistoryEdition/ActivityGraphIDs.h"
#include "HistoryEdition/ActivityNode.h"
#include "Misc/AutomationTest.h"

namespace UE::ConcertSyncTests
{
	TArray<ConcertSyncCore::FActivityNodeID> ValidateEachActivityHasNode(
		FAutomationTestBase& Test,
		const TArray<int64>& ActivityMappings,
		const ConcertSyncCore::FActivityDependencyGraph& Graph,
		uint32 ActivityCount,
		TFunctionRef<FString(uint32 ActivityType)> LexToString)
	{
		using namespace ConcertSyncCore;
		
		TArray<FActivityNodeID> ActivityNodes;
		ActivityNodes.SetNumZeroed(ActivityCount);
		for (uint32 ActivityType = 0; ActivityType < ActivityCount; ++ActivityType)
		{
			const int64 ActivityId = ActivityMappings[ActivityType];
			const TOptional<FActivityNodeID> NodeID = Graph.FindNodeByActivity(ActivityId);
			if (!NodeID.IsSet())
			{
				Test.AddError(FString::Printf(TEXT("No node generated for activity type %s"), *LexToString(ActivityType)));
				continue;
			}
			
			if (!NodeID.IsSet())
			{
				Test.AddError(FString::Printf(TEXT("Graph has invalid state. Node ID %lld is invalid for activity type %s"), NodeID->ID, *LexToString(ActivityType)));
				continue;
			}
			
			ActivityNodes[ActivityType] = *NodeID;
		}

		return ActivityNodes;
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/PlayerStateCountLimiterConfig.h"

UPlayerStateCountLimiterConfig::UPlayerStateCountLimiterConfig()
: Super()
{
#if UE_WITH_IRIS
	Mode = ENetObjectCountLimiterMode::Fill;
	MaxObjectCount = 2;
	Priority = 1.0f;
	// We want the owning connection's player state to have high priority. 
	OwningConnectionPriority = 2.0f;
	bEnableOwnedObjectsFastLane = true;
#endif
}

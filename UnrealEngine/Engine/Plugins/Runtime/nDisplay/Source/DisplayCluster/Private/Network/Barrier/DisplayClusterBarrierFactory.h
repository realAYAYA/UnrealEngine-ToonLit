// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IDisplayClusterBarrier;


/**
 * Barrier factory
 */
class FDisplayClusterBarrierFactory
{
private:
	FDisplayClusterBarrierFactory();

public:
	// Factory method to instantiate a barrier based on the user settings
	static IDisplayClusterBarrier* CreateBarrier(const FString& BarrierId, const TArray<FString>& ThreadMarkers, const uint32 Timeout);
};

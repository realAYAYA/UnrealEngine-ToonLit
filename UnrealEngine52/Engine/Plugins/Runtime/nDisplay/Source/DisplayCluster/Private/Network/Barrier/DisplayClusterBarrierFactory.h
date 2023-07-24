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
	static TUniquePtr<IDisplayClusterBarrier> CreateBarrier(const TArray<FString>& ThreadMarkers, const uint32 Timeout, const FString& Name);
};

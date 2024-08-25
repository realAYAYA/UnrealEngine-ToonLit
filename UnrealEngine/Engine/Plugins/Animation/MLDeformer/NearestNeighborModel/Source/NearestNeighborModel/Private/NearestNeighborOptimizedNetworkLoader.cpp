// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborOptimizedNetworkLoader.h"
#include "NearestNeighborOptimizedNetwork.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void UNearestNeighborOptimizedNetworkLoader::SetOptimizedNetwork(UNearestNeighborOptimizedNetwork* InNetwork)
{
	Network = InNetwork;
}

UNearestNeighborOptimizedNetwork* UNearestNeighborOptimizedNetworkLoader::GetOptimizedNetwork() const
{
	return Network;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

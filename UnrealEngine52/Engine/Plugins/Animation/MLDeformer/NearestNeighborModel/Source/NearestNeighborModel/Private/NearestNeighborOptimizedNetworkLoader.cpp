// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborOptimizedNetworkLoader.h"
#include "NearestNeighborOptimizedNetwork.h"

void UNearestNeighborOptimizedNetworkLoader::SetOptimizedNetwork(UNearestNeighborOptimizedNetwork* InNetwork)
{
	Network = InNetwork;
}

UNearestNeighborOptimizedNetwork* UNearestNeighborOptimizedNetworkLoader::GetOptimizedNetwork() const
{
	return Network;
}

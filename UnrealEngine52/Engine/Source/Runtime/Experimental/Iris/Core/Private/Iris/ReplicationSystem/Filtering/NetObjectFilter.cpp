// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"

FNetObjectFilteringParams::FNetObjectFilteringParams(const UE::Net::FNetBitArrayView InFilteredObjects)
: FilteredObjects(InFilteredObjects)
, FilteringInfos(nullptr)
, StateBuffers(nullptr)
, ConnectionId(0)
{
}

UNetObjectFilter::UNetObjectFilter()
{
}

void UNetObjectFilter::AddConnection(uint32 ConnectionId)
{
}

void UNetObjectFilter::RemoveConnection(uint32 ConnectionId)
{
}

void UNetObjectFilter::PreFilter(FNetObjectPreFilteringParams&)
{
}

void UNetObjectFilter::Filter(FNetObjectFilteringParams&)
{
}

void UNetObjectFilter::PostFilter(FNetObjectPostFilteringParams&)
{
}

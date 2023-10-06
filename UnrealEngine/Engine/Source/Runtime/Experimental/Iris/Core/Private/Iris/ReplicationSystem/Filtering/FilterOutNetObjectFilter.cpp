// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/Filtering/FilterOutNetObjectFilter.h"

void UFilterOutNetObjectFilter::OnInit(FNetObjectFilterInitParams& Params)
{
}

bool UFilterOutNetObjectFilter::AddObject(uint32 ObjectIndex, FNetObjectFilterAddObjectParams& Params)
{
	return true;
}

void UFilterOutNetObjectFilter::RemoveObject(uint32 ObjectIndex, const FNetObjectFilteringInfo& Info)
{
}

void UFilterOutNetObjectFilter::UpdateObjects(FNetObjectFilterUpdateParams& Params)
{
}

void UFilterOutNetObjectFilter::Filter(FNetObjectFilteringParams& Params)
{
	// Filter out everything
	Params.OutAllowedObjects.Reset();
}

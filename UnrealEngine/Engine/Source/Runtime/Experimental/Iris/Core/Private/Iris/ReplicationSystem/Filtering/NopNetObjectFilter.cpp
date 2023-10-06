// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/Filtering/NopNetObjectFilter.h"

void UNopNetObjectFilter::OnInit(FNetObjectFilterInitParams& Params)
{
}

bool UNopNetObjectFilter::AddObject(uint32 ObjectIndex, FNetObjectFilterAddObjectParams& Params)
{
	return true;
}

void UNopNetObjectFilter::RemoveObject(uint32 ObjectIndex, const FNetObjectFilteringInfo& Info)
{
}

void UNopNetObjectFilter::UpdateObjects(FNetObjectFilterUpdateParams& Params)
{
}

void UNopNetObjectFilter::Filter(FNetObjectFilteringParams& Params)
{
	// Allow all objects to be replicated, just as if no dynamic filtering was applied.
	Params.OutAllowedObjects.SetAllBits();
}

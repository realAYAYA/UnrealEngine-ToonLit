// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/ReplicationSystem/Filtering/ReplicationFiltering.h"

FNetObjectFilteringParams::FNetObjectFilteringParams(const UE::Net::FNetBitArrayView InFilteredObjects)
: FilteredObjects(InFilteredObjects)
, FilteringInfos(nullptr)
, ConnectionId(0)
{
}

FNetObjectPreFilteringParams::FNetObjectPreFilteringParams(const UE::Net::FNetBitArrayView InFilteredObjects)
: FilteredObjects(InFilteredObjects)
{
}

UNetObjectFilter::UNetObjectFilter()
{
}

void UNetObjectFilter::Init(FNetObjectFilterInitParams& Params)
{
	if (Params.Config)
	{
		FilterType = Params.Config->FilterType;
	}

	{
		UE::Net::Private::FNetObjectFilteringInfoAccessor FilteringInfoAccessor;
		TArrayView<FNetObjectFilteringInfo> NetObjectFilteringInfos = FilteringInfoAccessor.GetNetObjectFilteringInfos(Params.ReplicationSystem);
		FilterInfo = FFilterInfo(Params.FilteredObjects, NetObjectFilteringInfos);
	}

	OnInit(Params);
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

// FFilterInfo implementation
UNetObjectFilter::FFilterInfo::FFilterInfo(const UE::Net::FNetBitArrayView InFilteredObjects, const TArrayView<FNetObjectFilteringInfo> InFilteringInfos)
: FilteredObjects(InFilteredObjects)
, FilteringInfos(InFilteringInfos)
{
}

UNetObjectFilter::FFilterInfo& UNetObjectFilter::FFilterInfo::operator=(const UNetObjectFilter::FFilterInfo& Other)
{
	// Re-initialize this instance.
	this->~FFilterInfo();
	new (this) FFilterInfo(Other.FilteredObjects, Other.FilteringInfos);
	return *this;
}

FNetObjectFilteringInfo* UNetObjectFilter::FFilterInfo::GetFilteringInfo(uint32 ObjectIndex)
{
	// Only allow retriving infos for objects handled by this instance.
	if (!IsAddedToFilter(ObjectIndex))
	{
		return nullptr;
	}

	return &FilteringInfos[ObjectIndex];
}


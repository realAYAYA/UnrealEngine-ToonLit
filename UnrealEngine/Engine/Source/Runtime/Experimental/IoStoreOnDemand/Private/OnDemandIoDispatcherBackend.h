// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoDispatcherBackend.h"
#include "Templates/SharedPointer.h"

#define UE_API IOSTOREONDEMAND_API

class IIoCache;

namespace UE
{

enum class EOnDemandEndpointType
{
	CDN = 1,
	ZEN
};

struct FOnDemandEndpoint
{
	EOnDemandEndpointType EndpointType;
	FString DistributionUrl;
	FString ServiceUrl;
	FString TocPath;

	bool IsValid() const { return (DistributionUrl.Len() > 0 || ServiceUrl.Len() > 0) && TocPath.Len() > 0; }
};

class IOnDemandIoDispatcherBackend
	: public IIoDispatcherBackend
{
public:
	virtual ~IOnDemandIoDispatcherBackend() = default;
	virtual void Mount(const FOnDemandEndpoint& Endpoint) = 0;
};

UE_API TSharedPtr<IOnDemandIoDispatcherBackend> MakeOnDemandIoDispatcherBackend(TSharedPtr<IIoCache> Cache);

} // namespace UE

#undef UE_API

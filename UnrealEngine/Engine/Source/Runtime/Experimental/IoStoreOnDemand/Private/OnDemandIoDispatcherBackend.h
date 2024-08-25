// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoDispatcherBackend.h"
#include "Templates/UniquePtr.h"

class IIasCache;
struct FAnalyticsEventAttribute;

namespace UE::IO::IAS
{

struct FDistributedEndpointUrl
{
	FString EndpointUrl;
	FString FallbackUrl;

	bool IsValid() const
	{
		return !EndpointUrl.IsEmpty();
	}

	bool HasFallbackUrl() const
	{
		return !FallbackUrl.IsEmpty();
	}

	void Reset()
	{
		EndpointUrl.Empty();
		FallbackUrl.Empty();
	}
};

struct FOnDemandEndpoint
{
	FString DistributionUrl;
	FString FallbackUrl;

	TArray<FString> ServiceUrls;
	FString TocPath;

	bool bForceTocDownload = false;

	bool IsValid() const
	{
		return (DistributionUrl.Len() > 0 || ServiceUrls.Num() > 0) && TocPath.Len() > 0;
	}
};

class IOnDemandIoDispatcherBackend
	: public IIoDispatcherBackend
{
public:
	virtual ~IOnDemandIoDispatcherBackend() = default;

	virtual void Mount(const FOnDemandEndpoint& Endpoint) = 0;
	virtual void SetBulkOptionalEnabled(bool bInEnabled) = 0;
	virtual void SetEnabled(bool bInEnabled) = 0;
	virtual bool IsEnabled() const = 0;
	virtual void AbandonCache() = 0;
	virtual void ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const = 0;
};

TSharedPtr<IOnDemandIoDispatcherBackend> MakeOnDemandIoDispatcherBackend(TUniquePtr<IIasCache>&& Cache);

} // namespace UE::IO::IAS

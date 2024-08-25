// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"
#include "HttpFwd.h"

namespace UE::IO::IAS
{

class FDistributionEndpoints
{
public:
	enum class EResult : uint8
	{
		Success = 0,
		Failure
	};

	FDistributionEndpoints() = default;
	~FDistributionEndpoints() = default;

	/**
	 * Queries the distributed endpoint in order to get a list of service urls from which we can download data.
	 * 
	 * @param DistributionUrl	The url of the distributed endpoint to resolve
	 * @param OutServiceUrls	An array into which the resolved urls will be placed
	 */
	FDistributionEndpoints::EResult ResolveEndpoints(const FString& DistributionUrl, TArray<FString>& OutServiceUrls);

	/**
	 * Queries the distributed endpoint in order to get a list of service urls from which we can download data.
	 * 
	 * @param DistributionUrl	The url of the distributed endpoint to resolve
	 * @param OutServiceUrls	An array into which the resolved urls will be placed
	 * @param Event				An event which can be triggered to cancel the request
	 */
	FDistributionEndpoints::EResult ResolveEndpoints(const FString& DistributionUrl, TArray<FString>& OutServiceUrls, FEvent& Event);

private:

	FDistributionEndpoints::EResult ParseResponse(FHttpResponsePtr HttpResponse, TArray<FString>& OutUrls);
};

} // namespace UE::IO::IAS

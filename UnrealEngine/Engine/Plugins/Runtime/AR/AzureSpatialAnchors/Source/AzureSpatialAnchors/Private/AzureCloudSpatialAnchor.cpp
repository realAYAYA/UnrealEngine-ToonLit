// Copyright Epic Games, Inc. All Rights Reserved.

#include "AzureCloudSpatialAnchor.h"

#include "Engine/Engine.h"
#include "IAzureSpatialAnchors.h"

FString UAzureCloudSpatialAnchor::GetAzureCloudIdentifier() const
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return FString();
	}
	FString RetVal;
	IASA->GetCloudSpatialAnchorIdentifier(CloudAnchorID, RetVal);
	return RetVal;
}

void UAzureCloudSpatialAnchor::SetExpiration(float Lifetime)
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return;
	}

	IASA->SetCloudAnchorExpiration(CloudAnchorID, Lifetime);
}

float UAzureCloudSpatialAnchor::GetExpiration() const
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return 0.0f;
	}
	float LifetimeInSeconds = 0.0f;
	IASA->GetCloudAnchorExpiration(CloudAnchorID, LifetimeInSeconds);
	return LifetimeInSeconds;
}

void UAzureCloudSpatialAnchor::SetAppProperties(const TMap<FString, FString>& InAppProperties)
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return;
	}

	IASA->SetCloudAnchorAppProperties(CloudAnchorID, InAppProperties);
}


TMap<FString, FString> UAzureCloudSpatialAnchor::GetAppProperties() const
{
	IAzureSpatialAnchors* IASA = IAzureSpatialAnchors::Get();
	if (IASA == nullptr)
	{
		return TMap<FString, FString>();
	}
	TMap<FString, FString> AppProperties;
	IASA->GetCloudAnchorAppProperties(CloudAnchorID, AppProperties);
	return AppProperties;
}
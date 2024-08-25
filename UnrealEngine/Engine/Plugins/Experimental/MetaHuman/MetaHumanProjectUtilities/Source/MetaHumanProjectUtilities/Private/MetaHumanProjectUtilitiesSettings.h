// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "MetaHumanProjectUtilitiesSettings.generated.h"

/**
 *
 */
UCLASS(Config = MetaHumanProjectUtilities)
class METAHUMANPROJECTUTILITIES_API UMetaHumanProjectUtilitiesSettings : public UObject
{
	GENERATED_BODY()

public:
	// The URL for fetching version information and release notes
	UPROPERTY(Config)
	FString VersionServiceBaseUrl;
};

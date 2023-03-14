// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HoloLensLocalizedResources.generated.h"

USTRUCT()
struct FHoloLensCorePackageStringResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString PackageDisplayName;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString PublisherDisplayName;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString PackageDescription;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString ApplicationDisplayName;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString ApplicationDescription;
};

USTRUCT()
struct FHoloLensCorePackageImageResources
{
	GENERATED_BODY()
};

USTRUCT()
struct FHoloLensCorePackageLocalizedResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString CultureId;

	UPROPERTY(EditAnywhere, config, Category = Packaging, Meta=(ShowOnlyInnerProperties))
	FHoloLensCorePackageStringResources Strings;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FHoloLensCorePackageImageResources Images;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GroomSettings.h"
#include "UObject/Object.h"
#include "GroomAssetInterpolation.h"
#include "GroomImportOptions.generated.h"

UCLASS(BlueprintType, config = EditorPerProjectUserSettings, HideCategories = ("Hidden"))
class HAIRSTRANDSCORE_API UGroomImportOptions : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, meta = (ShowOnlyInnerProperties), Category = Conversion)
	FGroomConversionSettings ConversionSettings;

	/* Interpolation settings per group */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Hidden)
	TArray<FHairGroupsInterpolation> InterpolationSettings;
};

USTRUCT(BlueprintType)
struct HAIRSTRANDSCORE_API FGroomHairGroupPreview
{
	GENERATED_USTRUCT_BODY()

	FGroomHairGroupPreview()
	: GroupID(0)
	, CurveCount(0)
	, GuideCount(0)
	, bHasRootUV(false)
	, bHasColorAttributes(false)
	, bHasRoughnessAttributes(false)
	, bHasPrecomputedWeights(false)
	, InterpolationSettings()
	{}

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Preview)
	FName GroupName;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Preview)
	int32 GroupID;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Preview)
	int32 CurveCount;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Preview)
	int32 GuideCount;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Preview)
	bool bHasRootUV;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Preview)
	bool bHasColorAttributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Preview)
	bool bHasRoughnessAttributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Preview)
	bool bHasPrecomputedWeights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Preview)
	FHairGroupsInterpolation InterpolationSettings;
};

UCLASS(BlueprintType, config = EditorPerProjectUserSettings, HideCategories = ("Hidden"))
class HAIRSTRANDSCORE_API UGroomHairGroupsPreview : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, meta = (ShowOnlyInnerProperties), Category = Preview)
	TArray<FGroomHairGroupPreview> Groups;
};
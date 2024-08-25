// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "NNEModelData.h"

#include "NNEDenoiserModelData.generated.h"

/** Denoiser model data asset */
UCLASS(BlueprintType)
class NNEDENOISER_API UNNEDenoiserModelData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** NNE model data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	TSoftObjectPtr<UNNEModelData> ModelData;

	/** Input mapping table */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	TSoftObjectPtr<UDataTable> InputMapping;

	/** Output mapping table */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	TSoftObjectPtr<UDataTable> OutputMapping;
};
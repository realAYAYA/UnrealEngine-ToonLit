// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataTable.h"

#include "NNEDenoiserModelIOMappingData.generated.h"

/** An enum to represent resource names used for input and output mapping */
UENUM()
enum EResourceName : uint8
{
	Color,
	Albedo,
	Normal,
	Flow,
	Output
};

/** Table row base for denoiser input and output mapping */
USTRUCT(BlueprintType)
struct FNNEDenoiserModelIOMappingData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

public:
	/** Input/output tensor index */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	int32 TensorIndex = 0;

	/** Input/output tensor channel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	int32 TensorChannel = 0;

	/** Mapped resource name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	TEnumAsByte<EResourceName> Resource = EResourceName::Color;

	/** Resource channel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	int32 ResourceChannel = 0;

	/** Resource frame index */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	int32 FrameIndex = 0;
};
// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Engine/EngineTypes.h"
#include "ImagePixelData.h"

namespace UE
{
namespace MoviePipeline
{
	MOVIERENDERPIPELINECORE_API TUniquePtr<FImagePixelData> QuantizeImagePixelDataToBitDepth(const FImagePixelData* InData, const int32 TargetBitDepth, FImagePixelPayloadPtr InPayload = nullptr, bool bConvertToSrgb = true);
}
}
	  
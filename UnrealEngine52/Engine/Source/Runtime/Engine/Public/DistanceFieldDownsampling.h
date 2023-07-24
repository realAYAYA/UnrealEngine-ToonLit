// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldDownsampling.h
=============================================================================*/

#pragma once

#include "RHI.h"

class FRHICommandListImmediate;
class FRDGBuilder;

struct FDistanceFieldDownsamplingDataTask
{
	FTexture3DRHIRef VolumeTextureRHI;
	FVector TexelSrcSize;
	FIntVector DstSize;
	FIntVector OffsetInAtlas;
};

class ENGINE_API FDistanceFieldDownsampling
{
public:
	static bool CanDownsample();
	static void GetDownsampledSize(const FIntVector& Size, float Factor, FIntVector& OutDownsampledSize);
	static void FillDownsamplingTask(const FIntVector& SrcSize, const FIntVector& DstSize, const FIntVector& OffsetInAtlas, EPixelFormat Format, FDistanceFieldDownsamplingDataTask& OutDataTask, FUpdateTexture3DData& OutTextureUpdateData);
	static void DispatchDownsampleTasks(FRDGBuilder& GraphBuilder, FRHIUnorderedAccessView* DFAtlasUAV, ERHIFeatureLevel::Type FeatureLevel, TArray<FDistanceFieldDownsamplingDataTask>& DownsamplingTasks, TArray<FUpdateTexture3DData>& UpdateTextureData);

	UE_DEPRECATED(5.0, "This method has been refactored to use an FRDGBuilder instead.")
	static void DispatchDownsampleTasks(FRHICommandListImmediate& RHICmdList, FRHIUnorderedAccessView* DFAtlasUAV, ERHIFeatureLevel::Type FeatureLevel, TArray<FDistanceFieldDownsamplingDataTask>& DownsamplingTasks, TArray<FUpdateTexture3DData>& UpdateTextureData);
};

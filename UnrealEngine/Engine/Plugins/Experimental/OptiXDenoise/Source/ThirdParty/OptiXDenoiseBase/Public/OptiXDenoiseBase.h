// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Minimal OptiX Wrapper header for OptiX Denoise Plugin

#ifndef OPTIXDENOISEBASE
	#define OPTIXDENOISEBASE_API __declspec( dllimport )
#else
	#define OPTIXDENOISEBASE_API __declspec( dllexport )
#endif

#include "cuda.h"
#include "OptiXCudaFunctionList.h"

typedef CUsurfObject TSurfaceObject;

typedef enum EOptiXDenoiseAlphaMode
{
	COPY,
	ALPHA_AS_AOV,
	FULL_DENOISE_PASS
}EOptiXDenoiseAlphaMode;

typedef unsigned int EOptiXDenoiseResult;

class FOptiXDenoiseContextImpl;
class FOpticalFlowContextImpl;

class OPTIXDENOISEBASE_API FOpticalFlowContext
{
public:
	FOpticalFlowContext();
	~FOpticalFlowContext();
	EOptiXDenoiseResult Init(CUcontext Context, CUstream Stream, unsigned int Width, unsigned int Height);
	EOptiXDenoiseResult ComputeFlow( FOptiXImageData& Flow, const FOptiXImageData* Input);
	void Destroy();
private:
	FOpticalFlowContextImpl* OpticalFlowContextImpl = nullptr;
};

typedef void (*OptiXDenoiseLogCallback)(unsigned int level, const char* tag, const char* message, void* cbdata);

class OPTIXDENOISEBASE_API FOptiXDenoiseContext
{
public:
	FOptiXDenoiseContext();
	~FOptiXDenoiseContext();
	
	EOptiXDenoiseResult InitOptiX();
	EOptiXDenoiseResult CreateContext(CUcontext CudaContext, OptiXDenoiseLogCallback LogCallBackFunction, int logCallbackLevel);

	EOptiXDenoiseResult InitializeDenoiser(bool GuildNormal, bool GuildAlbedo, bool TemporalMode);
	EOptiXDenoiseResult ComputeMemoryResource(uint32_t OutputWidth, uint32_t OutputHeight);

	size_t GetWithoutOverlapScratchSizeInBytes() const;
	size_t GetWithOverlapScratchSizeInBytes() const;
	size_t GetOverlapWindowSizeInPixels() const;
	size_t GetStateSizeInBytes() const;
	size_t GetInternalGuideLayerPixelSizeInBytes() const;

	void SetPreviousOutputInternalGuideLayer(const FOptiXImageData& OptiXImageData);
	void SetOutputInternalGuideLayer(const FOptiXImageData& OptiXImageData);

	EOptiXDenoiseResult SetupDenoiser(CUstream CudaStream, uint32_t InputWidth, uint32_t InputHeight, CUdeviceptr State, size_t StateSizeInBytes, CUdeviceptr Scratch, size_t ScratchSizeInBytes);

	FOptiXDenoiseContext& SetDenoiseAlpha(EOptiXDenoiseAlphaMode Mode);
	FOptiXDenoiseContext& SetHdrIntensity(CUdeviceptr Intensity);
	FOptiXDenoiseContext& SetHdrAverageColor(CUdeviceptr AverageColor);
	FOptiXDenoiseContext& SetBlendFactor(float BlendFactor);
	FOptiXDenoiseContext& SetTemporalModeUsePreviousLayers(bool Use);

	FOptiXDenoiseContext& SetLayerInput(const FOptiXImageData& OptiXImageData);
	FOptiXDenoiseContext& SetLayerOutput(const FOptiXImageData& OptiXImageData);
	FOptiXDenoiseContext& SetLayerPreviousOutput(const FOptiXImageData& OptiXImageData);

	FOptiXDenoiseContext& SetGuideLayerNormal(const FOptiXImageData& OptiXImageData);
	FOptiXDenoiseContext& SetGuideLayerAlbedo(const FOptiXImageData& OptiXImageData);
	FOptiXDenoiseContext& SetGuideLayerFlow(const FOptiXImageData& OptiXImageData);

	EOptiXDenoiseResult ComputeIntensity(CUstream CudaStream, CUdeviceptr Intensity, CUdeviceptr CudaScratch, size_t CudaScratchSize);
	EOptiXDenoiseResult ComputeAverageColor(CUstream CudaStream, CUdeviceptr AverageColor, CUdeviceptr CudaScratch, size_t CudaScratchSize);
	EOptiXDenoiseResult InvokeOptiXDenoise(CUstream CudaStream, CUdeviceptr CudaState, size_t CudaStateSize, uint32_t OffsetX, uint32_t OffsetY, CUdeviceptr CudaScratch, size_t CudaScratchSize);
	EOptiXDenoiseResult InvokeOptiXDenoise(CUstream CudaStream, CUdeviceptr CudaState, size_t CudaStateSize, CUdeviceptr CudaScratch, size_t CudaScratchSize, uint32_t CudaOverlap, uint32_t TileWidth, uint32_t TileHeight);

	void Destroy();

private:
	FOptiXDenoiseContextImpl* OptiXDenoiseContextImpl = nullptr;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "OptiXDenoiseBase.h"

// avoid win api pollution
#define NOMINMAX

#include <optix.h>
#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <optix_denoiser_tiling.h>
#include "optixOpticalFlow/optix_denoiser_opticalflow.h"


EOptiXDenoiseResult OptiXDenoiseResult(OptixResult Result)
{
	return static_cast<EOptiXDenoiseResult>(static_cast<uint32_t>(Result));
}

OptixImage2D GetOptiXImage2D(const FOptiXImageData& OptiXImageData)
{
	OptixImage2D Image;
	Image.data = OptiXImageData.Data;
	Image.width = OptiXImageData.Width;
	Image.height = OptiXImageData.Height;
	Image.rowStrideInBytes = OptiXImageData.RowStrideInBytes;
	Image.pixelStrideInBytes = OptiXImageData.PixelStrideInBytes;

	switch (OptiXImageData.Format)
	{
		case EOptiXImageFormat::CUDA_A32B32G32R32_F:
			Image.format = OPTIX_PIXEL_FORMAT_FLOAT4;
			break;
		case EOptiXImageFormat::INTERNAL_LAYER:
			Image.format = OPTIX_PIXEL_FORMAT_INTERNAL_GUIDE_LAYER;
		default:
			Image.format = OPTIX_PIXEL_FORMAT_FLOAT4; // @TODO: Error check
	}
	return Image;
}

class FOpticalFlowContextImpl
{
public:
	FOpticalFlowContextImpl() { OpticalFlow = new OptixUtilOpticalFlow();}
	~FOpticalFlowContextImpl() { if (OpticalFlow) { delete  OpticalFlow; OpticalFlow = nullptr;}}

	EOptiXDenoiseResult Init(CUcontext Context, CUstream Stream, unsigned int Width, unsigned int Height);
	EOptiXDenoiseResult ComputeFlow(FOptiXImageData& Flow, const FOptiXImageData* Input);
	void Destroy();
private:
	OptixUtilOpticalFlow* OpticalFlow = nullptr;
};

EOptiXDenoiseResult FOpticalFlowContextImpl::Init(CUcontext Context, CUstream Stream, unsigned int Width, unsigned int Height)
{
	OptixResult Result = OpticalFlow->init(Context, Stream, Width, Height);
	return OptiXDenoiseResult(Result);
}

EOptiXDenoiseResult FOpticalFlowContextImpl::ComputeFlow(FOptiXImageData& Flow, const FOptiXImageData* Input)
{
	OptixImage2D FlowRaw = GetOptiXImage2D(Flow);
	OptixImage2D InputsRaw[2] = { GetOptiXImage2D(Input[0]), GetOptiXImage2D(Input[1]) };
	return OpticalFlow->computeFlow(FlowRaw, (const OptixImage2D*)&InputsRaw[0]);
}

void FOpticalFlowContextImpl::Destroy()
{
	OpticalFlow->destroy();
}

FOpticalFlowContext::FOpticalFlowContext()
{
	OpticalFlowContextImpl = new FOpticalFlowContextImpl();
}

FOpticalFlowContext::~FOpticalFlowContext()
{
	if (OpticalFlowContextImpl)
	{
		delete OpticalFlowContextImpl;
		OpticalFlowContextImpl = nullptr;
	}
}

EOptiXDenoiseResult FOpticalFlowContext::Init(CUcontext Context, CUstream Stream, unsigned int Width, unsigned int Height)
{
	return OpticalFlowContextImpl->Init(Context, Stream, Width, Height);
}

EOptiXDenoiseResult FOpticalFlowContext::ComputeFlow(FOptiXImageData& Flow, const FOptiXImageData* Input)
{
	return OpticalFlowContextImpl->ComputeFlow(Flow, Input);
}

void FOpticalFlowContext::Destroy()
{
	OpticalFlowContextImpl->Destroy();
}

class FOptiXDenoiseContextImpl
{
public:
	EOptiXDenoiseResult InitializeDenoiser(bool GuildNormal, bool GuildAlbedo, bool TemporalMode);
	EOptiXDenoiseResult ComputeMemoryResource(uint32_t OutputWidth, uint32_t OutputHeight);

	void SetPreviousOutputInternalGuideLayer(const FOptiXImageData& OptiXImageData);
	void SetOutputInternalGuideLayer(const FOptiXImageData& OptiXImageData);

	EOptiXDenoiseResult SetupDenoiser(CUstream CudaStream, uint32_t InputWidth, uint32_t InputHeight, CUdeviceptr State, size_t StateSizeInBytes, CUdeviceptr Scratch, size_t ScratchSizeInBytes);

	void SetDenoiseAlpha(EOptiXDenoiseAlphaMode inMode);
	void SetHdrIntensity(CUdeviceptr Intensity) { Params.hdrIntensity = Intensity; }
	void SetHdrAverageColor(CUdeviceptr AverageColor) { Params.hdrAverageColor = AverageColor; }
	void SetBlendFactor(float BlendFactor) { Params.blendFactor = BlendFactor; }
	void SetTemporalModeUsePreviousLayers(bool Use) { Params.temporalModeUsePreviousLayers = Use; }

	void SetLayerInput(const FOptiXImageData& OptiXImageData) { Layer.input = GetOptiXImage2D(OptiXImageData); }
	void SetLayerOutput(const FOptiXImageData& OptiXImageData) { Layer.output = GetOptiXImage2D(OptiXImageData); }
	void SetLayerPreviousOutput(const FOptiXImageData& OptiXImageData) { Layer.previousOutput = GetOptiXImage2D(OptiXImageData); }

	void SetGuideLayerNormal(const FOptiXImageData& OptiXImageData) { GuideLayer.normal = GetOptiXImage2D(OptiXImageData); }
	void SetGuideLayerAlbedo(const FOptiXImageData& OptiXImageData) { GuideLayer.albedo = GetOptiXImage2D(OptiXImageData); }
	void SetGuideLayerFlow(const FOptiXImageData& OptiXImageData) { GuideLayer.flow = GetOptiXImage2D(OptiXImageData); }

	EOptiXDenoiseResult ComputeIntensity(CUstream CudaStream, CUdeviceptr Intensity, CUdeviceptr CudaScratch, size_t CudaScratchSize);
	EOptiXDenoiseResult ComputeAverageColor(CUstream CudaStream, CUdeviceptr AverageColor, CUdeviceptr CudaScratch, size_t CudaScratchSize);
	EOptiXDenoiseResult InvokeOptiXDenoise(CUstream CudaStream, CUdeviceptr CudaState, size_t CudaStateSize, uint32_t OffsetX, uint32_t OffsetY, CUdeviceptr CudaScratch, size_t CudaScratchSize);
	EOptiXDenoiseResult InvokeOptiXDenoise(CUstream CudaStream, CUdeviceptr CudaState, size_t CudaStateSize, CUdeviceptr CudaScratch, size_t CudaScratchSize, uint32_t CudaOverlap, uint32_t TileWidth, uint32_t TileHeight);

	void Destroy();

public:
	OptixDeviceContext		Context = nullptr;
	OptixDenoiser			Denoiser = nullptr;
	OptixDenoiserParams		Params = {};
	OptixDenoiserOptions	Options;
	OptixDenoiserSizes		Sizes;

	OptixDenoiserLayer Layer = {};
	OptixDenoiserGuideLayer GuideLayer = {};

	OptixDenoiserModelKind ModelKind;
};

EOptiXDenoiseResult FOptiXDenoiseContextImpl::InitializeDenoiser(bool GuildNormal, bool GuildAlbedo, bool TemporalMode)
{
	OptixResult Result;
	{
		Options = {};
		Options.guideAlbedo = GuildAlbedo ? 1 : 0;
		Options.guideNormal = GuildNormal ? 1 : 0;

		// @todo add AOVs denoising, Add KPMode supported
		ModelKind = TemporalMode ? OPTIX_DENOISER_MODEL_KIND_TEMPORAL : OPTIX_DENOISER_MODEL_KIND_HDR;
		Result = optixDenoiserCreate(Context, ModelKind, &Options, &Denoiser);
	}

	return OptiXDenoiseResult(Result);
}

EOptiXDenoiseResult FOptiXDenoiseContextImpl::ComputeMemoryResource(uint32_t OutputWidth, uint32_t OutputHeight)
{
	OptixResult Result; 
	Result = optixDenoiserComputeMemoryResources(Denoiser, OutputWidth, OutputHeight, &Sizes);
	return OptiXDenoiseResult(Result);
}

void FOptiXDenoiseContextImpl::SetPreviousOutputInternalGuideLayer(const FOptiXImageData& OptiXImageData)
{
	GuideLayer.previousOutputInternalGuideLayer.data = OptiXImageData.Data;
	GuideLayer.previousOutputInternalGuideLayer.width = OptiXImageData.Width;
	GuideLayer.previousOutputInternalGuideLayer.height = OptiXImageData.Height;
	GuideLayer.previousOutputInternalGuideLayer.pixelStrideInBytes = unsigned(Sizes.internalGuideLayerPixelSizeInBytes);
	GuideLayer.previousOutputInternalGuideLayer.rowStrideInBytes =
		GuideLayer.previousOutputInternalGuideLayer.width * GuideLayer.previousOutputInternalGuideLayer.pixelStrideInBytes;
	GuideLayer.previousOutputInternalGuideLayer.format = OPTIX_PIXEL_FORMAT_INTERNAL_GUIDE_LAYER;
}

void FOptiXDenoiseContextImpl::SetOutputInternalGuideLayer(const FOptiXImageData& OptiXImageData)
{
	GuideLayer.outputInternalGuideLayer.data = OptiXImageData.Data;
	GuideLayer.outputInternalGuideLayer.width = OptiXImageData.Width;
	GuideLayer.outputInternalGuideLayer.height = OptiXImageData.Height;
	GuideLayer.outputInternalGuideLayer.pixelStrideInBytes = unsigned(Sizes.internalGuideLayerPixelSizeInBytes);
	GuideLayer.outputInternalGuideLayer.rowStrideInBytes =
		GuideLayer.outputInternalGuideLayer.width * GuideLayer.outputInternalGuideLayer.pixelStrideInBytes;
	GuideLayer.outputInternalGuideLayer.format = OPTIX_PIXEL_FORMAT_INTERNAL_GUIDE_LAYER;
}

EOptiXDenoiseResult FOptiXDenoiseContextImpl::SetupDenoiser(CUstream CudaStream, uint32_t InputWidth, uint32_t InputHeight, CUdeviceptr State, size_t StateSizeInBytes, CUdeviceptr Scratch, size_t ScratchSizeInBytes)
{
	OptixResult Result;
	Result = optixDenoiserSetup(
		Denoiser,
		CudaStream,
		//TileWidth + 2 * CudaOverlap,
		//TileHeight + 2 * CudaOverlap,
		InputWidth,
		InputHeight,
		State,
		StateSizeInBytes,
		Scratch,
		ScratchSizeInBytes);

	return OptiXDenoiseResult(Result);
}

void FOptiXDenoiseContextImpl::SetDenoiseAlpha(EOptiXDenoiseAlphaMode InMode)
{

	OptixDenoiserAlphaMode Mode;

	switch (InMode)
	{
	case EOptiXDenoiseAlphaMode::COPY:
		Mode = OptixDenoiserAlphaMode::OPTIX_DENOISER_ALPHA_MODE_COPY;
		break;
	case EOptiXDenoiseAlphaMode::ALPHA_AS_AOV:
		Mode = OptixDenoiserAlphaMode::OPTIX_DENOISER_ALPHA_MODE_ALPHA_AS_AOV;
		break;
	case EOptiXDenoiseAlphaMode::FULL_DENOISE_PASS:
		Mode = OptixDenoiserAlphaMode::OPTIX_DENOISER_ALPHA_MODE_FULL_DENOISE_PASS;
		break;
	default:
		break;
	}

	Params.denoiseAlpha = Mode;
}

EOptiXDenoiseResult FOptiXDenoiseContextImpl::ComputeIntensity(CUstream CudaStream, CUdeviceptr Intensity, CUdeviceptr CudaScratch, size_t CudaScratchSize)
{
	OptixResult Result = optixDenoiserComputeIntensity(
		Denoiser,
		CudaStream,
		&Layer.input,
		Intensity,
		CudaScratch,
		CudaScratchSize);

	return OptiXDenoiseResult(Result);
}

EOptiXDenoiseResult FOptiXDenoiseContextImpl::ComputeAverageColor(CUstream CudaStream, CUdeviceptr AverageColor, CUdeviceptr CudaScratch, size_t CudaScratchSize)
{
	OptixResult Result = optixDenoiserComputeAverageColor(
		Denoiser,
		CudaStream,
		&Layer.input,
		AverageColor,
		CudaScratch,
		CudaScratchSize);

	return OptiXDenoiseResult(Result);
}

EOptiXDenoiseResult FOptiXDenoiseContextImpl::InvokeOptiXDenoise(CUstream CudaStream, CUdeviceptr CudaState, size_t CudaStateSize, uint32_t OffsetX, uint32_t OffsetY, CUdeviceptr CudaScratch, size_t CudaScratchSize)
{
	OptixResult Result = optixDenoiserInvoke(
		Denoiser,
		CudaStream,
		&Params,
		CudaState,
		CudaStateSize,
		&GuideLayer,
		&Layer,
		1u,
		OffsetX,	// input offset x
		OffsetY,	// input offset y
		CudaScratch,
		CudaScratchSize);

	return OptiXDenoiseResult(Result);
}

EOptiXDenoiseResult FOptiXDenoiseContextImpl::InvokeOptiXDenoise(CUstream CudaStream, CUdeviceptr CudaState, size_t CudaStateSize, CUdeviceptr CudaScratch, size_t CudaScratchSize, uint32_t CudaOverlap, uint32_t TileWidth, uint32_t TileHeight)
{
	OptixResult Result = optixUtilDenoiserInvokeTiled(
		Denoiser,
		CudaStream,
		&Params,
		CudaState,
		CudaStateSize,
		&GuideLayer,
		&Layer,
		1u,
		CudaScratch,
		CudaScratchSize,
		CudaOverlap,
		TileWidth,
		TileHeight);

	return OptiXDenoiseResult(Result);
}

void FOptiXDenoiseContextImpl::Destroy()
{
	if (Denoiser)
	{
		optixDenoiserDestroy(Denoiser);
		Denoiser = nullptr;
	}
	if (Context)
	{
		optixDeviceContextDestroy(Context);
		Context = nullptr;
	}
}

FOptiXDenoiseContext::FOptiXDenoiseContext()
{
	OptiXDenoiseContextImpl = new FOptiXDenoiseContextImpl();
}

FOptiXDenoiseContext::~FOptiXDenoiseContext()
{
	if (OptiXDenoiseContextImpl)
	{
		delete OptiXDenoiseContextImpl;
		OptiXDenoiseContextImpl = nullptr;
	}
}

EOptiXDenoiseResult FOptiXDenoiseContext::InitOptiX()
{
	OptixResult Result;
	Result = optixInit();

	return OptiXDenoiseResult(Result);
}

EOptiXDenoiseResult FOptiXDenoiseContext::CreateContext(CUcontext CudaContext, OptiXDenoiseLogCallback LogCallBackFunction, int logCallbackLevel)
{
	OptixResult Result;
	OptixDeviceContextOptions DeviceContexOptions = {};
	DeviceContexOptions.logCallbackFunction = (OptixLogCallback)LogCallBackFunction;
	DeviceContexOptions.logCallbackLevel = logCallbackLevel;

	Result = optixDeviceContextCreate(CudaContext, &DeviceContexOptions, &OptiXDenoiseContextImpl->Context);

	return OptiXDenoiseResult(Result);
}

EOptiXDenoiseResult FOptiXDenoiseContext::InitializeDenoiser(bool GuildNormal, bool GuildAlbedo, bool TemporalMode)
{
	return OptiXDenoiseContextImpl->InitializeDenoiser(GuildNormal, GuildAlbedo, TemporalMode);
}

EOptiXDenoiseResult FOptiXDenoiseContext::ComputeMemoryResource(uint32_t OutputWidth, uint32_t OutputHeight)
{
	return OptiXDenoiseContextImpl->ComputeMemoryResource(OutputWidth, OutputHeight);
}

size_t FOptiXDenoiseContext::GetWithoutOverlapScratchSizeInBytes() const
{
	OptixDenoiserSizes& Sizes = OptiXDenoiseContextImpl->Sizes;
	return Sizes.withoutOverlapScratchSizeInBytes;
}

size_t FOptiXDenoiseContext::GetWithOverlapScratchSizeInBytes() const
{
	OptixDenoiserSizes& Sizes = OptiXDenoiseContextImpl->Sizes;
	return Sizes.withOverlapScratchSizeInBytes;
}

size_t FOptiXDenoiseContext::GetOverlapWindowSizeInPixels() const
{
	OptixDenoiserSizes& Sizes = OptiXDenoiseContextImpl->Sizes;
	return Sizes.overlapWindowSizeInPixels;
}

size_t FOptiXDenoiseContext::GetStateSizeInBytes() const
{
	OptixDenoiserSizes& Sizes = OptiXDenoiseContextImpl->Sizes;
	return Sizes.stateSizeInBytes;
}

size_t FOptiXDenoiseContext::GetInternalGuideLayerPixelSizeInBytes() const
{
	OptixDenoiserSizes& Sizes = OptiXDenoiseContextImpl->Sizes;
	return Sizes.internalGuideLayerPixelSizeInBytes;
}

void FOptiXDenoiseContext::SetPreviousOutputInternalGuideLayer(const FOptiXImageData& OptiXImageData)
{
	OptiXDenoiseContextImpl->SetPreviousOutputInternalGuideLayer(OptiXImageData);
}

void FOptiXDenoiseContext::SetOutputInternalGuideLayer(const FOptiXImageData& OptiXImageData)
{
	OptiXDenoiseContextImpl->SetOutputInternalGuideLayer(OptiXImageData);
}

EOptiXDenoiseResult FOptiXDenoiseContext::SetupDenoiser(CUstream CudaStream, uint32_t InputWidth, uint32_t InputHeight, CUdeviceptr State, size_t StateSizeInBytes, CUdeviceptr Scratch, size_t ScratchSizeInBytes)
{
	return OptiXDenoiseContextImpl->SetupDenoiser(CudaStream, InputWidth, InputHeight, State, StateSizeInBytes, Scratch, ScratchSizeInBytes);
}

FOptiXDenoiseContext& FOptiXDenoiseContext::SetDenoiseAlpha(EOptiXDenoiseAlphaMode Mode)
{
	OptiXDenoiseContextImpl->SetDenoiseAlpha(Mode);
	return *this;
}

FOptiXDenoiseContext& FOptiXDenoiseContext::SetHdrIntensity(CUdeviceptr Intensity)
{
	OptiXDenoiseContextImpl->SetHdrIntensity(Intensity);
	return *this;
}

FOptiXDenoiseContext& FOptiXDenoiseContext::SetHdrAverageColor(CUdeviceptr AverageColor)
{
	OptiXDenoiseContextImpl->SetHdrAverageColor(AverageColor);
	return *this;
}

FOptiXDenoiseContext& FOptiXDenoiseContext::SetBlendFactor(float BlendFactor)
{
	OptiXDenoiseContextImpl->SetBlendFactor(BlendFactor);
	return *this;
}

FOptiXDenoiseContext& FOptiXDenoiseContext::SetTemporalModeUsePreviousLayers(bool Use)
{
	OptiXDenoiseContextImpl->SetTemporalModeUsePreviousLayers(Use);
	return *this;
}

FOptiXDenoiseContext& FOptiXDenoiseContext::SetLayerInput(const FOptiXImageData& OptiXImageData)
{
	OptiXDenoiseContextImpl->SetLayerInput(OptiXImageData);
	return *this;
}

FOptiXDenoiseContext& FOptiXDenoiseContext::SetLayerOutput(const FOptiXImageData& OptiXImageData)
{
	OptiXDenoiseContextImpl->SetLayerOutput(OptiXImageData);
	return *this;
}

FOptiXDenoiseContext& FOptiXDenoiseContext::SetLayerPreviousOutput(const FOptiXImageData& OptiXImageData)
{
	OptiXDenoiseContextImpl->SetLayerPreviousOutput(OptiXImageData);
	return *this;
}

FOptiXDenoiseContext& FOptiXDenoiseContext::SetGuideLayerNormal(const FOptiXImageData& OptiXImageData)
{
	OptiXDenoiseContextImpl->SetGuideLayerNormal(OptiXImageData);
	return *this;
}

FOptiXDenoiseContext& FOptiXDenoiseContext::SetGuideLayerAlbedo(const FOptiXImageData& OptiXImageData)
{
	OptiXDenoiseContextImpl->SetGuideLayerAlbedo(OptiXImageData);
	return *this;
}

FOptiXDenoiseContext& FOptiXDenoiseContext::SetGuideLayerFlow(const FOptiXImageData& OptiXImageData)
{
	OptiXDenoiseContextImpl->SetGuideLayerFlow(OptiXImageData);
	return *this;
}

EOptiXDenoiseResult FOptiXDenoiseContext::ComputeIntensity(CUstream CudaStream, CUdeviceptr Intensity, CUdeviceptr CudaScratch, size_t CudaScratchSize)
{
	return OptiXDenoiseContextImpl->ComputeIntensity(CudaStream,Intensity, CudaScratch,CudaScratchSize);
}

EOptiXDenoiseResult FOptiXDenoiseContext::ComputeAverageColor(CUstream CudaStream, CUdeviceptr AverageColor, CUdeviceptr CudaScratch, size_t CudaScratchSize)
{
	return OptiXDenoiseContextImpl->ComputeAverageColor(CudaStream, AverageColor, CudaScratch, CudaScratchSize);
}

EOptiXDenoiseResult FOptiXDenoiseContext::InvokeOptiXDenoise(CUstream CudaStream, CUdeviceptr CudaState, size_t CudaStateSize, uint32_t OffsetX, uint32_t OffsetY, CUdeviceptr CudaScratch, size_t CudaScratchSize)
{
	return OptiXDenoiseContextImpl->InvokeOptiXDenoise(CudaStream, CudaState, CudaStateSize, OffsetX, OffsetY, CudaScratch, CudaScratchSize);
}

EOptiXDenoiseResult FOptiXDenoiseContext::InvokeOptiXDenoise(CUstream CudaStream, CUdeviceptr CudaState, size_t CudaStateSize, CUdeviceptr CudaScratch, size_t CudaScratchSize, uint32_t CudaOverlap, uint32_t TileWidth, uint32_t TileHeight)
{
	return OptiXDenoiseContextImpl->InvokeOptiXDenoise(CudaStream, CudaState, CudaStateSize, CudaScratch, CudaScratchSize, CudaOverlap, TileWidth, TileHeight);
}

void FOptiXDenoiseContext::Destroy()
{
	OptiXDenoiseContextImpl->Destroy();
}
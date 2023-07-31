// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelCaptureCapturerRHIToI420Compute.h"
#include "PixelCaptureInputFrameRHI.h"
#include "PixelCaptureOutputFrameI420.h"
#include "PixelCaptureBufferFormat.h"

#include "RGBToYUVShader.h"

namespace
{
	inline void MemCpyStride(void* Dest, const void* Src, size_t DestStride, size_t SrcStride, size_t Height)
	{
		char* DestPtr = static_cast<char*>(Dest);
		const char* SrcPtr = static_cast<const char*>(Src);
		size_t Row = Height;
		while (Row--)
		{
			FMemory::Memcpy(DestPtr + DestStride * Row, SrcPtr + SrcStride * Row, DestStride);
		}
	}
}

TSharedPtr<FPixelCaptureCapturerRHIToI420Compute> FPixelCaptureCapturerRHIToI420Compute::Create(float InScale)
{
	return TSharedPtr<FPixelCaptureCapturerRHIToI420Compute>(new FPixelCaptureCapturerRHIToI420Compute(InScale));
}

FPixelCaptureCapturerRHIToI420Compute::FPixelCaptureCapturerRHIToI420Compute(float InScale)
	: Scale(InScale)
{
}

FPixelCaptureCapturerRHIToI420Compute ::~FPixelCaptureCapturerRHIToI420Compute()
{
	CleanUp();
}

void FPixelCaptureCapturerRHIToI420Compute::Initialize(int32 InputWidth, int32 InputHeight)
{
	const int32 Width = InputWidth * Scale;
	const int32 Height = InputHeight * Scale;

	PlaneYDimensions = { Width, Height };
	PlaneUVDimensions = { (Width + 1) / 2, (Height + 1) / 2 }; // UV is halved and rounded up

	FRHITextureCreateDesc TextureDescY =
		FRHITextureCreateDesc::Create2D(TEXT("Compute YUV Target"), PlaneYDimensions.X, PlaneYDimensions.Y, EPixelFormat::PF_R8)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::UAV)
			.SetInitialState(ERHIAccess::UAVCompute)
			.DetermineInititialState();

	FRHITextureCreateDesc TextureDescUV =
		FRHITextureCreateDesc::Create2D(TEXT("Compute YUV Target"), PlaneUVDimensions.X, PlaneUVDimensions.Y, EPixelFormat::PF_R8)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::UAV)
			.SetInitialState(ERHIAccess::UAVCompute)
			.DetermineInititialState();

	TextureY = GDynamicRHI->RHICreateTexture(TextureDescY);
	TextureU = GDynamicRHI->RHICreateTexture(TextureDescUV);
	TextureV = GDynamicRHI->RHICreateTexture(TextureDescUV);

	FRHITextureCreateDesc StagingDescY =
		FRHITextureCreateDesc::Create2D(TEXT("YUV Output CPU Texture"), PlaneYDimensions.X, PlaneYDimensions.Y, EPixelFormat::PF_R8)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::CPUReadback)
			.SetInitialState(ERHIAccess::Unknown)
			.DetermineInititialState();

	FRHITextureCreateDesc StagingDescUV =
		FRHITextureCreateDesc::Create2D(TEXT("YUV Output CPU Texture"), PlaneUVDimensions.X, PlaneUVDimensions.Y, EPixelFormat::PF_R8)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::CPUReadback)
			.SetInitialState(ERHIAccess::Unknown)
			.DetermineInititialState();

	StagingTextureY = GDynamicRHI->RHICreateTexture(StagingDescY);
	StagingTextureU = GDynamicRHI->RHICreateTexture(StagingDescUV);
	StagingTextureV = GDynamicRHI->RHICreateTexture(StagingDescUV);

	TextureYUAV = GDynamicRHI->RHICreateUnorderedAccessView(TextureY, 0, 0, 0);
	TextureUUAV = GDynamicRHI->RHICreateUnorderedAccessView(TextureU, 0, 0, 0);
	TextureVUAV = GDynamicRHI->RHICreateUnorderedAccessView(TextureV, 0, 0, 0);

	int32 OutWidth, OutHeight;
	GDynamicRHI->RHIMapStagingSurface(StagingTextureY, nullptr, MappedY, OutWidth, OutHeight);
	YStride = OutWidth;
	GDynamicRHI->RHIMapStagingSurface(StagingTextureU, nullptr, MappedU, OutWidth, OutHeight);
	UStride = OutWidth;
	GDynamicRHI->RHIMapStagingSurface(StagingTextureV, nullptr, MappedV, OutWidth, OutHeight);
	VStride = OutWidth;

	FPixelCaptureCapturer::Initialize(InputWidth, InputHeight);
}

IPixelCaptureOutputFrame* FPixelCaptureCapturerRHIToI420Compute::CreateOutputBuffer(int32 InputWidth, int32 InputHeight)
{
	const int32 Width = InputWidth * Scale;
	const int32 Height = InputHeight * Scale;
	return new FPixelCaptureOutputFrameI420(MakeShared<FPixelCaptureI420Buffer>(Width, Height));
}

void FPixelCaptureCapturerRHIToI420Compute::BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer)
{
	checkf(InputFrame.GetType() == StaticCast<int32>(PixelCaptureBufferFormat::FORMAT_RHI), TEXT("Incorrect source frame coming into frame capture process."));

	MarkCPUWorkStart();

	const FPixelCaptureInputFrameRHI& RHISourceFrame = StaticCast<const FPixelCaptureInputFrameRHI&>(InputFrame);
	FTexture2DRHIRef SourceTexture = RHISourceFrame.FrameTexture;

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	RHICmdList.EnqueueLambda([this](FRHICommandListImmediate&) { MarkGPUWorkStart(); });

	FRGBToYUVShaderParameters ShaderParameters;
	ShaderParameters.SourceTexture = SourceTexture;
	ShaderParameters.DestPlaneYDimensions = PlaneYDimensions;
	ShaderParameters.DestPlaneUVDimensions = PlaneUVDimensions;
	ShaderParameters.DestPlaneY = TextureYUAV;
	ShaderParameters.DestPlaneU = TextureUUAV;
	ShaderParameters.DestPlaneV = TextureVUAV;
	FRGBToYUVShader::Dispatch(RHICmdList, ShaderParameters);

	RHICmdList.CopyTexture(TextureY, StagingTextureY, {});
	RHICmdList.CopyTexture(TextureU, StagingTextureU, {});
	RHICmdList.CopyTexture(TextureV, StagingTextureV, {});

	// by adding this shared ref to the rhi lambda we can ensure that 'this' will not be destroyed
	// until after the rhi thread is done with it, so all the commands will still have valid references.
	TSharedRef<FPixelCaptureCapturerRHIToI420Compute> ThisRHIRef = StaticCastSharedRef<FPixelCaptureCapturerRHIToI420Compute>(AsShared());
	RHICmdList.EnqueueLambda([ThisRHIRef, OutputBuffer](FRHICommandListImmediate&) { ThisRHIRef->OnRHIStageComplete(OutputBuffer); });

	MarkCPUWorkEnd();
}

void FPixelCaptureCapturerRHIToI420Compute::OnRHIStageComplete(IPixelCaptureOutputFrame* OutputBuffer)
{
	MarkGPUWorkEnd();

	MarkCPUWorkStart();

	FPixelCaptureOutputFrameI420* OutputI420Buffer = StaticCast<FPixelCaptureOutputFrameI420*>(OutputBuffer);
	TSharedPtr<FPixelCaptureI420Buffer> I420Buffer = OutputI420Buffer->GetI420Buffer();

	MemCpyStride(I420Buffer->GetMutableDataY(), MappedY, I420Buffer->GetStrideY(), YStride, PlaneYDimensions.Y);
	MemCpyStride(I420Buffer->GetMutableDataU(), MappedU, I420Buffer->GetStrideUV(), UStride, PlaneUVDimensions.Y);
	MemCpyStride(I420Buffer->GetMutableDataV(), MappedV, I420Buffer->GetStrideUV(), VStride, PlaneUVDimensions.Y);

	MarkCPUWorkEnd();
	EndProcess();
}

void FPixelCaptureCapturerRHIToI420Compute::CleanUp()
{
	GDynamicRHI->RHIUnmapStagingSurface(StagingTextureY);
	GDynamicRHI->RHIUnmapStagingSurface(StagingTextureU);
	GDynamicRHI->RHIUnmapStagingSurface(StagingTextureV);

	MappedY = nullptr;
	MappedU = nullptr;
	MappedV = nullptr;
}

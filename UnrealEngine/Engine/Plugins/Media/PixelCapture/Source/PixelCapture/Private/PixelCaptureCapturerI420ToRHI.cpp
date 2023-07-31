// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelCaptureCapturerI420ToRHI.h"
#include "PixelCaptureInputFrameI420.h"
#include "PixelCaptureOutputFrameRHI.h"
#include "PixelCaptureBufferFormat.h"

#include "Async/Async.h"

#include "libyuv/convert.h"
#include "libyuv/video_common.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"

TSharedPtr<FPixelCaptureCapturerI420ToRHI> FPixelCaptureCapturerI420ToRHI::Create()
{
	return TSharedPtr<FPixelCaptureCapturerI420ToRHI>(new FPixelCaptureCapturerI420ToRHI());
}

FPixelCaptureCapturerI420ToRHI::~FPixelCaptureCapturerI420ToRHI()
{
	if (ARGBBuffer)
	{
		delete[] ARGBBuffer;
	}
}

void FPixelCaptureCapturerI420ToRHI::Initialize(int32 InputWidth, int32 InputHeight)
{
	Dimensions = { InputWidth, InputHeight };

	const uint32_t Size = webrtc::CalcBufferSize(webrtc::VideoType::kARGB, Dimensions.X, Dimensions.Y);
	ARGBBuffer = new uint8_t[Size];

	FPixelCaptureCapturer::Initialize(InputWidth, InputHeight);
}

IPixelCaptureOutputFrame* FPixelCaptureCapturerI420ToRHI::CreateOutputBuffer(int32 InputWidth, int32 InputHeight)
{
	FRHITextureCreateDesc TextureDesc =
		FRHITextureCreateDesc::Create2D(TEXT("FPixelCaptureCapturerI420ToRHI Texture"), InputWidth, InputHeight, EPixelFormat::PF_B8G8R8A8)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::CPUWritable | ETextureCreateFlags::RenderTargetable)
			.SetInitialState(ERHIAccess::Present)
			.DetermineInititialState();

	if (RHIGetInterfaceType() == ERHIInterfaceType::Vulkan)
	{
		TextureDesc.AddFlags(ETextureCreateFlags::External);
	}
	else
	{
		TextureDesc.AddFlags(ETextureCreateFlags::Shared);
	}

	return new FPixelCaptureOutputFrameRHI(GDynamicRHI->RHICreateTexture(TextureDesc));
}

void FPixelCaptureCapturerI420ToRHI::BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer)
{
	checkf(InputFrame.GetType() == StaticCast<int32>(PixelCaptureBufferFormat::FORMAT_I420), TEXT("Incorrect source frame coming into frame capture process, expected FORMAT_I420"));

	MarkCPUWorkStart();

	const FPixelCaptureInputFrameI420& SourceFrame = StaticCast<const FPixelCaptureInputFrameI420&>(InputFrame);
	TSharedPtr<FPixelCaptureI420Buffer> I420Buffer = SourceFrame.GetBuffer();
	FPixelCaptureOutputFrameRHI* OutputH264Buffer = StaticCast<FPixelCaptureOutputFrameRHI*>(OutputBuffer);

	// convert the incoming i420 to argb
	libyuv::ConvertFromI420(
		I420Buffer->GetDataY(), I420Buffer->GetStrideY(),
		I420Buffer->GetDataU(), I420Buffer->GetStrideUV(),
		I420Buffer->GetDataV(), I420Buffer->GetStrideUV(),
		ARGBBuffer, 0,
		I420Buffer->GetWidth(), I420Buffer->GetHeight(),
		libyuv::FOURCC_ARGB);

	TWeakPtr<FPixelCaptureCapturerI420ToRHI> WeakSelf = StaticCastSharedRef<FPixelCaptureCapturerI420ToRHI>(AsShared());
	AsyncTask(ENamedThreads::ActualRenderingThread, [WeakSelf, OutputH264Buffer]() {
		if (TSharedPtr<FPixelCaptureCapturerI420ToRHI> Self = WeakSelf.Pin())
		{
			Self->MarkGPUWorkStart();

			// update our staging texture
			FUpdateTextureRegion2D Region(0, 0, 0, 0, Self->Dimensions.X, Self->Dimensions.Y);
			RHIUpdateTexture2D(OutputH264Buffer->GetFrameTexture(), 0, Region, Self->Dimensions.X * 4, Self->ARGBBuffer);

			Self->MarkGPUWorkEnd();
			Self->EndProcess();
		}
	});

	MarkCPUWorkEnd();
}

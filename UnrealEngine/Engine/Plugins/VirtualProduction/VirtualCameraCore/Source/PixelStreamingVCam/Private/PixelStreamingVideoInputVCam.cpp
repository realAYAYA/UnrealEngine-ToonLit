// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingVideoInputVCam.h"
#include "PixelStreamingSettings.h"
#include "PixelCaptureCapturerRHI.h"
#include "PixelCaptureCapturerRHIRDG.h"
#include "PixelCaptureCapturerRHINoCopy.h"
#include "PixelCaptureCapturerRHIToI420CPU.h"
#include "PixelCaptureCapturerRHIToI420Compute.h"
#include "RHI.h"

TSharedPtr<FPixelStreamingVideoInputVCam> FPixelStreamingVideoInputVCam::Create()
{
	TSharedPtr<FPixelStreamingVideoInputVCam> NewInput = TSharedPtr<FPixelStreamingVideoInputVCam>(new FPixelStreamingVideoInputVCam());
	return NewInput;
}

FPixelStreamingVideoInputVCam::FPixelStreamingVideoInputVCam()
{
}

FPixelStreamingVideoInputVCam::~FPixelStreamingVideoInputVCam()
{
}

FString FPixelStreamingVideoInputVCam::ToString()
{
	return TEXT("a Virtual Camera");
}

TSharedPtr<FPixelCaptureCapturer> FPixelStreamingVideoInputVCam::CreateCapturer(int32 FinalFormat, float FinalScale)
{
	switch (FinalFormat)
	{
		case PixelCaptureBufferFormat::FORMAT_RHI:
		{
			if(FPixelStreamingSettings::GetSimulcastParameters().Layers.Num() == 1 && 
			   FPixelStreamingSettings::GetSimulcastParameters().Layers[0].Scaling == 1.0)
			{
				// If we only have a single layer (and it's scale is 1), we can use the no copy capturer 
				// as we know the output from the media capture will already be the correct format and scale
				return FPixelCaptureCapturerRHINoCopy::Create(FinalScale);
			}
			else
			{
				// "Safe Texture Copy" polls a fence to ensure a GPU copy is complete
				// the RDG pathway does not poll a fence so is more unsafe but offers
				// a significant performance increase
				if (FPixelStreamingSettings::GetCaptureUseFence())
				{
					return FPixelCaptureCapturerRHI::Create(FinalScale);
				}
				else
				{
					return FPixelCaptureCapturerRHIRDG::Create(FinalScale);
				}
			}
		}
		case PixelCaptureBufferFormat::FORMAT_I420:
		{
			if (FPixelStreamingSettings::GetVPXUseCompute())
			{
				return FPixelCaptureCapturerRHIToI420Compute::Create(FinalScale);
			}
			else
			{
				return FPixelCaptureCapturerRHIToI420CPU::Create(FinalScale);
			}
		}
		default:
			// UE_LOG(LogPixelStreaming, Error, TEXT("Unsupported final format %d"), FinalFormat);
			return nullptr;
	}
}

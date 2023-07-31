// Copyright Epic Games, Inc. All Rights Reserved.

#include "Capture/DisplayClusterMediaCaptureBase.h"
#include "DisplayClusterMediaHelpers.h"
#include "DisplayClusterMediaLog.h"
#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"

#include "MediaCapture.h"
#include "MediaOutput.h"

#include "RenderGraphBuilder.h"


FDisplayClusterMediaCaptureBase::FDisplayClusterMediaCaptureBase(const FString& InMediaId, const FString& InClusterNodeId, UMediaOutput* InMediaOutput)
	: FDisplayClusterMediaBase(InMediaId, InClusterNodeId)
	, MediaOutput(InMediaOutput)
{
	check(InMediaOutput);

	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostTick().AddRaw(this, &FDisplayClusterMediaCaptureBase::OnPostClusterTick);
}


FDisplayClusterMediaCaptureBase::~FDisplayClusterMediaCaptureBase()
{
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostTick().RemoveAll(this);
}

void FDisplayClusterMediaCaptureBase::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MediaOutput);

	if (MediaCapture)
	{
		Collector.AddReferencedObject(MediaCapture);
	}
}

bool FDisplayClusterMediaCaptureBase::StartCapture()
{
	if (MediaOutput && !MediaCapture)
	{
		MediaCapture = MediaOutput->CreateMediaCapture();
		if (MediaCapture)
		{
			MediaCapture->SetMediaOutput(MediaOutput);

			bWasCaptureStarted = StartMediaCapture();
			return bWasCaptureStarted;
		}
	}

	return false;
}

void FDisplayClusterMediaCaptureBase::StopCapture()
{
	if (MediaCapture)
	{
		MediaCapture->StopCapture(false);
		MediaCapture = nullptr;
		bWasCaptureStarted = false;
	}
}

void FDisplayClusterMediaCaptureBase::ExportMediaData(FRDGBuilder& GraphBuilder, const FMediaTextureInfo& TextureInfo)
{
	FRHITexture* const SrcTexture = TextureInfo.Texture;

	if (SrcTexture)
	{
		MediaCapture->SetValidSourceGPUMask(GraphBuilder.RHICmdList.GetGPUMask());

		const FIntPoint SrcRegionSize = TextureInfo.Region.Size();
		MediaCapture->CaptureImmediate_RenderThread(GraphBuilder, SrcTexture);
	}
}

void FDisplayClusterMediaCaptureBase::OnPostClusterTick()
{
	if (MediaCapture && bWasCaptureStarted)
	{
		if (MediaCapture->GetState() == EMediaCaptureState::Error)
		{
			constexpr double Interval = 1.0;
			const double CurrentTime = FPlatformTime::Seconds();
			if (CurrentTime - LastRestartTimestamp > Interval)
			{
				UE_LOG(LogDisplayClusterMedia, Log, TEXT("MediaCapture '%s' is in error, restarting it."), *GetMediaId());

				StartMediaCapture();
				LastRestartTimestamp = CurrentTime;
			}
		}
	}
}

bool FDisplayClusterMediaCaptureBase::StartMediaCapture()
{
	FRHICaptureResourceDescription Descriptor;
	Descriptor.ResourceSize = GetCaptureSize();
	
	FMediaCaptureOptions MediaCaptureOptions;
	MediaCaptureOptions.NumberOfFramesToCapture = -1;
	MediaCaptureOptions.bSkipFrameWhenRunningExpensiveTasks = false;
	MediaCaptureOptions.OverrunAction = EMediaCaptureOverrunAction::Skip;

	const bool bCaptureStarted = MediaCapture->CaptureRHITexture(Descriptor, MediaCaptureOptions);
	if (bCaptureStarted)
	{
		UE_LOG(LogDisplayClusterMedia, Log, TEXT("Started media capture: '%s'"), *GetMediaId());
	}
	else
	{
		UE_LOG(LogDisplayClusterMedia, Warning, TEXT("Couldn't start media capture '%s'"), *GetMediaId());
	}

	return bCaptureStarted;
}

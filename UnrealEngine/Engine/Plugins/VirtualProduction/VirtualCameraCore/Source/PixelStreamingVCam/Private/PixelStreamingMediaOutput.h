// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaOutput.h"
#include "IPixelStreamingStreamer.h"
#include "PixelStreamingMediaIOCapture.h"
#include "PixelStreamingVideoInputVCam.h"
#include "Delegates/DelegateCombinations.h"
#include "PixelStreamingMediaOutput.generated.h"

UCLASS(BlueprintType)
class UPixelStreamingMediaOutput : public UMediaOutput
{
	GENERATED_BODY()

	//~ Begin UMediaOutput interface
public:
	virtual FIntPoint GetRequestedSize() const override { return UMediaOutput::RequestCaptureSourceSize; }
	virtual EPixelFormat GetRequestedPixelFormat() const override { return EPixelFormat::PF_B8G8R8A8; }
	virtual EMediaCaptureConversionOperation GetConversionOperation(EMediaCaptureSourceType InSourceType) const override { return EMediaCaptureConversionOperation::CUSTOM; }

	virtual void BeginDestroy() override;

protected:
	virtual UMediaCapture* CreateMediaCaptureImpl() override;
	//~ End UMediaOutput interface

public:
	static UPixelStreamingMediaOutput* Create(UObject* Outer, const FString& StreamerId);

	bool IsValid() const { return Streamer != nullptr; }
	TSharedPtr<IPixelStreamingStreamer> GetStreamer() const { return Streamer; }

	void StartStreaming();
	void StopStreaming();

	DECLARE_EVENT_OneParam(UPixelStreamingMediaOutput, FRemoteResolutionChangedEvent, const FIntPoint&)
	FRemoteResolutionChangedEvent& OnRemoteResolutionChanged() { return RemoteResolutionChangedEvent; }

private:
	TSharedPtr<IPixelStreamingStreamer> Streamer;

	/** Broadcasts whenever the layer changes */
	FRemoteResolutionChangedEvent RemoteResolutionChangedEvent;

	void OnCaptureStateChanged();
	void OnCaptureViewportInitialized();

	/* When the remote device sends a resolution change request back to the VCam we can respond here. */
	void RegisterRemoteResolutionCommandHandler();
};

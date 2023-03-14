// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaOutput.h"
#include "IPixelStreamingStreamer.h"
#include "PixelStreamingMediaCapture.h"
#include "PixelStreamingVideoInput.h"
#include "PixelStreamingMediaOutput.generated.h"


UCLASS(BlueprintType)
class UPixelStreamingMediaOutput : public UMediaOutput
{
	GENERATED_BODY()

	//~ Begin UMediaOutput interface
public:
	virtual FIntPoint GetRequestedSize() const override { return UMediaOutput::RequestCaptureSourceSize; }
	virtual EPixelFormat GetRequestedPixelFormat() const override { return EPixelFormat::PF_B8G8R8A8; }
	virtual EMediaCaptureConversionOperation GetConversionOperation(EMediaCaptureSourceType InSourceType) const override { return EMediaCaptureConversionOperation::NONE; }

	virtual void BeginDestroy() override;

protected:
	virtual UMediaCapture* CreateMediaCaptureImpl() override;
	//~ End UMediaOutput interface

public:
	TSharedPtr<IPixelStreamingStreamer> GetStreamer() const { return Streamer; }

	void SetSignallingServerURL(FString InURL);
	void SetSignallingStreamID(FString InStreamID);
	void StartStreaming();
	void StopStreaming();

private:
	UPixelStreamingMediaCapture* Capture = nullptr;
	TSharedPtr<IPixelStreamingStreamer> Streamer;
	TSharedPtr<FPixelStreamingVideoInput> VideoInput;

	FString SignallingServerURL;
	FString StreamID = TEXT("VCam");

	void OnCaptureStateChanged();
	void OnCaptureViewportInitialized();
};

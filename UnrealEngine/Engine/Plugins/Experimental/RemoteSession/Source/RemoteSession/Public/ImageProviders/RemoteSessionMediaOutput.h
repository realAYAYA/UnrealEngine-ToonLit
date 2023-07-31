// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/RemoteSessionImageChannel.h"
#include "MediaCapture.h"
#include "MediaOutput.h"
#include "RemoteSessionMediaOutput.generated.h"

UCLASS(BlueprintType)
class REMOTESESSION_API URemoteSessionMediaOutput : public UMediaOutput
{
	GENERATED_BODY()

	//~ Begin UMediaOutput interface
public:
	virtual FIntPoint GetRequestedSize() const override { return UMediaOutput::RequestCaptureSourceSize; }
	virtual EPixelFormat GetRequestedPixelFormat() const override { return EPixelFormat::PF_B8G8R8A8; }
	virtual EMediaCaptureConversionOperation GetConversionOperation(EMediaCaptureSourceType InSourceType) const override { return EMediaCaptureConversionOperation::SET_ALPHA_ONE; }
protected:
	virtual UMediaCapture* CreateMediaCaptureImpl() override;
	//~ End UMediaOutput interface

public:

	void SetImageChannel(TWeakPtr<FRemoteSessionImageChannel> ImageChannel);
	TWeakPtr<FRemoteSessionImageChannel> GetImageChannel() const { return ImageChannel; }

private:

	TWeakPtr<FRemoteSessionImageChannel> ImageChannel;
};


UCLASS(BlueprintType)
class REMOTESESSION_API URemoteSessionMediaCapture : public UMediaCapture
{
	GENERATED_BODY()

public:

	//~ Begin UMediaCapture interface
	virtual void OnFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, void* InBuffer, int32 Width, int32 Height, int BytesPerRow) override;
	virtual bool InitializeCapture() override;
	//~ End UMediaCapture interface

private:

	void CacheValues();

private:

	TSharedPtr<FRemoteSessionImageChannel> ImageChannel;
};

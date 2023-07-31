// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaCapture.h"

#include "ImageWriteTypes.h"
#include "FileMediaCapture.generated.h"

/**
 * 
 */
UCLASS()
class MEDIAIOCORE_API UFileMediaCapture : public UMediaCapture
{
	GENERATED_BODY()

protected:
	virtual void OnFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, void* InBuffer, int32 Width, int32 Height, int32 BytesPerRow) override;
	virtual bool InitializeCapture() override;

private:
	void CacheMediaOutputValues();

private:
	FString BaseFilePathName;
	EImageFormat ImageFormat;
	TFunction<void(bool)> OnCompleteWrapper;
	bool bOverwriteFile;
	int32 CompressionQuality;
	bool bAsync;
};

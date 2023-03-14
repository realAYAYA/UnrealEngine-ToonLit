// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneCaptureProtocolBase.h"
#include "FrameGrabber.h"
#include "Protocols/FrameGrabberProtocol.h"
#include "AVIWriter.h"
#include "VideoCaptureProtocol.generated.h"


UCLASS(meta=(DisplayName="Video Sequence (avi)", CommandLineID="Video"))
class MOVIESCENECAPTURE_API UVideoCaptureProtocol : public UFrameGrabberProtocol
{
public:
	GENERATED_BODY()

	UVideoCaptureProtocol(const FObjectInitializer& Init)
		: Super(Init)
		, bUseCompression(true)
		, CompressionQuality(75)
	{}

public:

	UPROPERTY(config, EditAnywhere, Category=VideoSettings)
	bool bUseCompression;

	UPROPERTY(config, EditAnywhere, Category=VideoSettings, meta=(ClampMin=1, ClampMax=100, EditCondition=bUseCompression))
	float CompressionQuality;

public:
	virtual bool SetupImpl() override;
	virtual void FinalizeImpl() override;
	virtual FFramePayloadPtr GetFramePayload(const FFrameMetrics& FrameMetrics);
	virtual void ProcessFrame(FCapturedFrameData Frame);
	virtual bool CanWriteToFileImpl(const TCHAR* InFilename, bool bOverwriteExisting) const override;

protected:

	void ConditionallyCreateWriter();
private:

	TArray<TUniquePtr<FAVIWriter>> AVIWriters;
};

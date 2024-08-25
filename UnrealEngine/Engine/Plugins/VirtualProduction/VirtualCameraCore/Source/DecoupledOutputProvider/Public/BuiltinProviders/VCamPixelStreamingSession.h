// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DecoupledOutputProvider.h"
#include "VCamPixelStreamingSession.generated.h"

UCLASS(meta = (DisplayName = "Pixel Streaming Provider"))
class DECOUPLEDOUTPUTPROVIDER_API UVCamPixelStreamingSession : public UDecoupledOutputProvider
{
	GENERATED_BODY()
public:

	UVCamPixelStreamingSession();
	
	/** If using the output from a Composure Output Provider, specify it here */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "10"))
	int32 FromComposureOutputProviderIndex = INDEX_NONE;

	/** If true the streamed UE viewport will match the resolution of the remote device. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "11"))
	bool bMatchRemoteResolution = true;

	/** Check this if you wish to control the corresponding CineCamera with transform data received from the LiveLink app */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "12"))
	bool EnableARKitTracking = true;

	/** If not selected, when the editor is not the foreground application, input through the vcam session may seem sluggish or unresponsive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "13"))
	bool PreventEditorIdle = true;

	/** If true then the Live Link Subject of the owning VCam Component will be set to the subject created by this Output Provider when the Provider is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "14"))
	bool bAutoSetLiveLinkSubject = true;

	/** Set the name of this stream to be reported to the signalling server. If none is supplied a default will be used. If ids are not unique issues can occur. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "15"))
	FString StreamerId;
};

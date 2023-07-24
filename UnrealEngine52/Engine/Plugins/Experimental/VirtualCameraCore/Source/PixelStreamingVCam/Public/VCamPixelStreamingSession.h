// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Output/VCamOutputProviderBase.h"
#include "PixelStreamingMediaCapture.h"
#include "PixelStreamingMediaOutput.h"
#include "PixelStreamingServers.h"
#include "VCamPixelStreamingSession.generated.h"

UCLASS(meta = (DisplayName = "Pixel Streaming Provider"))
class PIXELSTREAMINGVCAM_API UVCamPixelStreamingSession : public UVCamOutputProviderBase
{
	GENERATED_BODY()
public:

	UVCamPixelStreamingSession();

	//~ Begin UVCamOutputProviderBase Interface
	virtual void Deinitialize() override;
	virtual void Activate() override;
	virtual void Deactivate() override;
	virtual void Tick(const float DeltaTime) override;
	//~ End UVCamOutputProviderBase Interface
	
#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ Begin UObject Interface
#endif
	
	// If using the output from a Composure Output Provider, specify it here
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "10"))
	int32 FromComposureOutputProviderIndex = INDEX_NONE;

	// If true the streamed UE viewport will match the resolution of the remote device.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "11"))
	bool bMatchRemoteResolution = true;

	// Check this if you wish to control the corresponding CineCamera with transform data received from the LiveLink app
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "12"))
	bool EnableARKitTracking = true;

	// If not selected, when the editor is not the foreground application, input through the vcam session may seem sluggish or unresponsive.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "13"))
	bool PreventEditorIdle = true;

	// If true then the Live Link Subject of the owning VCam Component will be set to the subject created by this Output Provider when the Provider is enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "14"))
	bool bAutoSetLiveLinkSubject = true;

	// Set the name of this stream to be reported to the signalling server. If none is supplied a default will be used. If ids are not unique issues can occur.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "15"))
	FString StreamerId;
	
protected:
	
	UPROPERTY(Transient)
	TObjectPtr<UPixelStreamingMediaOutput> MediaOutput = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPixelStreamingMediaCapture> MediaCapture = nullptr;

	//~ Begin UVCamOutputProviderBase Interface
	virtual bool ShouldOverrideResolutionOnActivationEvents() const { return true; }
	//~ End UVCamOutputProviderBase Interface

private:
	void SetupSignallingServer();
	void StopSignallingServer();
	void SetupCapture();
	void StartCapture();
	void SetupCustomInputHandling();
	void OnCaptureStateChanged();
	void OnARKitTransformReceived(FPixelStreamingPlayerId PlayerId, uint8 Type, TArray<uint8> Data);
	void OnRemoteResolutionChanged(const FIntPoint& RemoteResolution);
private:
	FHitResult 	LastViewportTouchResult;
	bool 		bUsingDummyUMG = false;
	bool 		bOldThrottleCPUWhenNotForeground;
	static int  NextDefaultStreamerId;
};

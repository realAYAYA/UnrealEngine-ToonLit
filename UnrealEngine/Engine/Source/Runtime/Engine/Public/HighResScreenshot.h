// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FSceneViewport;
class IImageWrapper;
class UMaterial;
class FImageWriteTask;
class IImageWriteQueue;

DECLARE_LOG_CATEGORY_EXTERN(LogHighResScreenshot, Log, All);

struct ENGINE_API FHighResScreenshotConfig
{
	static const float MinResolutionMultipler;
	static const float MaxResolutionMultipler;

	FIntRect UnscaledCaptureRegion;
	FIntRect CaptureRegion;
	float ResolutionMultiplier;
	float ResolutionMultiplierScale;
	bool bMaskEnabled;
	bool bDateTimeBasedNaming;
	bool bDumpBufferVisualizationTargets;
	TWeakPtr<FSceneViewport> TargetViewport;
	bool bDisplayCaptureRegion;
	bool bCaptureHDR;
	bool bForce128BitRendering;
	FString FilenameOverride;

	// Materials used in the editor to help with the capture of highres screenshots
	UMaterial* HighResScreenshotMaterial;
	UMaterial* HighResScreenshotMaskMaterial;
	UMaterial* HighResScreenshotCaptureRegionMaterial;

	/** Pointer to the image write queue to use for async image writes */
	IImageWriteQueue* ImageWriteQueue;

	FHighResScreenshotConfig();

	/** Initialize the Image write queue necessary for asynchronously saving screenshots **/
	void Init();

	/** Populate the specified task with parameters from the current high-res screenshot request */
	void PopulateImageTaskParams(FImageWriteTask& InOutTask);

	/** Point the screenshot UI at a different viewport **/
	void ChangeViewport(TWeakPtr<FSceneViewport> InViewport);

	/** Parse screenshot parameters from the supplied console command line **/
	bool ParseConsoleCommand(const FString& InCmd, FOutputDevice& Ar);

	/** Utility function for merging the mask buffer into the alpha channel of the supplied bitmap, if masking is enabled.
	  * Returns true if the mask was written, and false otherwise.
	**/
	bool MergeMaskIntoAlpha(TArray<FColor>& InBitmap, const FIntRect& ViewRect);

	/** Enable/disable HDR capable captures **/
	void SetHDRCapture(bool bCaptureHDRIN);

	/** Enable/disable forcing 128-bit rendering pipeline for capture **/
	void SetForce128BitRendering(bool bForce);

	/** Configure taking a high res screenshot */
	bool SetResolution(uint32 ResolutionX, uint32 ResolutionY, float ResolutionScale = 1.0f);

	/** Configure screenshot filename */
	void SetFilename(FString Filename);

	/** Configure screenshot mask is enabled */
	void SetMaskEnabled(bool bShouldMaskBeEnabled);
};

ENGINE_API FHighResScreenshotConfig& GetHighResScreenshotConfig();

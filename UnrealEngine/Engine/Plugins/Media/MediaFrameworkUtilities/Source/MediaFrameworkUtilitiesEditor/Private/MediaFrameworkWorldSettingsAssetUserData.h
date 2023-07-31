// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Actor.h"
#include "MediaCapture.h"
#include "UObject/LazyObjectPtr.h"

#include "MediaFrameworkWorldSettingsAssetUserData.generated.h"

class UMediaFrameworkWorldSettingsAssetUserData;
class UMediaOutput;
class UTextureRenderTarget2D;

/**
 * FMediaFrameworkCaptureCurrentViewportOutputInfo
 */
USTRUCT()
struct FMediaFrameworkCaptureCurrentViewportOutputInfo
{
	GENERATED_BODY()

	FMediaFrameworkCaptureCurrentViewportOutputInfo();

	UPROPERTY(EditAnywhere, Category="MediaViewportCapture")
	TObjectPtr<UMediaOutput> MediaOutput;

	UPROPERTY(EditAnywhere, Category="MediaViewportCapture")
	FMediaCaptureOptions CaptureOptions;

	UPROPERTY()
	TEnumAsByte<EViewModeIndex> ViewMode;
};

/**
 * FMediaFrameworkCaptureCameraViewportCameraOutputInfo
 */
USTRUCT()
struct FMediaFrameworkCaptureCameraViewportCameraOutputInfo
{
	GENERATED_BODY()

	FMediaFrameworkCaptureCameraViewportCameraOutputInfo();

	UPROPERTY(EditAnywhere, Category="MediaViewportCapture", meta=(DisplayName="Cameras"))
	TArray<TLazyObjectPtr<AActor>> LockedActors;

	UPROPERTY(EditAnywhere, Category="MediaViewportCapture")
	TObjectPtr<UMediaOutput> MediaOutput;

	UPROPERTY(EditAnywhere, Category="MediaViewportCapture")
	FMediaCaptureOptions CaptureOptions;

	UPROPERTY()
	TEnumAsByte<EViewModeIndex> ViewMode;

private:
	//DEPRECATED 4.21 The type of LockedCameraActors has changed and will be removed from the code base in a future release. Use LockedActors.
	UPROPERTY()
	TArray<TObjectPtr<AActor>> LockedCameraActors_DEPRECATED;
	friend UMediaFrameworkWorldSettingsAssetUserData;
};


/**
 * FMediaFrameworkCaptureRenderTargetCameraOutputInfo
 */
USTRUCT()
struct FMediaFrameworkCaptureRenderTargetCameraOutputInfo
{
	GENERATED_BODY()

	FMediaFrameworkCaptureRenderTargetCameraOutputInfo();

	UPROPERTY(EditAnywhere, Category="MediaRenderTargetCapture")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY(EditAnywhere, Category="MediaRenderTargetCapture")
	TObjectPtr<UMediaOutput> MediaOutput;

	UPROPERTY(EditAnywhere, Category="MediaRenderTargetCapture")
	FMediaCaptureOptions CaptureOptions;
};


/**
 * UMediaFrameworkCaptureCameraViewportAssetUserData
 */
UCLASS(MinimalAPI, config = Editor)
class UMediaFrameworkWorldSettingsAssetUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UMediaFrameworkWorldSettingsAssetUserData();

	UPROPERTY(EditAnywhere, config, Category="Media Render Target Capture", meta=(ShowOnlyInnerProperties))
	TArray<FMediaFrameworkCaptureRenderTargetCameraOutputInfo> RenderTargetCaptures;

	UPROPERTY(EditAnywhere, config, Category="Media Viewport Capture", meta=(ShowOnlyInnerProperties))
	TArray<FMediaFrameworkCaptureCameraViewportCameraOutputInfo> ViewportCaptures;

	/**
	 * Capture the current viewport. It may be the level editor active viewport or a PIE instance launch with "New Editor Window PIE".
	 * @note The behavior is different from MediaCapture.CaptureActiveSceneViewport. Here we can capture the editor viewport (since we are in the editor).
	 * @note If the viewport is the level editor active viewport, then all inputs will be disabled and the viewport will always rendered.
	 */
	UPROPERTY(EditAnywhere, config, Category="Media Current Viewport Capture", meta=(DisplayName="Current Viewport"))
	FMediaFrameworkCaptureCurrentViewportOutputInfo CurrentViewportMediaOutput;

public:
	virtual void Serialize(FArchive& Ar) override;
};

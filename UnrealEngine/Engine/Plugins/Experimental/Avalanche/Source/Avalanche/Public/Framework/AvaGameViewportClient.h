// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameViewportClient.h"
#include "Viewport/AvaCameraManager.h"
#include "SceneTypes.h"
#include "AvaGameViewportClient.generated.h"

class AAvaViewportCameraActor;
class FPrimitiveComponentId;
class FViewElementDrawer;
class UTextureRenderTarget2D;
struct FMinimalViewInfo;
struct FSceneViewInitOptions;
struct FSceneViewProjectionData;

UCLASS(DisplayName = "Motion Design Game Viewport Client")
class AVALANCHE_API UAvaGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()
	
public:
	UAvaGameViewportClient();

	//~ Begin FViewportClient
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;
	virtual bool IsStatEnabled(const FString& InName) const override;
	//~ End FViewportClient

	void SetCameraCutThisFrame();

	void SetRenderTarget(UTextureRenderTarget2D* InRenderTarget);

	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget.Get(); }

	TSharedRef<FAvaCameraManager> GetCameraManager() const { return CameraManager; }

private:
	bool CalcSceneViewInitOptions(FSceneViewInitOptions& OutInitOptions
		, FViewport* InViewport
		, FViewElementDrawer* ViewDrawer = nullptr
		, int32 StereoViewIndex = INDEX_NONE);

	bool GetProjectionData(FViewport* InViewport
		, FSceneViewProjectionData& ProjectionData
		, int32 StereoViewIndex = INDEX_NONE) const;

	FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily
		, FViewport* InViewport
		, FViewElementDrawer* ViewDrawer = nullptr
		, int32 StereoViewIndex = INDEX_NONE);

	TArray<FSceneViewStateReference> ViewStates;

	TWeakObjectPtr<UTextureRenderTarget2D> RenderTarget;

	TSharedRef<FAvaCameraManager> CameraManager;

	bool bCameraCutThisFrame = false;
};

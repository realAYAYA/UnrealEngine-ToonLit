// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Vector.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class FAvaCameraManager;
class FName;
class FSceneView;
class IAvaSequencePlaybackObject;
class UAvaCanvasComponent;
class UCameraComponent;
class UWorld;
struct FMinimalViewInfo;
struct FPostProcessSettings;

DECLARE_MULTICAST_DELEGATE_TwoParams(FAvaOnViewTargetChanged, FAvaCameraManager*, AActor*);

class AVALANCHE_API FAvaCameraManager : public TSharedFromThis<FAvaCameraManager>
{
public:
	FAvaCameraManager();
	virtual ~FAvaCameraManager() = default;

	void Init(IAvaSequencePlaybackObject* InPlaybackObject, bool bInCanvasController);

	bool HasViewTarget() const;
	AActor* GetViewTarget() const;
	bool SetViewTarget(AActor* InNewViewTarget);

	UCameraComponent* GetCachedCameraComponent() const;

#if WITH_EDITOR
	bool SetDefaultViewTarget(UWorld* InWorld, FName InDefaultViewTargetName);
#endif

	void SetCameraCut() { bCameraCut = true; }
	bool ShouldEnableCameraCut() const { return bCameraCut; }
	void ResetCameraCut() { bCameraCut = false; }

	FVector GetViewLocation() const;
	FRotator GetViewRotation() const;
	void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	void SetViewPoint(const FVector& InLocation, const FRotator& InRotation);

	FAvaOnViewTargetChanged& GetOnViewTargetChanged() { return OnViewTargetChanged; }

	void SetCameraViewInfo(FMinimalViewInfo& InOutViewInfo) const;
	void GetExtraPostProcessBlends(TArray<FPostProcessSettings>& OutSettings, TArray<float>& OutWeights) const;

	/** Gets the extra blends from the camera, if we have any. */
	void ApplyExtraPostProcessBlends(FSceneView* InView) const;

	virtual TArray<AActor*> GetAvailableCameras(UWorld* InWorld);

protected:
	TWeakObjectPtr<AActor> ViewTargetWeak;
	TWeakObjectPtr<UCameraComponent> CameraComponentWeak;
	bool bCameraCut;
	FAvaOnViewTargetChanged OnViewTargetChanged;

	void OnUpdateCameraCut(UObject* InCameraObject, bool bInJump);
};

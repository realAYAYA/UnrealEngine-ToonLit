// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_1
#include "Engine/EngineTypes.h"
#endif
#include "Engine/HitResult.h"
#include "LiveLinkRole.h"
#include "Misc/CoreMiscDefines.h"
#include "UObject/Interface.h"
#include "UObject/ScriptInterface.h"

#include "IVirtualCameraController.generated.h"

class IBlendableInterface;
class IVirtualCameraOptions;
class IVirtualCameraPresetContainer;
class UCineCameraComponent;
class ULevelSequencePlaybackController;
class USceneCaptureComponent2D;
class UWorld;

UENUM(BlueprintType)
enum class EVirtualCameraFocusMethod : uint8
{
	/* Depth of Field disabled entirely */
	None,

	/* User controls focus distance directly */
	Manual,

	/* Focus distance is locked onto a specific point in relation to an actor */
	Tracking,

	/* Focus distance automatically changes to focus on actors in a specific screen location */
	Auto,
};

USTRUCT(BlueprintType)
struct FTrackingOffset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	FVector Translation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	FRotator Rotation;

	FTransform AsTransform() const
	{
		return FTransform(Rotation, Translation);
	}

	FTrackingOffset()
		: Translation(EForceInit::ForceInitToZero), Rotation(EForceInit::ForceInitToZero)
	{

	};
};

USTRUCT(BlueprintType)
struct FVirtualCameraTransform
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualCamera")
	FTransform Transform;
};

DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(FVirtualCameraTransform, FPreSetVirtualCameraTransform, FVirtualCameraTransform, CameraTransform);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnActorClickedDelegate, FHitResult, HitResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVirtualCameraTickDelegateGroup, float, DeltaTime);
DECLARE_DYNAMIC_DELEGATE_OneParam(FVirtualCameraTickDelegate, float, DeltaTime);

UINTERFACE(Blueprintable)
class VIRTUALCAMERA_API UVirtualCameraController: public UInterface
{
	GENERATED_BODY()
};

class VIRTUALCAMERA_API IVirtualCameraController
{
	GENERATED_BODY()
	
public:

	/** Returns the target camera that is used to create the streamed view. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Component")
	UCineCameraComponent* GetStreamedCameraComponent() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Component")
	USceneCaptureComponent2D* GetSceneCaptureComponent() const;

	/** Returns the VirtualCamera's Sequence Controller. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Sequencer")
	ULevelSequencePlaybackController* GetSequenceController() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Presets")
	TScriptInterface<IVirtualCameraPresetContainer> GetPresetContainer();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Options")
	TScriptInterface<IVirtualCameraOptions> GetOptions();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Movement")
	FLiveLinkSubjectRepresentation GetLiveLinkRepresentation() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Movement")
	void SetLiveLinkRepresentation(const FLiveLinkSubjectRepresentation& InLiveLinkRepresenation);

	virtual bool StartStreaming() PURE_VIRTUAL(IVirtualCameraController::StartStreaming, return false;);

	virtual bool StopStreaming() PURE_VIRTUAL(IVirtualCameraController::StopStreaming, return false;);

	virtual UWorld* GetControllerWorld() PURE_VIRTUAL(IVirtualCameraController::GetControllerWorld, return nullptr;);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Streaming")
	bool IsStreaming() const;

	/** Check whether settings should save when stream is stopped. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Settings")
	bool ShouldSaveSettingsOnStopStreaming() const;

	/** Sets whether settings should be saved when stream is stopped. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Settings")
	void SetSaveSettingsOnStopStreaming(bool bShouldSettingsSave);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Movement")
	FTransform GetRelativeTransform() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera")
	void AddBlendableToCamera(const TScriptInterface<IBlendableInterface>& InBlendableToAdd, float InWeight);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Focus")
	void SetTrackedActorForFocus(AActor* InActorToTrack, const FVector& TrackingPointOffset);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Focus")
	void SetFocusVisualization(bool bInShowFocusVisualization);

	/** Delegate will be executed before transform is set onto VirtualCamera. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Movement")
	void SetBeforeSetVirtualCameraTransformDelegate(const FPreSetVirtualCameraTransform& InDelegate);

	/** Delegate will be executed when an actor in the scene was clicked/touched. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera | Focus")
	void SetOnActorClickedDelegate(const FOnActorClickedDelegate& InDelegate);

	/** Adds a delegate that will be executed every tick while streaming. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera")
	void AddOnVirtualCameraUpdatedDelegate(const FVirtualCameraTickDelegate& InDelegate);

	/** Remove delegate that is executed every tick while streaming. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VirtualCamera")
	void RemoveOnVirtualCameraUpdatedDelegate(const FVirtualCameraTickDelegate& InDelegate);
};

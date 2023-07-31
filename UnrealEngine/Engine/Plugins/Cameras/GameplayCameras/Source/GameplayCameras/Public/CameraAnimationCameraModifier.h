// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/TypeHash.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Camera/CameraTypes.h"
#include "Camera/CameraModifier.h"
#include "Containers/SparseArray.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CameraAnimationCameraModifier.generated.h"

class UCameraAnimationSequence;
class UCameraAnimationSequenceCameraStandIn;
class UCameraAnimationSequencePlayer;
struct FMinimalViewInfo;

UENUM()
enum class ECameraAnimationPlaySpace : uint8
{
	/** This anim is applied in camera space. */
	CameraLocal,
	/** This anim is applied in world space. */
	World,
	/** This anim is applied in a user-specified space (defined by UserPlaySpaceMatrix). */
	UserDefined,
};

UENUM()
enum class ECameraAnimationEasingType : uint8
{
	Linear,
	Sinusoidal,
	Quadratic,
	Cubic,
	Quartic,
	Quintic,
	Exponential,
	Circular,
};

/** Parameter struct for adding new camera animations to UCameraAnimationCameraModifier */
USTRUCT(BlueprintType)
struct FCameraAnimationParams
{
	GENERATED_BODY()

	/** Time scale for playing the new camera animation */
	UPROPERTY(BlueprintReadWrite, Category="Camera Animation")
	float PlayRate = 1.f;
	/** Global scale to use for the new camera animation */
	UPROPERTY(BlueprintReadWrite, Category="Camera Animation")
	float Scale = 1.f;

	/** Ease-in function type */
	UPROPERTY(BlueprintReadWrite, Category="Camera Animation")
	ECameraAnimationEasingType EaseInType = ECameraAnimationEasingType::Linear;
	/** Ease-in duration in seconds */
	UPROPERTY(BlueprintReadWrite, Category="Camera Animation")
	float EaseInDuration = 0.f;

	/** Ease-out function type */
	UPROPERTY(BlueprintReadWrite, Category="Camera Animation")
	ECameraAnimationEasingType EaseOutType = ECameraAnimationEasingType::Linear;
	/** Ease-out duration in seconds */
	UPROPERTY(BlueprintReadWrite, Category="Camera Animation")
	float EaseOutDuration = 0.f;

	/** Whether the camera animation should loop */
	UPROPERTY(BlueprintReadWrite, Category="Camera Animation")
	bool bLoop = false;
	/** Whether the camera animation should have a random start time */
	UPROPERTY(BlueprintReadWrite, Category="Camera Animation")
	bool bRandomStartTime = false;
	/** Override the duration of the animation with a new duration (including blends) */
	UPROPERTY(BlueprintReadWrite, Category="Camera Animation")
	float DurationOverride = 0.f;

	/** The transform space to use for the new camera shake */
	UPROPERTY(BlueprintReadWrite, Category="Camera Animation")
	ECameraAnimationPlaySpace PlaySpace = ECameraAnimationPlaySpace::CameraLocal;
	/** User space to use when PlaySpace is UserDefined */
	UPROPERTY(BlueprintReadWrite, Category="Camera Animation")
	FRotator UserPlaySpaceRot = FRotator::ZeroRotator;
};

/** A handle to a camera animation running in UCameraAnimationCameraModifier */
USTRUCT(BlueprintType)
struct FCameraAnimationHandle
{
	GENERATED_BODY()

	static FCameraAnimationHandle Invalid;

	FCameraAnimationHandle() : InstanceID(MAX_uint16), InstanceSerial(0) {}
	FCameraAnimationHandle(uint16 InInstanceID, uint16 InInstanceSerial) : InstanceID(InInstanceID), InstanceSerial(InInstanceSerial) {}

	bool IsValid() const
	{
		return InstanceID != MAX_int16;
	}

	friend bool operator==(FCameraAnimationHandle A, FCameraAnimationHandle B)
	{
		return A.InstanceID == B.InstanceID && A.InstanceSerial == B.InstanceSerial;
	}
	friend bool operator!=(FCameraAnimationHandle A, FCameraAnimationHandle B)
	{
		return !(A == B);
	}
	friend bool operator<(FCameraAnimationHandle A, FCameraAnimationHandle B)
	{
		return A.InstanceID < B.InstanceID;
	}
	friend uint32 GetTypeHash(FCameraAnimationHandle In)
	{
		return HashCombine(In.InstanceID, In.InstanceSerial);
	}

private:
	uint16 InstanceID;
	uint16 InstanceSerial;

	friend class UCameraAnimationCameraModifier;
};

/**
 * Information about an active camera animation inside UCameraAnimationCameraModifier.
 */
USTRUCT()
struct GAMEPLAYCAMERAS_API FActiveCameraAnimationInfo
{
	GENERATED_BODY()

	FActiveCameraAnimationInfo();

	/** Whether this is a valid, ongoing camera animation */
	bool IsValid() const { return Sequence != nullptr; }

	/** The sequence to use for the animation. */
	UPROPERTY()
	TObjectPtr<UCameraAnimationSequence> Sequence;

	/** The parameters for playing the animation. */
	UPROPERTY()
	FCameraAnimationParams Params;

	/** A reference handle for use with UCameraAnimationCameraModifier. */
	UPROPERTY()
	FCameraAnimationHandle Handle;

	/** The player for playing the animation. */
	UPROPERTY(Transient)
	TObjectPtr<UCameraAnimationSequencePlayer> Player;

	/** Standin for the camera actor and components */
	UPROPERTY(Transient)
	TObjectPtr<UCameraAnimationSequenceCameraStandIn> CameraStandIn;

	/** Current time into easing in */
	UPROPERTY()
	float EaseInCurrentTime;

	/** Current time into easing out */
	UPROPERTY()
	float EaseOutCurrentTime;

	/** Whether easing in is ongoing */
	UPROPERTY()
	bool bIsEasingIn;

	/** Whether easing out is ongoing */
	UPROPERTY()
	bool bIsEasingOut;
};

/**
 * A camera modifier that plays camera animation sequences.
 */
UCLASS(config=Camera)
class GAMEPLAYCAMERAS_API UCameraAnimationCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	UCameraAnimationCameraModifier(const FObjectInitializer& ObjectInitializer);

	/**
	 * Play a new camera animation sequence.
	 * @param Sequence		The sequence to use for the new camera animation.
	 * @param Params		The parameters for the new camera animation instance.
	 */
	UFUNCTION(BlueprintCallable, Category="Camera Animation")
	FCameraAnimationHandle PlayCameraAnimation(UCameraAnimationSequence* Sequence, FCameraAnimationParams Params);

	/**
	 * Returns whether the given camera animation is playing.
	 * @param Handle		A handle to a previously started camera animation.
	 * @return				Whether the corresponding camera animation is playing or not.
	 */
	UFUNCTION(BlueprintCallable, Category="Camera Animation")
	bool IsCameraAnimationActive(const FCameraAnimationHandle& Handle) const;
	
	/** 
	 * Stops the given camera animation instance.
	 * @param Hanlde		A handle to a previously started camera animation.
	 * @param bImmediate	True to stop it right now and ignore blend out, false to let it blend out as indicated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera Animation")
	void StopCameraAnimation(const FCameraAnimationHandle& Handle, bool bImmediate = false);
	
	/**
	 * Stop playing all instances of the given camera animation sequence.
	 * @param Sequence		The sequence of which to stop all instances.
	 * @param bImmediate	True to stop it right now and ignore blend out, false to let it blend out as indicated.
	 */
	UFUNCTION(BlueprintCallable, Category="Camera Animation")
	void StopAllCameraAnimationsOf(UCameraAnimationSequence* Sequence, bool bImmediate = false);
	
	/**
	 * Stop all camera animation instances.
	 * @param bImmediate	True to stop it right now and ignore blend out, false to let it blend out as indicated.
	 */
	UFUNCTION(BlueprintCallable, Category="Camera Animation")
	virtual void StopAllCameraAnimations(bool bImmediate = false);

	// UCameraModifier interface
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;

public:

	UFUNCTION(BlueprintPure, Category="Camera Animation", meta=(WorldContext="WorldContextObject"))
	static UCameraAnimationCameraModifier* GetCameraAnimationCameraModifier(const UObject* WorldContextObject, int32 PlayerIndex);

	UFUNCTION(BlueprintPure, Category="Camera Animation", meta=(WorldContext="WorldContextObject"))
	static UCameraAnimationCameraModifier* GetCameraAnimationCameraModifierFromID(const UObject* WorldContextObject, int32 ControllerID);

	UFUNCTION(BlueprintPure, Category="Camera Animation")
	static UCameraAnimationCameraModifier* GetCameraAnimationCameraModifierFromPlayerController(const APlayerController* PlayerController);

protected:

	static float EvaluateEasing(ECameraAnimationEasingType EasingType, float Interp);

	int32 FindInactiveCameraAnimation();
	const FActiveCameraAnimationInfo* GetActiveCameraAnimation(const FCameraAnimationHandle& Handle) const;
	FActiveCameraAnimationInfo* GetActiveCameraAnimation(const FCameraAnimationHandle& Handle);
	void DeactivateCameraAnimation(int32 Index);

	void TickAllAnimations(float DeltaTime, FMinimalViewInfo& InOutPOV);
	void TickAnimation(FActiveCameraAnimationInfo& CameraAnimation, float DeltaTime, FMinimalViewInfo& InOutPOV);

protected:

	/** List of active camera animation instances */
	UPROPERTY()
	TArray<FActiveCameraAnimationInfo> ActiveAnimations;

	/** Next serial number to use for a camera animation instance */
	UPROPERTY()
	uint16 NextInstanceSerialNumber;
};

/**
 * Blueprint function library for autocasting a player camera manager into the camera animation camera modifier.
 * This prevents breaking Blueprints now that APlayerCameraManager::StartCameraShake returns the base class.
 */
UCLASS()
class GAMEPLAYCAMERAS_API UGameplayCamerasFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Camera Animation", meta=(BlueprintAutocast))
	static UCameraAnimationCameraModifier* Conv_CameraAnimationCameraModifier(APlayerCameraManager* PlayerCameraManager);

	UFUNCTION(BlueprintPure, Category = "Camera Animation", meta = (BlueprintAutocast))
	static ECameraShakePlaySpace Conv_CameraShakePlaySpace(ECameraAnimationPlaySpace CameraAnimationPlaySpace);

	UFUNCTION(BlueprintPure, Category = "Camera Animation", meta = (BlueprintAutocast))
	static ECameraAnimationPlaySpace Conv_CameraAnimationPlaySpace(ECameraShakePlaySpace CameraShakePlaySpace);
};


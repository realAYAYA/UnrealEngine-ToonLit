// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "AlphaBlend.h"
#include "RootMotionModifier.generated.h"

class UMotionWarpingComponent;
class UAnimNotifyState_MotionWarping;
class URootMotionModifier;
class USceneComponent;

/** 
 * Context passed to any active root motion modifier during the update phase. 
 * Contains relevant data from the animation that contributed to root motion this frame (or in the past when replaying saved moves)
 */
USTRUCT()
struct FMotionWarpingUpdateContext
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<const UAnimSequenceBase> Animation = nullptr;

	UPROPERTY()
	float PreviousPosition = 0.f;

	UPROPERTY()
	float CurrentPosition = 0.f;

	UPROPERTY()
	float Weight = 0.f;

	UPROPERTY()
	float PlayRate = 1.f;

	UPROPERTY()
	float DeltaSeconds = 0.f;
};

/** The possible states of a Root Motion Modifier */
UENUM(BlueprintType)
enum class ERootMotionModifierState : uint8
{
	/** The modifier is waiting for the animation to hit the warping window */
	Waiting,

	/** The modifier is active and currently affecting the final root motion */
	Active,

	/** The modifier has been marked for removal. Usually because the warping window is done */
	MarkedForRemoval,

	/** The modifier will remain in the list (as long as the window is active) but will not modify the root motion */
	Disabled
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnRootMotionModifierDelegate, UMotionWarpingComponent*, MotionWarpingComp, URootMotionModifier*, RootMotionModifier);

// URootMotionModifier
///////////////////////////////////////////////////////////////

UCLASS(Abstract, BlueprintType, EditInlineNew)
class MOTIONWARPING_API URootMotionModifier : public UObject
{
	GENERATED_BODY()

public:

	/** Source of the root motion we are warping */
	UPROPERTY(BlueprintReadOnly, Category = "Defaults")
	TWeakObjectPtr<const UAnimSequenceBase> Animation = nullptr;

	/** Start time of the warping window */
	UPROPERTY(BlueprintReadOnly, Category = "Defaults")
	float StartTime = 0.f;

	/** End time of the warping window */
	UPROPERTY(BlueprintReadOnly, Category = "Defaults")
	float EndTime = 0.f;

	/** Previous playback time of the animation */
	UPROPERTY(BlueprintReadOnly, Category = "Defaults")
	float PreviousPosition = 0.f;

	/** Current playback time of the animation */
	UPROPERTY(BlueprintReadOnly, Category = "Defaults")
	float CurrentPosition = 0.f;

	/** Current blend weight of the animation */
	UPROPERTY(BlueprintReadOnly, Category = "Defaults")
	float Weight = 0.f;

	/** Character owner transform at the time this modifier becomes active */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Defaults")
	FTransform StartTransform;

	/** Actual playback time when the modifier becomes active */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Defaults")
	float ActualStartTime = 0.f;

	/** Delegate called when this modifier is activated (starts affecting the root motion) */
	UPROPERTY()
	FOnRootMotionModifierDelegate OnActivateDelegate;

	/** Delegate called when this modifier updates while active (affecting the root motion) */
	UPROPERTY()
	FOnRootMotionModifierDelegate OnUpdateDelegate;

	/** Delegate called when this modifier is deactivated (stops affecting the root motion) */
	UPROPERTY()
	FOnRootMotionModifierDelegate OnDeactivateDelegate;

	URootMotionModifier(const FObjectInitializer& ObjectInitializer);

	/** Called when the state of the modifier changes */
	virtual void OnStateChanged(ERootMotionModifierState LastState);

	/** Sets the state of the modifier */
	void SetState(ERootMotionModifierState NewState);

	/** Returns the state of the modifier */
	FORCEINLINE ERootMotionModifierState GetState() const { return State; }

	/** Returns a pointer to the component that owns this modifier */
	UMotionWarpingComponent* GetOwnerComponent() const;

	/** Returns a pointer to the character that owns the component that owns this modifier */
	class ACharacter* GetCharacterOwner() const;

	virtual void Update(const FMotionWarpingUpdateContext& Context);
	virtual FTransform ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds) { return FTransform::Identity; }

	FORCEINLINE const UAnimSequenceBase* GetAnimation() const { return Animation.Get(); }

private:

	friend UMotionWarpingComponent;

	/** Current state */
	UPROPERTY()
	ERootMotionModifierState State = ERootMotionModifierState::Waiting;
};

// URootMotionModifier_Warp
///////////////////////////////////////////////////////////////

/** Represents a point of alignment in the world */
USTRUCT(BlueprintType)
struct MOTIONWARPING_API FMotionWarpingTarget
{
	GENERATED_BODY()

	/** Unique name for this warp target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	FName Name;

	/** When the warp target is created from a component this stores the location of the component at the time of creation, otherwise its the location supplied by the user */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	FVector Location;

	/** When the warp target is created from a component this stores the rotation of the component at the time of creation, otherwise its the rotation supplied by the user */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	FRotator Rotation;

	/** Optional component used to calculate the final target transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	TWeakObjectPtr<const USceneComponent> Component;

	/** Optional bone name in the component used to calculate the final target transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	FName BoneName;

	/** Whether the target transform calculated from a component and an optional bone should be updated during the warp */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	bool bFollowComponent;

	FMotionWarpingTarget()
		: Name(NAME_None), Location(FVector::ZeroVector), Rotation(FRotator::ZeroRotator), Component(nullptr), BoneName(NAME_None), bFollowComponent(false) {}

	FMotionWarpingTarget(const FName& InName, const FTransform& InTransform)
		: Name(InName), Location(InTransform.GetLocation()), Rotation(InTransform.Rotator()), Component(nullptr), BoneName(NAME_None), bFollowComponent(false) {}

	FMotionWarpingTarget(const FName& InName, const USceneComponent* InComp, FName InBoneName, bool bInbFollowComponent);

	FTransform GetTargetTrasform() const;

	FORCEINLINE FVector GetLocation() const { return GetTargetTrasform().GetLocation(); }
	FORCEINLINE FQuat GetRotation() const { return GetTargetTrasform().GetRotation(); }
	FORCEINLINE FRotator Rotator() const { return GetTargetTrasform().Rotator(); }

	FORCEINLINE bool operator==(const FMotionWarpingTarget& Other) const
	{
		return Other.Name == Name && Other.Location.Equals(Location) && Other.Rotation.Equals(Rotation) && Other.Component == Component && Other.BoneName == BoneName && Other.bFollowComponent == bFollowComponent;
	}

	FORCEINLINE bool operator!=(const FMotionWarpingTarget& Other) const
	{
		return Other.Name != Name || !Other.Location.Equals(Location) || !Other.Rotation.Equals(Rotation) || Other.Component != Component || Other.BoneName != BoneName || Other.bFollowComponent != bFollowComponent;
	}

	static FTransform GetTargetTransformFromComponent(const USceneComponent* Comp, const FName& BoneName);

};

UENUM(BlueprintType)
enum class EMotionWarpRotationType : uint8
{
	/** Character rotates to match the rotation of the sync point */
	Default,

	/** Character rotates to face the sync point */
	Facing,
};

/** Method used to extract the warp point from the animation */
UENUM(BlueprintType)
enum class EWarpPointAnimProvider : uint8
{
	/** No warp point is provided */
	None,

	/** Warp point defined by a 'hard-coded' transform  user can enter through the warping notify */
	Static,

	/** Warp point defined by a bone */
	Bone
};

UCLASS(Abstract)
class MOTIONWARPING_API URootMotionModifier_Warp : public URootMotionModifier
{
	GENERATED_BODY()

public:

	/** Name used to find the warp target for this modifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ExposeOnSpawn))
	FName WarpTargetName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	EWarpPointAnimProvider WarpPointAnimProvider = EWarpPointAnimProvider::None;

	//@TODO: Hide from the UI when Target != Static
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "WarpPointAnimProvider == EWarpPointAnimProvider::Static"))
	FTransform WarpPointAnimTransform = FTransform::Identity;

	//@TODO: Hide from the UI when Target != Bone
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "WarpPointAnimProvider == EWarpPointAnimProvider::Bone"))
	FName WarpPointAnimBoneName = NAME_None;

	/** Whether to warp the translation component of the root motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bWarpTranslation = true;

	/** Whether to ignore the Z component of the translation. Z motion will remain untouched */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "bWarpTranslation"))
	bool bIgnoreZAxis = true;

	/** Easing function used when adding translation. Only relevant when there is no translation in the animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "bWarpTranslation"))
	EAlphaBlendOption AddTranslationEasingFunc = EAlphaBlendOption::Linear;

	/** Custom curve used to add translation when there is none to warp. Only relevant when AddTranslationEasingFunc is set to Custom */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend", meta = (EditCondition = "AddTranslationEasingFunc==EAlphaBlendOption::Custom", EditConditionHides))
	TObjectPtr<class UCurveFloat> AddTranslationEasingCurve = nullptr;

	/** Whether to warp the rotation component of the root motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bWarpRotation = true;

	/** Whether rotation should be warp to match the rotation of the sync point or to face the sync point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "bWarpRotation"))
	EMotionWarpRotationType RotationType;

	/**
	 * Allow to modify how fast the rotation is warped.
	 * e.g if the window duration is 2sec and this is 0.5, the target rotation will be reached in 1sec instead of 2sec
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "bWarpRotation"))
	float WarpRotationTimeMultiplier = 1.f;

	URootMotionModifier_Warp(const FObjectInitializer& ObjectInitializer);

	//~ Begin FRootMotionModifier Interface
	virtual void Update(const FMotionWarpingUpdateContext& Context) override;
	//~ End FRootMotionModifier Interface

	/** Event called during update if the target transform changes while the warping is active */
	virtual void OnTargetTransformChanged();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	void PrintLog(const FString& Name, const FTransform& OriginalRootMotion, const FTransform& WarpedRootMotion) const;
#endif

	FORCEINLINE FVector GetTargetLocation() const { return CachedTargetTransform.GetLocation(); }
	FORCEINLINE FRotator GetTargetRotator() const { return GetTargetRotation().Rotator(); }
	FQuat GetTargetRotation() const;

	FQuat WarpRotation(const FTransform& RootMotionDelta, const FTransform& RootMotionTotal, float DeltaSeconds);

protected:

	UPROPERTY()
	FTransform CachedTargetTransform = FTransform::Identity;

	/** Cached of the offset from the warp target. Used to calculate the final target transform when a warp target is defined in the animation */
	TOptional<FTransform> CachedOffsetFromWarpPoint;
};

// URootMotionModifier_SimpleWarp. 
// DEPRECATED in favor of URootMotionModifier_SkewWarp (kept for reference)
///////////////////////////////////////////////////////////////

UCLASS(Deprecated, meta = (DisplayName = "Simple Warp"))
class MOTIONWARPING_API UDEPRECATED_RootMotionModifier_SimpleWarp : public URootMotionModifier_Warp
{
	GENERATED_BODY()

public:

	UDEPRECATED_RootMotionModifier_SimpleWarp(const FObjectInitializer& ObjectInitializer);
	virtual FTransform ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds) override;
};

// URootMotionModifier_Scale
///////////////////////////////////////////////////////////////

UCLASS(meta = (DisplayName = "Scale"))
class MOTIONWARPING_API URootMotionModifier_Scale : public URootMotionModifier
{
	GENERATED_BODY()

public:

	/** Vector used to scale each component of the translation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FVector Scale = FVector(1.f);

	URootMotionModifier_Scale(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}

	virtual FTransform ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds) override
	{
		FTransform FinalRootMotion = InRootMotion;
		FinalRootMotion.ScaleTranslation(Scale);
		return FinalRootMotion;
	}

	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static URootMotionModifier_Scale* AddRootMotionModifierScale(
		UPARAM(DisplayName = "Motion Warping Comp") UMotionWarpingComponent* InMotionWarpingComp,
		UPARAM(DisplayName = "Animation") const UAnimSequenceBase* InAnimation,
		UPARAM(DisplayName = "Start Time") float InStartTime,
		UPARAM(DisplayName = "End Time") float InEndTime,
		UPARAM(DisplayName = "Scale") FVector InScale = FVector(1.f));
};
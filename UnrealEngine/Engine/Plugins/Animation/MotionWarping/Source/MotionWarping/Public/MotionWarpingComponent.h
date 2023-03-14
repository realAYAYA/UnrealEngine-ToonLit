// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "RootMotionModifier.h"
#include "MotionWarpingComponent.generated.h"

class ACharacter;
class UAnimSequenceBase;
class UCharacterMovementComponent;
class UMotionWarpingComponent;
class UAnimNotifyState_MotionWarping;

DECLARE_LOG_CATEGORY_EXTERN(LogMotionWarping, Log, All);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
struct MOTIONWARPING_API FMotionWarpingCVars
{
	static TAutoConsoleVariable<int32> CVarMotionWarpingDisable;
	static TAutoConsoleVariable<int32> CVarMotionWarpingDebug;
	static TAutoConsoleVariable<float> CVarMotionWarpingDrawDebugDuration;
};
#endif

USTRUCT(BlueprintType)
struct FMotionWarpingWindowData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	TObjectPtr<UAnimNotifyState_MotionWarping> AnimNotify = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	float StartTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	float EndTime = 0.f;
};

UCLASS()
class MOTIONWARPING_API UMotionWarpingUtilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Extract bone pose in local space for all bones in BoneContainer. If Animation is a Montage the pose is extracted from the first track */
	static void ExtractLocalSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCompactPose& OutPose);

	/** Extract bone pose in component space for all bones in BoneContainer. If Animation is a Montage the pose is extracted from the first track */
	static void ExtractComponentSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCSPose<FCompactPose>& OutPose);

	/** Extract Root Motion transform from a contiguous position range */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static FTransform ExtractRootMotionFromAnimation(const UAnimSequenceBase* Animation, float StartTime, float EndTime);

	/** Extract root bone transform at a given time */
	static FTransform ExtractRootTransformFromAnimation(const UAnimSequenceBase* Animation, float Time);

	/** @return All the MotionWarping windows within the supplied animation */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static void GetMotionWarpingWindowsFromAnimation(const UAnimSequenceBase* Animation, TArray<FMotionWarpingWindowData>& OutWindows);

	/** @return All the MotionWarping windows within the supplied animation for a given Warp Target */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static void GetMotionWarpingWindowsForWarpTargetFromAnimation(const UAnimSequenceBase* Animation, FName WarpTargetName, TArray<FMotionWarpingWindowData>& OutWindows);

	/** @return root transform relative to the warp point bone at the supplied time */
	static FTransform CalculateRootTransformRelativeToWarpPointAtTime(const ACharacter& Character, const UAnimSequenceBase* Animation, float Time, const FName& WarpPointBoneName);

	/** @return root transform relative to the warp point transform at the supplied time */
	static FTransform CalculateRootTransformRelativeToWarpPointAtTime(const ACharacter& Character, const UAnimSequenceBase* Animation, float Time, const FTransform& WarpPointTransform);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMotionWarpingPreUpdate, class UMotionWarpingComponent*, MotionWarpingComp);

UCLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class MOTIONWARPING_API UMotionWarpingComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Whether to look inside animations within montage when looking for warping windows */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bSearchForWindowsInAnimsWithinMontages;

	/** Event called before Root Motion Modifiers are updated */
	UPROPERTY(BlueprintAssignable, Category = "Motion Warping")
	FMotionWarpingPreUpdate OnPreUpdate;

	UMotionWarpingComponent(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const;

	/** Gets the character this component belongs to */
	FORCEINLINE ACharacter* GetCharacterOwner() const { return CharacterOwner.Get(); }

	/** Returns the list of root motion modifiers */
	FORCEINLINE const TArray<URootMotionModifier*>& GetModifiers() const { return Modifiers; }

	/** Check if we contain a RootMotionModifier for the supplied animation and time range */
	bool ContainsModifier(const UAnimSequenceBase* Animation, float StartTime, float EndTime) const;

	/** Add a new modifier */
	int32 AddModifier(URootMotionModifier* Modifier);

	/** Mark all the modifiers as Disable */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	void DisableAllRootMotionModifiers();

	URootMotionModifier* AddModifierFromTemplate(URootMotionModifier* Template, const UAnimSequenceBase* Animation, float StartTime, float EndTime);

	/** Find the target associated with a specified name */
	FORCEINLINE const FMotionWarpingTarget* FindWarpTarget(const FName& WarpTargetName) const 
	{ 
		return WarpTargets.FindByPredicate([&WarpTargetName](const FMotionWarpingTarget& WarpTarget){ return WarpTarget.Name == WarpTargetName; });
	}

	/** Adds or update a warp target */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	void AddOrUpdateWarpTarget(const FMotionWarpingTarget& WarpTarget);

	/** 
	 * Create and adds or update a target associated with a specified name 
	 * @param WarpTargetName Warp target identifier
	 * @param TargetTransform Transform used to set the location and rotation for the warp target
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	void AddOrUpdateWarpTargetFromTransform(FName WarpTargetName, FTransform TargetTransform);

	/**
	 * Create and adds or update a target associated with a specified name
	 * @param WarpTargetName Warp target identifier
	 * @param Component Scene Component used to get the target transform
	 * @param BoneName Optional bone or socket in the component used to get the target transform. 
	 * @param bFollowComponent Whether the target transform should update while the warping is active. Useful for tracking moving targets.
	 *		  Note that this will be one frame off if our owner ticks before the target actor. Add tick pre-requisites to avoid this.
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	void AddOrUpdateWarpTargetFromComponent(FName WarpTargetName, const USceneComponent* Component, FName BoneName, bool bFollowComponent);

	/**
	 * Create and adds or update a target associated with a specified name
	 * @param WarpTargetName Warp target identifier
	 * @param TargetLocation Location for the warp target
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	void AddOrUpdateWarpTargetFromLocation(FName WarpTargetName, FVector TargetLocation)
	{
		AddOrUpdateWarpTargetFromTransform(WarpTargetName, FTransform(TargetLocation));
	}

	/**
	 * Create and adds or update a target associated with a specified name
	 * @param WarpTargetName Warp target identifier
	 * @param TargetLocation Location for the warp target
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	void AddOrUpdateWarpTargetFromLocationAndRotation(FName WarpTargetName, FVector TargetLocation, FRotator TargetRotation)
	{
		AddOrUpdateWarpTargetFromTransform(WarpTargetName, FTransform(TargetRotation, TargetLocation));
	}

	/** Removes the warp target associated with the specified key  */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	int32 RemoveWarpTarget(FName WarpTargetName);

protected:

	/** Character this component belongs to */
	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacter> CharacterOwner;

	/** List of root motion modifiers */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URootMotionModifier>> Modifiers;

	UPROPERTY(Transient, Replicated)
	TArray<FMotionWarpingTarget> WarpTargets;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	TOptional<FVector> OriginalRootMotionAccum;
	TOptional<FVector> WarpedRootMotionAccum;
#endif

	void Update(float DeltaSeconds);

	bool FindAndUpdateWarpTarget(const FMotionWarpingTarget& WarpTarget);

	FTransform ProcessRootMotionPreConvertToWorld(const FTransform& InRootMotion, class UCharacterMovementComponent* CharacterMovementComponent, float DeltaSeconds);
};
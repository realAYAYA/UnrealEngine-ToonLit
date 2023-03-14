// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimNodeBase.h"
#include "AlphaBlend.h"
#include "AnimNode_BlendListBase.generated.h"

class UBlendProfile;
class UCurveFloat;

UENUM()
enum class EBlendListTransitionType : uint8
{
	StandardBlend,
	Inertialization
};

// Blend list node; has many children
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_BlendListBase : public FAnimNode_Base
{
	GENERATED_BODY()

protected:	
	UPROPERTY(EditAnywhere, EditFixedSize, Category=Links)
	TArray<FPoseLink> BlendPose;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, EditFixedSize, Category=Config, meta=(PinShownByDefault, FoldProperty))
	TArray<float> BlendTime;

	UPROPERTY(EditAnywhere, Category=Config, meta=(FoldProperty))
	EBlendListTransitionType TransitionType = EBlendListTransitionType::StandardBlend;

	UPROPERTY(EditAnywhere, Category=BlendType, meta=(FoldProperty))
	EAlphaBlendOption BlendType = EAlphaBlendOption::Linear;
	
protected:
	/** This reinitializes the re-activated child if the child's weight was zero. */
	UPROPERTY(EditAnywhere, Category = Option, meta=(FoldProperty))
	bool bResetChildOnActivation = false;

private:
	UPROPERTY(EditAnywhere, Category=BlendType, meta=(FoldProperty))
	TObjectPtr<UCurveFloat> CustomBlendCurve = nullptr;

	UPROPERTY(EditAnywhere, Category=BlendType, meta=(UseAsBlendProfile=true, FoldProperty))
	TObjectPtr<UBlendProfile> BlendProfile = nullptr;
#endif // #if WITH_EDITORONLY_DATA

protected:
	// Struct for tracking blends for each pose
	struct FBlendData
	{
		FAlphaBlend Blend;
		float Weight;
		float RemainingTime;
		float StartAlpha;
	};

	TArray<FBlendData> PerBlendData;

	// Per-bone blending data, allocated when using blend profiles
	TArray<FBlendSampleData> PerBoneSampleData;

	int32 LastActiveChildIndex = 0;
	
public:	
	FAnimNode_BlendListBase() = default;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

#if WITH_EDITOR
	virtual void AddPose()
	{
		BlendTime.Add(0.1f);
		new (BlendPose) FPoseLink();
	}

	virtual void RemovePose(int32 PoseIndex)
	{
		BlendTime.RemoveAt(PoseIndex);
		BlendPose.RemoveAt(PoseIndex);
	}
#endif

public:
	// Get the array of blend times to apply to our input poses
	const TArray<float>& GetBlendTimes() const;

	// Get the type of transition that this blend list will make
	EBlendListTransitionType GetTransitionType() const;

	// Get the blend type we will use when blending
	EAlphaBlendOption GetBlendType() const;
	
	/** Get whether to reinitialize the child pose when re-activated. For example, when active child changes */
	bool GetResetChildOnActivation() const;

	// Get the custom blend curve to apply when blending, if any
	UCurveFloat* GetCustomBlendCurve() const;

	// Get the blend profile to use when blending, if any
	UBlendProfile* GetBlendProfile() const;
	
protected:
	virtual int32 GetActiveChildIndex() { return 0; }
	virtual FString GetNodeName(FNodeDebugData& DebugData) { return DebugData.GetNodeName(this); }
};

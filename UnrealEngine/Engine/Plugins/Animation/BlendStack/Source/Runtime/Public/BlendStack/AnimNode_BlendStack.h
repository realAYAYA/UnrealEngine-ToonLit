// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Mirror.h"
#include "AnimNode_BlendStack.generated.h"

USTRUCT()
struct BLENDSTACK_API FBlendStackAnimPlayer
{
	GENERATED_BODY()
	
	void Initialize(const FAnimationInitializeContext& Context, UAnimationAsset* AnimationAsset, float AccumulatedTime, bool bLoop,
		bool bMirrored, UMirrorDataTable* MirrorDataTable, float BlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption InBlendOption,
		const FVector& BlendParameters, float PlayRate, float ActivationDelay, int32 InPoseLinkIdx, FName GroupName, EAnimGroupRole::Type GroupRole, EAnimSyncMethod Method);
	
	void UpdatePlayRate(float PlayRate);
	void Evaluate_AnyThread(FPoseContext& Output);
	void Update_AnyThread(const FAnimationUpdateContext& Context);
	float GetAccumulatedTime() const;
	float GetPlayRate() const;
	
	float GetBlendInPercentage() const;
	
	int32 GetBlendInWeightsNum() const;
	void GetBlendInWeights(TArrayView<float> Weights) const;

	EAlphaBlendOption GetBlendOption() const { return BlendOption; }
	void StorePoseContext(const FPoseContext& PoseContext);
	bool HasValidPoseContext() const;
	void MovePoseContextTo(FBlendStackAnimPlayer& Other);

	float GetTotalBlendInTime() const { return TotalBlendInTime; }
	float GetCurrentBlendInTime() const { return CurrentBlendInTime; }
	float GetTimeToActivation() const { return TimeToActivation; }
	
	void UpdateWithDeltaTime(float DeltaTime, int32 PlayerDepth, float PlayerDepthBlendInTimeMultiplier);
	bool GetMirror() const { return MirrorNode.GetMirror(); }
	FVector GetBlendParameters() const;
	void SetBlendParameters(const FVector& BlendParameters);
	FString GetAnimationName() const;
	UAnimationAsset* GetAnimationAsset() const;

	FAnimNode_Mirror_Standalone& GetMirrorNode() { return MirrorNode; }
	int32 GetPoseLinkIndex() const { return PoseLinkIndex; }

	void RestorePoseContext(FPoseContext& PoseContext) const;
	void UpdateSourceLinkNode();
	bool IsLooping() const;
	bool IsActive() const;

	// Curves to add to the pose after the player evaluates
	TBaseBlendedCurve<FDefaultAllocator, UE::Anim::FCurveElement> OverrideCurve;

private:
	// Embedded standalone player to play sequence
	UPROPERTY(Transient)
	FAnimNode_SequencePlayer_Standalone SequencePlayerNode;

	// Embedded standalone player to play blend spaces
	UPROPERTY(Transient)
	FAnimNode_BlendSpacePlayer_Standalone BlendSpacePlayerNode;

	// Embedded mirror node to handle mirroring
	UPROPERTY(Transient)
	FAnimNode_Mirror_Standalone MirrorNode;

	// if SequencePlayerNode.GetSequence() and BlendSpacePlayerNode.GetBlendSpace() are nullptr, 
	// instead of using SequencePlayerNode or BlendSpacePlayerNode (wrapped in MirrorNode),
	// the output FPoseContext will be from StoredBones, StoredCurve, StoredAttributes
	// NoTe: we don't need a full FCompactHeapPose, since we use StoredBoneContainer to cache the bone container
	TArray<FTransform> StoredBones;
	FBlendedHeapCurve StoredCurve;
	UE::Anim::FHeapAttributeContainer StoredAttributes;
	// We need to store the bone container, in case we have a LOD swap during a blend that uses the stored pose.
	FBoneContainer StoredBoneContainer;

	EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear;
	int32 PoseLinkIndex = INDEX_NONE;
	TCustomBoneIndexArray<float, FSkeletonPoseBoneIndex> TotalBlendInTimePerBone;

	float TotalBlendInTime = 0.f;
	float CurrentBlendInTime = 0.f;
	float TimeToActivation = 0.f;
};

namespace UE::BlendStack
{
struct FBlendStack_SampleGraphExecutionHelper
{
	FBlendStackAnimPlayer* Player = nullptr;
	FGraphTraversalCounter CacheBoneCounter;

	void SetInputPosePlayer(FBlendStackAnimPlayer& InPlayer);
	void EvaluatePlayer(FPoseContext& Output, FBlendStackAnimPlayer& SamplePlayer, FPoseLink& SamplePoseLink);
	void ConditionalCacheBones(const FAnimationBaseContext& Output, FPoseLink& SamplePoseLink);
};
} // namespace UE::BlendStack

USTRUCT(BlueprintInternalUseOnly)
struct BLENDSTACK_API FAnimNode_BlendStack_Standalone : public FAnimNode_AssetPlayerBase
{
	GENERATED_BODY()

	TArray<UE::BlendStack::FBlendStack_SampleGraphExecutionHelper> SampleGraphExecutionHelpers;

	UPROPERTY()
	TArray<FPoseLink> PerSampleGraphPoseLinks;

	int32 CurrentSamplePoseLink = -1;

	UPROPERTY(Transient)
	TArray<FBlendStackAnimPlayer> AnimPlayers;

	// Flag that determines if any notifies from originating from an anim player samples should be filtered or not.
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bShouldFilterNotifies = false;
	
	// FAnimNode_Base interface
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	void BlendTo(const FAnimationUpdateContext& Context, UAnimationAsset* AnimationAsset, float AccumulatedTime = 0.f, bool bLoop = false, 
		bool bMirrored = false, UMirrorDataTable* MirrorDataTable = nullptr, float BlendTime = 0.2f,
		const UBlendProfile* BlendProfile = nullptr, EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear, 
		bool bUseInertialBlend = false, const FVector& BlendParameters = FVector::Zero(), float PlayRate = 1.f, float ActivationDelay = 0.f,
		FName GroupName = NAME_None, EAnimGroupRole::Type GroupRole = EAnimGroupRole::CanBeLeader, EAnimSyncMethod Method = EAnimSyncMethod::DoNotSync);
	void UpdatePlayRate(float PlayRate);
	void Reset();

	// FAnimNode_AssetPlayerBase interface
	virtual UAnimationAsset* GetAnimAsset() const;
	virtual float GetAccumulatedTime() const override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	// End of FAnimNode_AssetPlayerBase interface

	int32 GetNextPoseLinkIndex();

	int32 GetMaxActiveBlends() const { return MaxActiveBlends; }

protected:
	static void BlendWithPose(FAnimationPoseData& InOutPoseData, const FAnimationPoseData& OtherPoseData, const float InOutPoseWeight);
	static void BlendWithPosePerBone(FAnimationPoseData& InOutPoseData, const FAnimationPoseData& OtherPoseData, TConstArrayView<float> OtherPoseWeights);

	// Number of max active blending animation in the blend stack. If MaxActiveBlends is zero then blend stack is disabled
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "0"))
	int32 MaxActiveBlends = 4;

	// if the number of requested blends is higher than MaxActiveBlends, blend stack will blend and accumulate 
	// into a stored pose all the overflowing animations. if bStoreBlendedPose is false, the memory to store the pose will be saved,
	// but once reached the MaxActiveBlends, blendstack will start discarding animations, potentially resulting in animation pops
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bStoreBlendedPose = true;

    TSharedPtr<TArray<FName>> NotifiesFiredLastTick;
	TSharedPtr<TMap<FName,float>> NotifyRecencyMap;

	// Window of time after firing a notify that any instance of the same notify will be filtered out.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin = "0", PinHiddenByDefault))
	float NotifyRecencyTimeOut = 0.2f;

	// if the most relevant (recently added) animation is within MaxBlendInTimeToOverrideAnimation, the new requested blend will take its spot
	// otherwise a new blended will be added to the stack
	UPROPERTY(EditAnywhere, Category = Settings)
	float MaxBlendInTimeToOverrideAnimation = 0.f;

	// AnimPlayers blend in timer will be incremented PlayerDepthBlendInTimeMultiplier times faster on a deeper blend
	UPROPERTY(EditAnywhere, Category = Settings)
	float PlayerDepthBlendInTimeMultiplier = 1.f;

private:
	void PopLastAnimPlayer();
	void InitializeSample(const FAnimationInitializeContext& Context, FBlendStackAnimPlayer& SamplePlayer);
	void EvaluateSample(FPoseContext& Output, const int32 PlayerIndex);
	void UpdateSample(const FAnimationUpdateContext& Context, const int32 PlayerIndex);
	void CacheBonesForSample(const FAnimationCacheBonesContext& Context, const int32 PlayerIndex);
	bool IsSampleGraphAvailableForPlayer(const int32 PlayerIndex);

	friend class UAnimGraphNode_BlendStack_Base;
};


USTRUCT(BlueprintInternalUseOnly)
struct BLENDSTACK_API FAnimNode_BlendStack : public FAnimNode_BlendStack_Standalone
{
	GENERATED_BODY()

	// requested animation to play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinShownByDefault))
	TObjectPtr<UAnimationAsset> AnimationAsset;

	// requested animation time. If negative, the animation will play from the beginning uninterrupted
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	float AnimationTime = -1.f;

	// requested AnimationAsset looping
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	bool bLoop = true;

	// requested AnimationAsset mirroring
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	bool bMirrored = false;

	// requested animation play rate
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	float WantedPlayRate = 1.f;
	
	// tunable animation transition blend time 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault, ClampMin = "0"))
	float BlendTime = 0.2f;

	// if AnimationTime and MaxAnimationDeltaTime are positive and the currently playing animation total time differs more than MaxAnimationDeltaTime from AnimationTime
	// (animation desynchronized from the requested time) the blend stack will force a blend into the same animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	float MaxAnimationDeltaTime = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault, UseAsBlendProfile = true))
	TObjectPtr<UBlendProfile> BlendProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear;

	// requested blend space blend parameters (if AnimationAsset is a blend space)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	FVector BlendParameters = FVector::Zero();

	// if set and bMirrored MirrorDataTable will be used for mirroring the aniamtion
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault))
	TObjectPtr<UMirrorDataTable> MirrorDataTable;

	// tunable animation transition blend time 
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bUseInertialBlend = false;

	// Reset the blend stack if it has become relevant to the graph after not being updated on previous frames.
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bResetOnBecomingRelevant = true;

	// Update Counter for detecting being relevant
	FGraphTraversalCounter UpdateCounter;

	// FAnimNode_Base interface
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	// End of FAnimNode_Base interface

	// Force a blend on the next update, even if the anim sequence has not changed.
	void ForceBlendNextUpdate();

private:

	bool bForceBlendNextUpdate = false;

};


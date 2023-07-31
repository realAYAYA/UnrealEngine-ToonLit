// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Mirror.h"
#include "Containers/Deque.h"
#include "PoseSearch/PoseSearch.h"

#include "AnimNode_BlendStack.generated.h"

USTRUCT()
struct FPoseSearchAnimPlayer
{
	GENERATED_BODY()
	
	void Initialize(ESearchIndexAssetType InAssetType, UAnimationAsset* AnimationAsset, float AccumulatedTime, bool bLoop, bool bMirrored, UMirrorDataTable* MirrorDataTable, float BlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption InBlendOption, FVector BlendParameters);
	void Evaluate_AnyThread(FPoseContext& Output);
	void Update_AnyThread(const FAnimationUpdateContext& Context, float BlendWeight);
	float GetAccumulatedTime() const;
	
	float GetBlendInPercentage() const;
	bool GetBlendInWeights(TArray<float>& Weights) const;
	ESearchIndexAssetType GetAssetType() const { return AssetType; }
	EAlphaBlendOption GetBlendOption() const { return BlendOption; }
	void StorePoseContext(const FPoseContext& PoseContext);

	float GetTotalBlendInTime() const { return TotalBlendInTime; }
	float GetCurrentBlendInTime() const { return CurrentBlendInTime; }
	bool GetMirror() const { return MirrorNode.GetMirror(); }
	FString GetAnimName() const;
	FAnimNode_Mirror_Standalone& GetMirrorNode() { return MirrorNode; }

protected:
	void RestorePoseContext(FPoseContext& PoseContext) const;
	void UpdateSourceLinkNode();

	// Embedded sequence player node for playing animations from the motion matching database
	FAnimNode_SequencePlayer_Standalone SequencePlayerNode;

	// Embedded blendspace player node for playing blendspaces from the motion matching database
	FAnimNode_BlendSpacePlayer_Standalone BlendSpacePlayerNode;

	// Embedded mirror node to handle mirroring if the pose search results in a mirrored sequence
	FAnimNode_Mirror_Standalone MirrorNode;

	// @todo: use FCompactHeapPose
	// if AssetType == ESearchIndexAssetType::Invalid instead of using SequencePlayerNode or BlendSpacePlayerNode (wrapped in MirrorNode), the pose output will be from StoredPose
	TArray<FTransform> StoredPose;

	ESearchIndexAssetType AssetType = ESearchIndexAssetType::Sequence;
	EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear;

	TCustomBoneIndexArray<float, FSkeletonPoseBoneIndex> TotalBlendInTimePerBone;

	float TotalBlendInTime = 0.f;
	float CurrentBlendInTime = 0.f;
};

USTRUCT(BlueprintInternalUseOnly)
struct POSESEARCH_API FAnimNode_BlendStack : public FAnimNode_AssetPlayerBase
{
	GENERATED_BODY()

	TDeque<FPoseSearchAnimPlayer> AnimPlayers;
	int32 RequestedMaxActiveBlends = 0;

	// FAnimNode_Base interface
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	void BlendTo(ESearchIndexAssetType AssetType, UAnimationAsset* AnimationAsset, float AccumulatedTime = 0.f, bool bLoop = false, bool bMirrored = false, UMirrorDataTable* MirrorDataTable = nullptr, int32 MaxActiveBlends = 3, float BlendTime = 0.3f, const UBlendProfile* BlendProfile = nullptr, EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear, FVector BlendParameters = FVector::Zero());

	// FAnimNode_AssetPlayerBase interface
	virtual float GetAccumulatedTime() const override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	// End of FAnimNode_AssetPlayerBase interface
};

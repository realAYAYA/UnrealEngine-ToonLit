// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DNAReader.h"
#include "RigInstance.h"
#include "RigLogic.h"

#include "Animation/AnimNodeBase.h"
#include "Animation/SmartName.h"

#include "AnimNode_RigLogic.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRigLogicAnimNode, Log, All);

struct FSharedRigRuntimeContext;
struct FDNAIndexMapping;

USTRUCT(BlueprintInternalUseOnly)
struct RIGLOGICMODULE_API FAnimNode_RigLogic : public FAnimNode_Base
{
public:
	GENERATED_USTRUCT_BODY()

	FAnimNode_RigLogic();
	~FAnimNode_RigLogic();

	void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	void Evaluate_AnyThread(FPoseContext& Output) override;
	void GatherDebugData(FNodeDebugData& DebugData) override;
	int32 GetLODThreshold() const override { return LODThreshold; }

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FPoseLink AnimSequence;

private:
	void UpdateControlCurves(const FPoseContext& OutputContext, const FDNAIndexMapping* DNAIndexMapping);
	void CalculateRigLogic(FRigLogic* RigLogic);
	void UpdateJoints(TArrayView<const uint16> VariableJointIndices, TArrayView<const float> NeutralJointValues, TArrayView<const float> DeltaJointValues, FPoseContext& OutputContext);
	void UpdateBlendShapeCurves(const FDNAIndexMapping* DNAIndexMapping, TArrayView<const float> BlendShapeValues, FPoseContext& OutputContext);
	void UpdateAnimMapCurves(const FDNAIndexMapping* DNAIndexMapping, TArrayView<const float> AnimMapOutputs, FPoseContext& OutputContext);

private:
	struct FJointCompactPoseBoneMapping {
		uint16 JointIndex;
		FCompactPoseBoneIndex CompactPoseBoneIndex;
	};

private:
	/*
	 * Max LOD level that post-process AnimBPs are evaluated.
	 * For example if you have the threshold set to 2, it will evaluate until including LOD 2 (based on 0 index). In case the LOD level gets set to 3, it will stop evaluating the post-process AnimBP.
	 * Setting it to -1 will always evaluate it and disable LODing.
	 */
	UPROPERTY(EditAnywhere, Category = RigLogic, meta = (PinHiddenByDefault))
	int32 LODThreshold = INDEX_NONE;

private:
	TSharedPtr<FSharedRigRuntimeContext> LocalRigRuntimeContext;
	TSharedPtr<FDNAIndexMapping> LocalDNAIndexMapping;
	FRigInstance* RigInstance;
	TArray<FJointCompactPoseBoneMapping> JointsMapDNAIndicesToCompactPoseBoneIndices;
};

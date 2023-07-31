// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_RigLogic.h"

#include "DNAAsset.h"
#include "DNAIndexMapping.h"
#include "DNAReader.h"
#include "RigLogic.h"
#include "RigInstance.h"
#include "SharedRigRuntimeContext.h"
#include "Animation/AnimInstanceProxy.h"
#include "HAL/LowLevelMemTracker.h"
#include "Animation/MorphTarget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_RigLogic)

LLM_DEFINE_TAG(Animation_RigLogic);
DEFINE_LOG_CATEGORY(LogRigLogicAnimNode);

static constexpr uint16 ATTR_COUNT_PER_JOINT = 9;

FAnimNode_RigLogic::FAnimNode_RigLogic() : RigInstance(nullptr)
{
}

FAnimNode_RigLogic::~FAnimNode_RigLogic()
{
	if (RigInstance != nullptr)
	{
		delete RigInstance;
		RigInstance = nullptr;
	}
}

void FAnimNode_RigLogic::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AnimNode_RigLogic_Evaluate_AnyThread);
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	FAnimNode_Base::Initialize_AnyThread(Context);
	AnimSequence.Initialize(Context);
	USkeletalMeshComponent* SkeletalMeshComponent = Context.AnimInstanceProxy->GetSkelMeshComponent();
	USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset();

	static FRWLock GlobalCreationLock;
	FRWScopeLock ScopeLock(GlobalCreationLock, SLT_ReadOnly);
	UDNAIndexMapping* DNAIndexMappingContainer = Cast<UDNAIndexMapping>(SkeletalMesh->GetAssetUserDataOfClass(UDNAIndexMapping::StaticClass()));
	if (DNAIndexMappingContainer == nullptr)
	{
		ScopeLock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
		DNAIndexMappingContainer = Cast<UDNAIndexMapping>(SkeletalMesh->GetAssetUserDataOfClass(UDNAIndexMapping::StaticClass()));
		if (DNAIndexMappingContainer == nullptr)
		{
			DNAIndexMappingContainer = NewObject<UDNAIndexMapping>();
			SkeletalMesh->AddAssetUserData(DNAIndexMappingContainer);
		}
	}
}

void FAnimNode_RigLogic::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AnimNode_RigLogic_Evaluate_AnyThread);
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	AnimSequence.CacheBones(Context);

	// Initialize things that depend on the skeleton of Anim BP
	USkeletalMeshComponent* SkeletalMeshComponent = Context.AnimInstanceProxy->GetSkelMeshComponent();
	USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset();
	USkeleton* Skeleton = Context.AnimInstanceProxy->GetSkeleton();
	UDNAAsset* DNAAsset = Cast<UDNAAsset>(SkeletalMesh->GetAssetUserDataOfClass(UDNAAsset::StaticClass()));
	UDNAIndexMapping* DNAIndexMappingContainer = Cast<UDNAIndexMapping>(SkeletalMesh->GetAssetUserDataOfClass(UDNAIndexMapping::StaticClass()));
	if (DNAAsset == nullptr)
	{
		return;
	}

	TSharedPtr<FSharedRigRuntimeContext> SharedRigRuntimeContext = DNAAsset->GetRigRuntimeContext(UDNAAsset::EDNARetentionPolicy::Unload);
	if (!SharedRigRuntimeContext.IsValid())
	{
		return;
	}

	if (LocalRigRuntimeContext != SharedRigRuntimeContext)
	{
		LocalRigRuntimeContext = SharedRigRuntimeContext;
		if (RigInstance != nullptr)
		{
			delete RigInstance;
		}
		RigInstance = new FRigInstance(LocalRigRuntimeContext->RigLogic.Get());
	}

	LocalDNAIndexMapping = DNAIndexMappingContainer->GetCachedMapping(LocalRigRuntimeContext->BehaviorReader.Get(), Skeleton, SkeletalMesh, SkeletalMeshComponent);
	// CacheBones is called on LOD switches as well, in which case compact pose bone indices must be remapped
	const FBoneContainer& RequiredBones = Context.AnimInstanceProxy->GetRequiredBones();
	if (RequiredBones.IsValid())
	{
		RigInstance->SetLOD(Context.AnimInstanceProxy->GetLODLevel());
		const uint16 CurrentLOD = RigInstance->GetLOD();
		const TArray<uint16>& VariableJointIndices = DNAAsset->RigRuntimeContext->VariableJointIndicesPerLOD[CurrentLOD].Values;
		JointsMapDNAIndicesToCompactPoseBoneIndices.Reset(VariableJointIndices.Num());
		for (const uint16 JointIndex : VariableJointIndices)
		{
			const FMeshPoseBoneIndex MeshPoseBoneIndex = LocalDNAIndexMapping->JointsMapDNAIndicesToMeshPoseBoneIndices[JointIndex];
			JointsMapDNAIndicesToCompactPoseBoneIndices.Add(RequiredBones.MakeCompactPoseIndex(MeshPoseBoneIndex));
		}
	}
}

void FAnimNode_RigLogic::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AnimNode_RigLogic_Update_AnyThread);

	GetEvaluateGraphExposedInputs().Execute(Context);
	AnimSequence.Update(Context);
}

void FAnimNode_RigLogic::Evaluate_AnyThread(FPoseContext& OutputContext)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AnimNode_RigLogic_Evaluate_AnyThread);

	if (!LocalRigRuntimeContext.IsValid())
	{
		return;
	}
	// Initialize things that depend on the skeleton of Anim BP
	AnimSequence.Evaluate(OutputContext);
	UpdateControlCurves(OutputContext, LocalDNAIndexMapping.Get());
	CalculateRigLogic(LocalRigRuntimeContext->RigLogic.Get());
	const uint16 CurrentLOD = RigInstance->GetLOD();
	TArrayView<const uint16> VariableJointIndices = LocalRigRuntimeContext->VariableJointIndicesPerLOD[CurrentLOD].Values;
	UpdateJoints(VariableJointIndices, LocalRigRuntimeContext->RigLogic->GetRawNeutralJointValues(), RigInstance->GetRawJointOutputs(), OutputContext);
	UpdateBlendShapeCurves(LocalDNAIndexMapping.Get(), RigInstance->GetBlendShapeOutputs(), OutputContext);
	UpdateAnimMapCurves(LocalDNAIndexMapping.Get(), RigInstance->GetAnimatedMapOutputs(), OutputContext);
}

void FAnimNode_RigLogic::GatherDebugData(FNodeDebugData& DebugData)
{
	AnimSequence.GatherDebugData(DebugData);
}

void FAnimNode_RigLogic::UpdateControlCurves(const FPoseContext& InputContext, const FDNAIndexMapping* DNAIndexMapping)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AnimNode_RigLogic_UpdateControlCurves);

	const int32 RawControlCount = RigInstance->GetRawControlCount();
	for (int32 ControlIndex = 0; ControlIndex < RawControlCount; ++ControlIndex)
	{
		const int32 ControlCurveUID = DNAIndexMapping->ControlAttributesMapDNAIndicesToUEUIDs[ControlIndex];
		if (ControlCurveUID != INDEX_NONE)
		{
			const float Value = InputContext.Curve.Get(static_cast<SmartName::UID_Type>(ControlCurveUID));
			RigInstance->SetRawControl(ControlIndex, Value);
		}
	}
}

void FAnimNode_RigLogic::CalculateRigLogic(FRigLogic* RigLogic)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AnimNode_RigLogic_CalculateRigLogic);
	RigLogic->Calculate(RigInstance);
}

void FAnimNode_RigLogic::UpdateJoints(TArrayView<const uint16> VariableJointIndices, TArrayView<const float> NeutralJointValues, TArrayView<const float> DeltaJointValues, FPoseContext& OutputContext)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AnimNode_RigLogic_UpdateJoints);

	const float* N = NeutralJointValues.GetData();
	const float* D = DeltaJointValues.GetData();
	check(VariableJointIndices.Num() == JointsMapDNAIndicesToCompactPoseBoneIndices.Num());
	for (int32 MappingIndex = 0; MappingIndex < JointsMapDNAIndicesToCompactPoseBoneIndices.Num(); ++MappingIndex)
	{
		const uint16 JointIndex = VariableJointIndices[MappingIndex];
		const FCompactPoseBoneIndex CompactPoseBoneIndex = JointsMapDNAIndicesToCompactPoseBoneIndices[MappingIndex];
		if (CompactPoseBoneIndex != INDEX_NONE)
		{
			const uint16 AttrIndex = JointIndex * ATTR_COUNT_PER_JOINT;
			FTransform& CompactPose = OutputContext.Pose[CompactPoseBoneIndex];
			// Translation: X = X, Y = -Y, Z = Z
			CompactPose.SetTranslation(FVector((N[AttrIndex + 0] + D[AttrIndex + 0]), -(N[AttrIndex + 1] + D[AttrIndex + 1]), (N[AttrIndex + 2] + D[AttrIndex + 2])));
			// Rotation: X = -Y, Y = -Z, Z = X
			CompactPose.SetRotation(FQuat(FRotator(-N[AttrIndex + 4], -N[AttrIndex + 5], N[AttrIndex + 3])) * FQuat(FRotator(-D[AttrIndex + 4], -D[AttrIndex + 5], D[AttrIndex + 3])));
			// Scale: X = X, Y = Y, Z = Z
			CompactPose.SetScale3D(FVector((N[AttrIndex + 6] + D[AttrIndex + 6]), (N[AttrIndex + 7] + D[AttrIndex + 7]), (N[AttrIndex + 8] + D[AttrIndex + 8])));
		}
	}
}

void FAnimNode_RigLogic::UpdateBlendShapeCurves(const FDNAIndexMapping* DNAIndexMapping, TArrayView<const float> BlendShapeValues, FPoseContext& OutputContext)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AnimNode_RigLogic_UpdateBlendShapeCurves);

	const uint16 LOD = RigInstance->GetLOD();
	const TArray<int32>& BlendShapeIndices = DNAIndexMapping->BlendShapeIndicesPerLOD[LOD].Values;
	const TArray<int32>& MorphTargetIndices = DNAIndexMapping->MorphTargetIndicesPerLOD[LOD].Values;
	check(BlendShapeIndices.Num() == MorphTargetIndices.Num());
	for (uint32 MappingIndex = 0; MappingIndex < static_cast<uint32>(BlendShapeIndices.Num()); ++MappingIndex)
	{
		const int32 BlendShapeIndex = BlendShapeIndices[MappingIndex];
		const int32 MorphTargetCurveUID = MorphTargetIndices[MappingIndex];
		if (MorphTargetCurveUID != INDEX_NONE)
		{
			const float Value = BlendShapeValues[BlendShapeIndex];
			OutputContext.Curve.Set(static_cast<SmartName::UID_Type>(MorphTargetCurveUID), Value);
		}
	}
}

void FAnimNode_RigLogic::UpdateAnimMapCurves(const FDNAIndexMapping* DNAIndexMapping, TArrayView<const float> AnimMapOutputs, FPoseContext& OutputContext)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AnimNode_RigLogic_UpdateAnimMapCurves);

	const uint16 LOD = RigInstance->GetLOD();
	const TArray<int32>& AnimatedMapIndices = DNAIndexMapping->AnimatedMapIndicesPerLOD[LOD].Values;
	const TArray<int32>& MaskMultiplierIndices = DNAIndexMapping->MaskMultiplierIndicesPerLOD[LOD].Values;
	check(AnimatedMapIndices.Num() == MaskMultiplierIndices.Num());
	for (uint32 MappingIndex = 0; MappingIndex < static_cast<uint32>(AnimatedMapIndices.Num()); ++MappingIndex)
	{
		const int32 AnimatedMapIndex = AnimatedMapIndices[MappingIndex];
		const int32 MaskMultiplierUID = MaskMultiplierIndices[MappingIndex];
		if (MaskMultiplierUID != INDEX_NONE)
		{
			const float Value = AnimMapOutputs[AnimatedMapIndex];
			OutputContext.Curve.Set(static_cast<SmartName::UID_Type>(MaskMultiplierUID), Value);
		}
	}
}


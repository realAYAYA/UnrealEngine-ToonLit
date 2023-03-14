// Copyright Epic Games, Inc. All Rights Reserved.

#include "DNAIndexMapping.h"

#include "HAL/LowLevelMemTracker.h"
#include "DNAReader.h"
#include "Hasher.h"
#include "Animation/Skeleton.h"
#include "Animation/SmartName.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DNAIndexMapping)


static uint32 HashDNA(const IDefinitionReader* Reader)
{
	Hasher DNAHasher;
	DNAHasher.Update(Reader->GetTranslationUnit());
	DNAHasher.Update(Reader->GetRotationUnit());
	DNAHasher.Update(Reader->GetCoordinateSystem());
	DNAHasher.Update(Reader->GetLODCount());
	DNAHasher.Update(Reader->GetDBMaxLOD());
	DNAHasher.Update(Reader->GetDBComplexity());
	DNAHasher.Update(Reader->GetDBName());
	DNAHasher.Update(Reader->GetJointCount());
	DNAHasher.Update(Reader->GetBlendShapeChannelCount());
	DNAHasher.Update(Reader->GetAnimatedMapCount());
	return DNAHasher.GetHash();
}

/** Constructs curve name from nameToSplit using formatString of form x<obj>y<attr>z **/
static FString CreateCurveName(const FString& NameToSplit, const FString& FormatString)
{
	// constructs curve name from NameToSplit (always in form <obj>.<attr>)
	// using FormatString of form x<obj>y<attr>z
	// where x, y and z are arbitrary strings
	// example:
	// FormatString="mesh_<obj>_<attr>"
	// 'head.blink_L' becomes 'mesh_head_blink_L'
	FString ObjectName, AttributeName;
	if (!NameToSplit.Split(".", &ObjectName, &AttributeName))
	{
		return TEXT("");
	}
	FString CurveName = FormatString;
	CurveName = CurveName.Replace(TEXT("<obj>"), *ObjectName);
	CurveName = CurveName.Replace(TEXT("<attr>"), *AttributeName);
	return CurveName;
}

void FDNAIndexMapping::MapControlCurves(const IBehaviorReader* DNABehavior, const USkeleton* Skeleton)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	const uint32 ControlCount = DNABehavior->GetRawControlCount();
	ControlAttributesMapDNAIndicesToUEUIDs.Reset(ControlCount);
	for (uint32_t ControlIndex = 0; ControlIndex < ControlCount; ++ControlIndex)
	{
		const FString DNAControlName = DNABehavior->GetRawControlName(ControlIndex);
		const FString AnimatedControlName = CreateCurveName(DNAControlName, TEXT("<obj>_<attr>"));
		if (AnimatedControlName == TEXT(""))
		{
			return;
		}
		const FName ControlFName(*AnimatedControlName);
		FSmartName ControlCurveName;
		const bool IsControlFound = Skeleton->GetSmartNameByName(USkeleton::AnimCurveMappingName, ControlFName, ControlCurveName);
		// SmartName::UID_Type is uint32; we use int32 to allow for NONE value
		const int32 UID = (IsControlFound ? static_cast<int32>(ControlCurveName.UID) : INDEX_NONE);
		ControlAttributesMapDNAIndicesToUEUIDs.Add(UID);
	}
}

void FDNAIndexMapping::MapJoints(const IBehaviorReader* DNABehavior, const USkeletalMeshComponent* SkeletalMeshComponent)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	const uint16 JointCount = DNABehavior->GetJointCount();
	JointsMapDNAIndicesToMeshPoseBoneIndices.Reset(JointCount);
	for (uint16 JointIndex = 0; JointIndex < JointCount; ++JointIndex)
	{
		const FString JointName = DNABehavior->GetJointName(JointIndex);
		const FName BoneName = FName(*JointName);
		const int32 BoneIndex = SkeletalMeshComponent->GetBoneIndex(BoneName);
		// BoneIndex may be INDEX_NONE, but it's handled properly by the Evaluate method
		JointsMapDNAIndicesToMeshPoseBoneIndices.Add(FMeshPoseBoneIndex{BoneIndex});
	}
}

void FDNAIndexMapping::MapMorphTargets(const IBehaviorReader* DNABehavior, const USkeleton* Skeleton, const USkeletalMesh* SkeletalMesh)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	const uint16 LODCount = DNABehavior->GetLODCount();
	const TMap<FName, int32>& MorphTargetIndexMap = SkeletalMesh->GetMorphTargetIndexMap();
	const TArray<UMorphTarget*>& MorphTargets = SkeletalMesh->GetMorphTargets();

	BlendShapeIndicesPerLOD.Reset(LODCount);
	BlendShapeIndicesPerLOD.AddDefaulted(LODCount);
	MorphTargetIndicesPerLOD.Reset(LODCount);
	MorphTargetIndicesPerLOD.AddDefaulted(LODCount);

	for (uint16 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
	{
		TArrayView<const uint16> MappingIndicesForLOD = DNABehavior->GetMeshBlendShapeChannelMappingIndicesForLOD(LODIndex);
		MorphTargetIndicesPerLOD[LODIndex].Values.Reserve(MappingIndicesForLOD.Num());
		BlendShapeIndicesPerLOD[LODIndex].Values.Reserve(MappingIndicesForLOD.Num());
		for (uint16 MappingIndex : MappingIndicesForLOD)
		{
			const FMeshBlendShapeChannelMapping Mapping = DNABehavior->GetMeshBlendShapeChannelMapping(MappingIndex);
			const FString MeshName = DNABehavior->GetMeshName(Mapping.MeshIndex);
			const FString BlendShapeName = DNABehavior->GetBlendShapeChannelName(Mapping.BlendShapeChannelIndex);
			const FString MorphTargetStr = MeshName + TEXT("__") + BlendShapeName;
			const FName MorphTargetName(*MorphTargetStr);
			const int32* MorphTargetIndex = MorphTargetIndexMap.Find(MorphTargetName);
			int32 UID = INDEX_NONE;
			if ((MorphTargetIndex != nullptr) && (*MorphTargetIndex != INDEX_NONE))
			{
				const UMorphTarget* MorphTarget = MorphTargets[*MorphTargetIndex];
				UID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, MorphTarget->GetFName());
			}
			BlendShapeIndicesPerLOD[LODIndex].Values.Add(Mapping.BlendShapeChannelIndex);
			MorphTargetIndicesPerLOD[LODIndex].Values.Add(UID);
		}
	}
}

void FDNAIndexMapping::MapMaskMultipliers(const IBehaviorReader* DNABehavior, const USkeleton* Skeleton)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	const uint16 LODCount = DNABehavior->GetLODCount();

	AnimatedMapIndicesPerLOD.Reset();
	AnimatedMapIndicesPerLOD.AddDefaulted(LODCount);
	MaskMultiplierIndicesPerLOD.Reset();
	MaskMultiplierIndicesPerLOD.AddDefaulted(LODCount);

	for (uint16 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
	{
		TArrayView<const uint16> IndicesPerLOD = DNABehavior->GetAnimatedMapIndicesForLOD(LODIndex);
		AnimatedMapIndicesPerLOD[LODIndex].Values.Reserve(IndicesPerLOD.Num());
		MaskMultiplierIndicesPerLOD[LODIndex].Values.Reserve(IndicesPerLOD.Num());
		for (uint16 AnimMapIndex : IndicesPerLOD)
		{
			const FString AnimatedMapName = DNABehavior->GetAnimatedMapName(AnimMapIndex);
			const FString MaskMultiplierNameStr = CreateCurveName(AnimatedMapName, TEXT("<obj>_<attr>"));
			if (MaskMultiplierNameStr == "")
			{
				return;
			}
			const FName MaskMultiplierName(*MaskMultiplierNameStr);
			FSmartName CurveName;
			const bool IsControlFound = Skeleton->GetSmartNameByName(USkeleton::AnimCurveMappingName, MaskMultiplierName, CurveName);
			const int32 UID = (IsControlFound ? CurveName.UID : INDEX_NONE);
			AnimatedMapIndicesPerLOD[LODIndex].Values.Add(AnimMapIndex);
			MaskMultiplierIndicesPerLOD[LODIndex].Values.Add(UID);
		}
	}
}

UDNAIndexMapping::UDNAIndexMapping() : Cached{nullptr}
{
}

TSharedPtr<FDNAIndexMapping> UDNAIndexMapping::GetCachedMapping(const IBehaviorReader* DNABehavior,
																const USkeleton* Skeleton,
																const USkeletalMesh* SkeletalMesh,
																const USkeletalMeshComponent* SkeletalMeshComponent)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	const FGuid SkeletonGuid = Skeleton->GetGuid();
	const uint32 DNAHash = HashDNA(DNABehavior);
	// If GUID or DNA hash is different, all skeleton <-> DNA mappings should be recreated
	FRWScopeLock ScopeLock(Lock, SLT_ReadOnly);
	if (!Cached.IsValid() || (Cached->SkeletonGuid != SkeletonGuid) || (Cached->DNAHash != DNAHash))
	{
		ScopeLock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
		if (!Cached.IsValid() || (Cached->SkeletonGuid != SkeletonGuid) || (Cached->DNAHash != DNAHash))
		{
			TSharedPtr<FDNAIndexMapping> NewMapping = MakeShared<FDNAIndexMapping>();
			NewMapping->SkeletonGuid = SkeletonGuid;
			NewMapping->DNAHash = DNAHash;
			NewMapping->MapControlCurves(DNABehavior, Skeleton);
			NewMapping->MapJoints(DNABehavior, SkeletalMeshComponent);
			NewMapping->MapMorphTargets(DNABehavior, Skeleton, SkeletalMesh);
			NewMapping->MapMaskMultipliers(DNABehavior, Skeleton);
			Cached = NewMapping;
		}
	}
	return Cached;
}


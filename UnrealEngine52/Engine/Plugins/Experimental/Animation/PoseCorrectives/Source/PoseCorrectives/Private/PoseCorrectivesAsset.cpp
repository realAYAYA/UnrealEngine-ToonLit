// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseCorrectivesAsset.h"

#include "Animation/Skeleton.h"
#include "Animation/SmartName.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"


/////////////////////////////////////////////////////
// UPoseCorrectivesAsset
/////////////////////////////////////////////////////
UPoseCorrectivesAsset::UPoseCorrectivesAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default interpolative
	RBFParams.SolverType = ERBFSolverType::Interpolative;
}

TArray<FName> UPoseCorrectivesAsset::GetBoneNames() const
{
	TArray<FName> BoneNamesList;
	if (TargetMesh)
	{
		const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
		const TArray<FMeshBoneInfo>& BoneInfo = RefSkeleton.GetRefBoneInfo();
		for (int32 BoneIndex = 0; BoneIndex < BoneInfo.Num(); ++BoneIndex)
		{
			BoneNamesList.Add(BoneInfo[BoneIndex].Name);
		}
	}
	return BoneNamesList;
}

int32 UPoseCorrectivesAsset::GetBoneIndex(const FName& BoneName) const
{
	const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
	return RefSkeleton.FindBoneIndex(BoneName);
}

TArray<FName> UPoseCorrectivesAsset::GetCurveNames() const
{
	TArray<FName> CurveNamesList;

	if (TargetMesh)
	{
		const USkeleton* Skeleton = TargetMesh->GetSkeleton();
		const FSmartNameMapping* CurveMapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

		CurveMapping->Iterate([&CurveNamesList](const FSmartNameMappingIterator& Iterator)
			{
				FName CurveName;
				if (Iterator.GetName(CurveName))
				{
					CurveNamesList.Add(CurveName);
				}
			});
	}

	return CurveNamesList;
}

TArray<FPoseCorrective> UPoseCorrectivesAsset::GetCorrectives() const
{
	TArray<FPoseCorrective> Correctives;
	for (const auto& CorrectivePair : PoseCorrectives)
	{
		Correctives.Add(CorrectivePair.Value);
	}
	return Correctives;
}
	
#if WITH_EDITOR

void ConvertToLocalSpace(TArray<FTransform>& BoneTransform, const FReferenceSkeleton& RefSkeleton)
{
	for (int32 BoneIndex = BoneTransform.Num() - 1; BoneIndex >= 0; --BoneIndex)
	{
		const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		if (ParentIndex != INDEX_NONE)
		{
			BoneTransform[BoneIndex] = BoneTransform[BoneIndex].GetRelativeTransform(BoneTransform[ParentIndex]);
		}
	}
}

bool UPoseCorrectivesAsset::AddGroup(const FName& Name)
{
	if (GroupDefinitions.Contains(Name))
		return false;

	Modify();
	GroupDefinitions.Add(Name, FPoseGroupDefinition());
	
	return true;
}

void UPoseCorrectivesAsset::RemoveGroup(const FName& Name)
{
	if (!GroupDefinitions.Contains(Name))
		return;

	Modify();
	GroupDefinitions.Remove(Name);		
}

void UPoseCorrectivesAsset::AddCorrective(USkeletalMeshComponent* SourceMeshComponent, USkeletalMeshComponent* TargetMeshComponent, const FName& CorrectiveName)
{
	Modify();

	FPoseCorrective PoseCorrective;

	if (const FPoseCorrective* Corrective = PoseCorrectives.Find(CorrectiveName))
	{
		PoseCorrective = *Corrective;
	}

	PoseCorrective.PoseLocal = SourceMeshComponent->GetBoneSpaceTransforms();
	PoseCorrective.CorrectivePoseLocal =  TargetMeshComponent->GetBoneSpaceTransforms();

	const FBlendedHeapCurve& SourceMeshCurves = SourceMeshComponent->GetAnimationCurves();
	const FBlendedHeapCurve& TargetMeshCurves = TargetMeshComponent->GetAnimationCurves();
	const FSmartNameMapping* Mapping = TargetMesh->GetSkeleton()->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

	TArray<FName> CurveNames = GetCurveNames();
	PoseCorrective.CurveData.Reset();
	PoseCorrective.CurveData.AddZeroed(CurveNames.Num());
	PoseCorrective.CorrectiveCurvesDelta.Reset();	
	PoseCorrective.CorrectiveCurvesDelta.AddZeroed(CurveNames.Num());

	for (int32 CurveIndex = 0; CurveIndex < CurveNames.Num(); ++CurveIndex)
	{
		const SmartName::UID_Type CurveUID = Mapping->FindUID(CurveNames[CurveIndex]);
		if (CurveUID != SmartName::MaxUID)
		{
			const float SourceCurveValue = SourceMeshCurves.Get(CurveUID);
			const float TargetCurveValue = TargetMeshCurves.Get(CurveUID);
			PoseCorrective.CurveData[CurveIndex] = SourceCurveValue;
			PoseCorrective.CorrectiveCurvesDelta[CurveIndex] = TargetCurveValue - SourceCurveValue;
		}
	}

	PoseCorrectives.Add(CorrectiveName, PoseCorrective);

	OnCorrectivesListChanged.Broadcast();
}

bool UPoseCorrectivesAsset::DeleteCorrective(const FName& CorrectiveName)
{
	if (PoseCorrectives.Contains(CorrectiveName))
	{
		Modify();
		PoseCorrectives.Remove(CorrectiveName);
		OnCorrectivesListChanged.Broadcast();
		return true;
	}
	
	return false;
}

bool UPoseCorrectivesAsset::RenameCorrective(const FName& CurrentName, const FName& NewName)
{
	if (PoseCorrectives.Contains(CurrentName))
	{
		Modify();
		
		FPoseCorrective corrective;
		PoseCorrectives.RemoveAndCopyValue(CurrentName, corrective);
		PoseCorrectives.Add(NewName, corrective);	
		return true;
	}
	
	return false;
}

void UPoseCorrectivesAsset::UpdateGroupForCorrective(const FName& CorrectiveName, const FName& GroupName)
{
	FPoseCorrective* PoseCorrective = FindCorrective(CorrectiveName);
	const FPoseGroupDefinition* GroupDefinition = FindGroupDefinition(GroupName);

	PoseCorrective->GroupName = GroupName;
	UpdateGroupForCorrective(PoseCorrective, GroupDefinition);
}

void UPoseCorrectivesAsset::UpdateGroupForCorrective(FPoseCorrective* PoseCorrective, const FPoseGroupDefinition* GroupDefinition)
{
	if (PoseCorrective && GroupDefinition)
	{
		Modify();

		PoseCorrective->DriverBoneIndices.Reset();
		PoseCorrective->CorrectiveBoneIndices.Reset();
		PoseCorrective->DriverCurveIndices.Reset();
		PoseCorrective->CorrectiveCurveIndices.Reset();

		const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
		const TArray<FMeshBoneInfo>& BoneInfo = RefSkeleton.GetRefBoneInfo();
	
		for (int32 BoneIndex = 0; BoneIndex < BoneInfo.Num(); ++BoneIndex)
		{
			const FName& BoneName = BoneInfo[BoneIndex].Name;
			if (GroupDefinition->DriverBones.Contains(BoneName))
			{
				PoseCorrective->DriverBoneIndices.Add(BoneIndex);
			}
		
			if (GroupDefinition->CorrectiveBones.Contains(BoneName))
			{
				PoseCorrective->CorrectiveBoneIndices.Add(BoneIndex);
			}
		}

		TArray<FName> CurveNames = GetCurveNames();
		for (int32 CurveIndex = 0; CurveIndex < CurveNames.Num(); ++CurveIndex)
		{
			const FName& CurveName = CurveNames[CurveIndex];
			if (GroupDefinition->DriverCurves.Contains(CurveName))
			{
				PoseCorrective->DriverCurveIndices.Add(CurveIndex);
			}

			if (GroupDefinition->CorrectiveCurves.Contains(CurveName))
			{
				PoseCorrective->CorrectiveCurveIndices.Add(CurveIndex);
			}
		}
	}
}

void UPoseCorrectivesAsset::UpdateCorrectivesByGroup(const FName& GroupName)
{
	for (const auto& Corrective : PoseCorrectives)
	{
		if (Corrective.Value.GroupName == GroupName)
			UpdateGroupForCorrective(Corrective.Key, GroupName);
	}
}

FPoseCorrective* UPoseCorrectivesAsset::FindCorrective(const FName& CorrectiveName)
{
	return PoseCorrectives.Find(CorrectiveName);
}

FPoseGroupDefinition* UPoseCorrectivesAsset::FindGroupDefinition(const FName& GroupName)
{
	return GroupDefinitions.Find(GroupName);
}

TArray<FName> UPoseCorrectivesAsset::GetCorrectiveNames() const
{
	TArray<FName> CorrectiveNames;
	for (const auto& CorrectivePair : PoseCorrectives)
	{
		CorrectiveNames.Add(CorrectivePair.Key);
	}
	return CorrectiveNames;
}

TArray<FName> UPoseCorrectivesAsset::GetGroupNames() const
{
	TArray<FName> GroupNames;
	for (const auto& GroupPair : GroupDefinitions)
	{
		GroupNames.Add(GroupPair.Key);
	}
	return GroupNames;
}

#endif


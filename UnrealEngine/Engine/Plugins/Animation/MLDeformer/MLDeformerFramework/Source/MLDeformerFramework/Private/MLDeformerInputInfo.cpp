// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerInputInfo.h"
#include "MLDeformerAsset.h"
#include "MLDeformerModel.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimInstance.h"
#include "ReferenceSkeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"


void UMLDeformerInputInfo::Reset()
{
	BoneNameStrings.Empty();
	BoneNames.Empty();
	CurveNameStrings.Empty();
	CurveNames.Empty();
	NumBaseMeshVertices = 0;
	NumTargetMeshVertices = 0;
}

void UMLDeformerInputInfo::UpdateFNames()
{
	// Update the bone names.
	const int32 NumBones = BoneNameStrings.Num();
	BoneNames.Reset(NumBones);
	BoneNames.Reserve(NumBones);
	for (const FString& NameString : BoneNameStrings)
	{
		BoneNames.Add(FName(NameString));
	}

	// Update the curve names.
	const int32 NumCurves = CurveNameStrings.Num();
	CurveNames.Reset(NumCurves);
	CurveNames.Reserve(NumCurves);
	for (const FString& NameString : CurveNameStrings)
	{
		CurveNames.Add(FName(NameString));
	}
}

void UMLDeformerInputInfo::OnPostLoad()
{ 
	UpdateFNames();
}

bool UMLDeformerInputInfo::IsEmpty() const
{ 
	return (BoneNameStrings.IsEmpty() && CurveNameStrings.IsEmpty());
}

int32 UMLDeformerInputInfo::GetNumBones() const
{ 
	return BoneNames.Num();
}

const FString& UMLDeformerInputInfo::GetBoneNameString(int32 Index) const
{ 
	return BoneNameStrings[Index];
}

const FName UMLDeformerInputInfo::GetBoneName(int32 Index) const
{ 
	return BoneNames[Index];
}

int32 UMLDeformerInputInfo::GetNumCurves() const
{ 
	return CurveNames.Num();
}

TArray<FString>& UMLDeformerInputInfo::GetBoneNameStrings()
{ 
	return BoneNameStrings;
}

TArray<FString>& UMLDeformerInputInfo::GetCurveNameStrings()
{ 
	return CurveNameStrings;
}

TArray<FName>& UMLDeformerInputInfo::GetBoneNames()
{ 
	return BoneNames;
}

TArray<FName>& UMLDeformerInputInfo::GetCurveNames()
{ 
	return CurveNames;
}

const FString& UMLDeformerInputInfo::GetCurveNameString(int32 Index) const
{ 
	return CurveNameStrings[Index];
}

const FName UMLDeformerInputInfo::GetCurveName(int32 Index) const
{ 
	return CurveNames[Index];
}

int32 UMLDeformerInputInfo::GetNumBaseMeshVertices() const
{ 
	return NumBaseMeshVertices;
}

void UMLDeformerInputInfo::SetNumBaseVertices(int32 NumVerts)
{ 
	NumBaseMeshVertices = NumVerts;
}

int32 UMLDeformerInputInfo::GetNumTargetMeshVertices() const
{
	return NumTargetMeshVertices;
}

void UMLDeformerInputInfo::SetNumTargetVertices(int32 NumVerts)
{
	NumTargetMeshVertices = NumVerts;
}

bool UMLDeformerInputInfo::IsCompatible(USkeletalMesh* SkeletalMesh) const
{
	if (SkeletalMesh == nullptr)
	{
		return false;
	}

	// Verify that all required bones are there.
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	for (const FName BoneName : BoneNames)
	{
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE) // We're missing the required bone. The network needs to input the transform for this bone.
		{
			return false;
		}
	}

	// Verify that all required curves are there.
	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	const FSmartNameMapping* SmartNameMapping = Skeleton ? Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName) : nullptr;
	if (SmartNameMapping)
	{
		for (const FName CurveName : CurveNames)
		{
			if (!SmartNameMapping->Exists(CurveName))
			{
				return false;
			}
		}
	}

	return true;
}

FString UMLDeformerInputInfo::GenerateCompatibilityErrorString(USkeletalMesh* SkeletalMesh) const
{
	if (SkeletalMesh == nullptr)
	{
		return FString();
	}

	// Verify that all required bones are there.
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	FString ErrorString;
	for (const FName BoneName : BoneNames)
	{
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE) // We're missing the required bone. The network needs to input the transform for this bone.
		{
			ErrorString += FString::Format(TEXT("Required bone '{0}' is missing.\n"), {*BoneName.ToString()});
		}
	}

	// Verify that all required curves are there.
	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	const FSmartNameMapping* SmartNameMapping = Skeleton ? Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName) : nullptr;
	if (SmartNameMapping)
	{
		for (const FName CurveName : CurveNames)
		{
			if (!SmartNameMapping->Exists(CurveName))
			{
				ErrorString += FString::Format(TEXT("Required curve '{0}' is missing.\n"), {*CurveName.ToString()});
			}
		}
	}

	// Check vertex count.
#if WITH_EDITORONLY_DATA
	if ((NumBaseMeshVertices > 0 && NumTargetMeshVertices > 0) &&
		NumBaseMeshVertices != SkeletalMesh->GetNumImportedVertices())
	{
		ErrorString += FString::Format(TEXT("The number of vertices that the network was trained on ({0} verts) doesn't match the skeletal mesh '{1}' ({2} verts)..\n"), 
			{
				NumBaseMeshVertices, 
				SkeletalMesh->GetName(),
				SkeletalMesh->GetNumImportedVertices(),
			} );
	}
#endif

	return ErrorString;
}

void UMLDeformerInputInfo::ExtractCurveValues(USkeletalMeshComponent* SkelMeshComponent, TArray<float>& OutValues) const
{
	check(CurveNames.Num() == CurveNameStrings.Num());

	UAnimInstance* AnimInstance = SkelMeshComponent->GetAnimInstance();
	const int32 NumCurves = CurveNames.Num();
	OutValues.Reset(NumCurves);
	OutValues.AddUninitialized(NumCurves);
	for (int32 Index = 0; Index < NumCurves; ++Index)
	{
		const FName CurveName = CurveNames[Index];
		OutValues[Index] = AnimInstance->GetCurveValue(CurveName);
	}
}

void UMLDeformerInputInfo::ExtractBoneRotations(USkeletalMeshComponent* SkelMeshComponent, TArray<float>& OutRotations) const
{
	const TArray<FTransform>& BoneTransforms = SkelMeshComponent->GetBoneSpaceTransforms();
	const int32 NumBones = GetNumBones();
	const int32 NumFloats = NumBones * 6; // 2 Columns of the rotation matrix.
	OutRotations.Reset(NumFloats);
	OutRotations.AddUninitialized(NumFloats);
	int32 Offset = 0;
	for (int32 Index = 0; Index < NumBones; ++Index)
	{
		const FName BoneName = GetBoneName(Index);
		const int32 SkelMeshBoneIndex = SkelMeshComponent->GetBoneIndex(BoneName);
		const FMatrix RotationMatrix = (SkelMeshBoneIndex != INDEX_NONE) ? BoneTransforms[SkelMeshBoneIndex].GetRotation().ToMatrix() : FMatrix::Identity;
		const FVector X = RotationMatrix.GetColumn(0);
		const FVector Y = RotationMatrix.GetColumn(1);	
		OutRotations[Offset++] = X.X;
		OutRotations[Offset++] = X.Y;
		OutRotations[Offset++] = X.Z;
		OutRotations[Offset++] = Y.X;
		OutRotations[Offset++] = Y.Y;
		OutRotations[Offset++] = Y.Z;
	}
}

int32 UMLDeformerInputInfo::CalcNumNeuralNetInputs() const
{
	return 
		BoneNameStrings.Num() * 6 +	// Six floats per bone (2 FVector3 columns of the rotation matrix).
		CurveNameStrings.Num();		// One float per curve.
}

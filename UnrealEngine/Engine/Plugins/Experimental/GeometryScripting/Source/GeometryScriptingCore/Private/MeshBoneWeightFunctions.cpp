// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/MeshBoneWeightFunctions.h"

#include "Animation/Skeleton.h"
#include "BoneWeights.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "DynamicMesh/DynamicBoneAttribute.h"
#include "SkinningOps/SkinBindingOp.h"
#include "UDynamicMesh.h"
#include "Operations/TransferBoneWeights.h"
#include "MathUtil.h"
#include "Containers/Queue.h"
#include "DynamicMesh/MeshBones.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshBoneWeightFunctions)

using namespace UE::Geometry;
using namespace UE::AnimationCore;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_MeshBoneWeightFunctions"


template<typename ReturnType> 
ReturnType SimpleMeshBoneWeightQuery(
	UDynamicMesh* Mesh, FGeometryScriptBoneWeightProfile Profile, 
	bool& bIsValidBoneWeights, ReturnType DefaultValue, 
	TFunctionRef<ReturnType(const FDynamicMesh3& Mesh, const FDynamicMeshVertexSkinWeightsAttribute& SkinWeights)> QueryFunc)
{
	bIsValidBoneWeights = false;
	ReturnType RetVal = DefaultValue;
	if (Mesh)
	{
		Mesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			if (ReadMesh.HasAttributes())
			{
				if ( const FDynamicMeshVertexSkinWeightsAttribute* BoneWeights = ReadMesh.Attributes()->GetSkinWeightsAttribute(Profile.GetProfileName()) )
				{
					bIsValidBoneWeights = true;
					RetVal = QueryFunc(ReadMesh, *BoneWeights);
				}
			}
		});
	}
	return RetVal;
}


template<typename ReturnType> 
ReturnType SimpleMeshBoneWeightEdit(
	UDynamicMesh* Mesh, FGeometryScriptBoneWeightProfile Profile, 
	bool& bIsValidBoneWeights, ReturnType DefaultValue, 
	TFunctionRef<ReturnType(FDynamicMesh3& Mesh, FDynamicMeshVertexSkinWeightsAttribute& SkinWeights)> EditFunc)
{
	bIsValidBoneWeights = false;
	ReturnType RetVal = DefaultValue;
	if (Mesh)
	{
		Mesh->EditMesh([&](FDynamicMesh3& EditMesh)
		{
			if (EditMesh.HasAttributes())
			{
				if ( FDynamicMeshVertexSkinWeightsAttribute* BoneWeights = EditMesh.Attributes()->GetSkinWeightsAttribute(Profile.GetProfileName()) )
				{
					bIsValidBoneWeights = true;
					RetVal = EditFunc(EditMesh, *BoneWeights);
				}
			}
		}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
	}
	return RetVal;
}




UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::MeshHasBoneWeights(
	UDynamicMesh* TargetMesh,
	bool& bHasBoneWeights,
	FGeometryScriptBoneWeightProfile Profile)
{
	bool bHasBoneWeightProfile = false;
	bool bOK = SimpleMeshBoneWeightQuery<bool>(TargetMesh, Profile, bHasBoneWeightProfile, false,
		[&](const FDynamicMesh3& Mesh, const FDynamicMeshVertexSkinWeightsAttribute& SkinWeights) { return true; });
	bHasBoneWeights = bHasBoneWeightProfile;
	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::MeshCreateBoneWeights(
	UDynamicMesh* TargetMesh,
	bool& bProfileExisted,
	bool bReplaceExistingProfile,
	FGeometryScriptBoneWeightProfile Profile)
{
	bProfileExisted = false;
	if (TargetMesh)
	{
		TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
		{
			if (EditMesh.HasAttributes() == false)
			{
				EditMesh.EnableAttributes();
			}

			FDynamicMeshVertexSkinWeightsAttribute *Attribute = EditMesh.Attributes()->GetSkinWeightsAttribute(Profile.GetProfileName());
			bProfileExisted = (Attribute != nullptr);
			if ( Attribute == nullptr || bReplaceExistingProfile)
			{
				if ( bReplaceExistingProfile && bProfileExisted )
				{
					EditMesh.Attributes()->RemoveSkinWeightsAttribute(Profile.GetProfileName());
				}

				Attribute = new FDynamicMeshVertexSkinWeightsAttribute(&EditMesh);
				EditMesh.Attributes()->AttachSkinWeightsAttribute(Profile.GetProfileName(), Attribute);
			}			
		}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
	}
	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::GetMaxBoneWeightIndex(
	UDynamicMesh* TargetMesh,
	bool& bHasBoneWeights,
	int& MaxBoneIndex,
	FGeometryScriptBoneWeightProfile Profile)
{
	MaxBoneIndex = -1;
	bool bHasBoneWeightProfile = false;
	bool bOK = SimpleMeshBoneWeightQuery<bool>(TargetMesh, Profile, bHasBoneWeightProfile, false,
		[&](const FDynamicMesh3& Mesh, const FDynamicMeshVertexSkinWeightsAttribute& SkinWeights) 
		{ 
			for (int32 VertexID : Mesh.VertexIndicesItr())
			{
				FBoneWeights BoneWeights;
				SkinWeights.GetValue(VertexID, BoneWeights);
				int32 Num = BoneWeights.Num();
				for (int32 k = 0; k < Num; ++k)
				{
					MaxBoneIndex = FMathd::Max(MaxBoneIndex, BoneWeights[k].GetBoneIndex());
				}
			}
			return true;
		});
	bHasBoneWeights = bHasBoneWeightProfile;
	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::GetVertexBoneWeights(
	UDynamicMesh* TargetMesh,
	int VertexID,
	TArray<FGeometryScriptBoneWeight>& BoneWeightsOut,
	bool& bHasValidBoneWeights,
	FGeometryScriptBoneWeightProfile Profile)
{
	bool bHasBoneWeightProfile = false;
	bHasValidBoneWeights = SimpleMeshBoneWeightQuery<bool>(TargetMesh, Profile, bHasBoneWeightProfile, false,
		[&](const FDynamicMesh3& Mesh, const FDynamicMeshVertexSkinWeightsAttribute& SkinWeights)
	{
		if (Mesh.IsVertex(VertexID))
		{
			FBoneWeights BoneWeights;
			SkinWeights.GetValue(VertexID, BoneWeights);
			int32 Num = BoneWeights.Num();
			BoneWeightsOut.SetNum(Num);
			for (int32 k = 0; k < Num; ++k)
			{
				FGeometryScriptBoneWeight NewBoneWeight;
				NewBoneWeight.BoneIndex = BoneWeights[k].GetBoneIndex();
				NewBoneWeight.Weight = BoneWeights[k].GetWeight();
				BoneWeightsOut.Add(NewBoneWeight);
			}
		}
		return BoneWeightsOut.Num() > 0;
	});

	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::GetLargestVertexBoneWeight(
	UDynamicMesh* TargetMesh,
	int VertexID,
	FGeometryScriptBoneWeight& BoneWeight,
	bool& bHasValidBoneWeights,
	FGeometryScriptBoneWeightProfile Profile)
{
	bHasValidBoneWeights = false;
	bool bHasBoneWeightProfile = false;
	FBoneWeight FoundMax = SimpleMeshBoneWeightQuery<FBoneWeight>(TargetMesh, Profile, bHasBoneWeightProfile, FBoneWeight(),
	[&](const FDynamicMesh3& Mesh, const FDynamicMeshVertexSkinWeightsAttribute& SkinWeights) 
	{ 
		FBoneWeight MaxBoneWeight = FBoneWeight();
		if (Mesh.IsVertex(VertexID))
		{
			bHasValidBoneWeights = true;
			float MaxWeight = 0;
			FBoneWeights BoneWeights;
			SkinWeights.GetValue(VertexID, BoneWeights);
			int32 Num = BoneWeights.Num();
			for (int32 k = 0; k < Num; ++k)
			{
				const FBoneWeight& BoneWeight = BoneWeights[k];
				if (BoneWeight.GetWeight() > MaxWeight)
				{
					MaxWeight = BoneWeight.GetWeight();
					MaxBoneWeight = BoneWeight;
				}
			}
		}
		else
		{
			UE_LOG(LogGeometry, Warning, TEXT("GetLargestMeshBoneWeight: VertexID %d does not exist"), VertexID);
		}
		return MaxBoneWeight;
	});
	
	if (bHasValidBoneWeights)
	{
		BoneWeight.BoneIndex = FoundMax.GetBoneIndex();
		BoneWeight.Weight = FoundMax.GetWeight();
	}

	return TargetMesh;
}





UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::SetVertexBoneWeights(
	UDynamicMesh* TargetMesh,
	int VertexID,
	const TArray<FGeometryScriptBoneWeight>& BoneWeights,
	bool& bHasValidBoneWeights,
	FGeometryScriptBoneWeightProfile Profile)
{
	bool bHasBoneWeightProfile = false;
	bHasValidBoneWeights = SimpleMeshBoneWeightEdit<bool>(TargetMesh, Profile, bHasBoneWeightProfile, false,
		[&](FDynamicMesh3& Mesh, FDynamicMeshVertexSkinWeightsAttribute& SkinWeights)
	{
		if (Mesh.IsVertex(VertexID))
		{
			int32 Num = BoneWeights.Num();
			TArray<FBoneWeight> NewWeightsList;
			for (int32 k = 0; k < Num; ++k)
			{
				FBoneWeight NewWeight;
				NewWeight.SetBoneIndex(BoneWeights[k].BoneIndex);
				NewWeight.SetWeight(BoneWeights[k].Weight);
				NewWeightsList.Add(NewWeight);
			}
			FBoneWeights NewBoneWeights = FBoneWeights::Create(NewWeightsList);
			SkinWeights.SetValue(VertexID, NewBoneWeights);
			return true;
		}
		return false;
	});

	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::SetAllVertexBoneWeights(
	UDynamicMesh* TargetMesh,
	const TArray<FGeometryScriptBoneWeight>& BoneWeights, 
	FGeometryScriptBoneWeightProfile Profile
	)
{
	bool bHasBoneWeightProfile = false;
	const int32 Num = BoneWeights.Num();
	TArray<FBoneWeight> NewWeightsList;
	for (int32 k = 0; k < Num; ++k)
	{
		FBoneWeight NewWeight;
		NewWeight.SetBoneIndex(BoneWeights[k].BoneIndex);
		NewWeight.SetWeight(BoneWeights[k].Weight);
		NewWeightsList.Add(NewWeight);
	}
	const FBoneWeights NewBoneWeights = FBoneWeights::Create(NewWeightsList);
	
	SimpleMeshBoneWeightEdit<bool>(TargetMesh, Profile, bHasBoneWeightProfile, false,
		[&](const FDynamicMesh3& Mesh, FDynamicMeshVertexSkinWeightsAttribute& SkinWeights)
	{
		for (const int32 VertexID : Mesh.VertexIndicesItr())
		{
			SkinWeights.SetValue(VertexID, NewBoneWeights);
		}
		return true;
	});

	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::ComputeSmoothBoneWeights(
	UDynamicMesh* TargetMesh,
	USkeleton* Skeleton, 
	FGeometryScriptSmoothBoneWeightsOptions Options, 
	FGeometryScriptBoneWeightProfile Profile,
	UGeometryScriptDebug* Debug
	)
{
	if (TargetMesh == nullptr)
	{
		AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ComputeSmoothBoneWeights_InvalidInput", "ComputeSmoothBoneWeights: TargetMesh is Null"));
		return TargetMesh;
	}
	if (Skeleton == nullptr)
	{
		AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ComputeSmoothBoneWeights_InvalidSkeleton", "ComputeSmoothBoneWeights: Skeleton is Null"));
		return TargetMesh;
	}

	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
	{
		FSkinBindingOp SkinBindingOp;
		SkinBindingOp.OriginalMesh = MakeShared<FDynamicMesh3>(MoveTemp(EditMesh));
		SkinBindingOp.SetTransformHierarchyFromReferenceSkeleton(Skeleton->GetReferenceSkeleton());
		SkinBindingOp.ProfileName = Profile.ProfileName;
		switch(Options.DistanceWeighingType)
		{
		case EGeometryScriptSmoothBoneWeightsType::DirectDistance:
			SkinBindingOp.BindType = ESkinBindingType::DirectDistance;
			break;
		case EGeometryScriptSmoothBoneWeightsType::GeodesicVoxel:
			SkinBindingOp.BindType = ESkinBindingType::GeodesicVoxel;
			break;
		}
		SkinBindingOp.Stiffness = Options.Stiffness;
		SkinBindingOp.MaxInfluences = Options.MaxInfluences;
		SkinBindingOp.VoxelResolution = Options.VoxelResolution;

		SkinBindingOp.CalculateResult(nullptr);

		EditMesh = MoveTemp(*SkinBindingOp.ExtractResult().Release());
	}, EDynamicMeshChangeType::AttributeEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::TransferBoneWeightsFromMesh(
	UDynamicMesh* SourceMesh,
	UDynamicMesh* TargetMesh,
	FGeometryScriptTransferBoneWeightsOptions Options,
	UGeometryScriptDebug* Debug)
{
	using namespace UE::Geometry;

	if (SourceMesh == nullptr)
	{
		AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("TransferBoneWeightsFromMesh_InvalidSourceMesh", "TransferBoneWeightsFromMesh: Source Mesh is Null"));
		return TargetMesh;
	}
	if (TargetMesh == nullptr)
	{
		AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("TransferBoneWeightsFromMesh_InvalidTargetMesh", "TransferBoneWeightsFromMesh: Target Mesh is Null"));
		return TargetMesh;
	}

	SourceMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		if (!ReadMesh.HasAttributes() || !ReadMesh.Attributes()->HasBones())
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("TransferBoneWeightsFromMesh_NoBones", "Source Mesh has no bone attribute"));
			return;
		}
		if (ReadMesh.Attributes()->GetNumBones() == 0)
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("TransferBoneWeightsFromMesh_EmptyBones", "Source Mesh has an empty bone attribute"));
			return;
		}

		FTransferBoneWeights TransferBoneWeights(&ReadMesh, Options.SourceProfile.GetProfileName());
		TransferBoneWeights.TransferMethod = static_cast<FTransferBoneWeights::ETransferBoneWeightsMethod>(Options.TransferMethod);
		TransferBoneWeights.bUseParallel = true;


		TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
		{
			if (!EditMesh.HasAttributes())
			{
				EditMesh.EnableAttributes();
			}
			
			if (EditMesh.Attributes()->HasBones())
			{
				// If the TargetMesh has bone attributes, but we want to use the SourceMesh bone attributes, then we copy.
				// Otherwise, nothing to do, and we use the target mesh bone attributes.
				if (Options.OutputTargetMeshBones == EOutputTargetMeshBones::SourceBones)
				{
					EditMesh.Attributes()->CopyBoneAttributes(*ReadMesh.Attributes());
				}
			}
			else
			{
				// If the TargetMesh has no bone attributes, then we must use the SourceMesh bone attributes. Otherwise, throw an error.
				if (Options.OutputTargetMeshBones == EOutputTargetMeshBones::SourceBones)
				{
					EditMesh.Attributes()->CopyBoneAttributes(*ReadMesh.Attributes());
				}
				else 
				{
					UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("TransferBoneWeightsFromMesh_NoTargetMeshBones", "TransferBoneWeightsFromMesh: TargetMesh has no bone attributes but the OutputTargetMeshBones option is set to TargetBones"));
				}
			}
			
			if (Options.TransferMethod == ETransferBoneWeightsMethod::InpaintWeights)
			{
				TransferBoneWeights.NormalThreshold = FMathd::DegToRad * Options.NormalThreshold;
				TransferBoneWeights.SearchRadius = Options.RadiusPercentage * EditMesh.GetBounds().DiagonalLength();
				TransferBoneWeights.NumSmoothingIterations = Options.NumSmoothingIterations;
				TransferBoneWeights.SmoothingStrength = Options.SmoothingStrength;
				TransferBoneWeights.LayeredMeshSupport = Options.LayeredMeshSupport;
				TransferBoneWeights.ForceInpaintWeightMapName = Options.InpaintMask;
			}

			if (TransferBoneWeights.Validate() != EOperationValidationResult::Ok)
			{
				UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::OperationFailed, LOCTEXT("TransferBoneWeightsFromMesh_ValidationFailed", "TransferBoneWeightsFromMesh: Invalid parameters were set for the transfer weight operator"));
				return;
			}
			if (!TransferBoneWeights.TransferWeightsToMesh(EditMesh, Options.TargetProfile.GetProfileName()))
			{
				UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::OperationFailed, LOCTEXT("TransferBoneWeightsFromMesh_TransferFailed", "TransferBoneWeightsFromMesh: Failed to transfer the weights"));
			}
			
		}, EDynamicMeshChangeType::AttributeEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
	});

	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::CopyBonesFromMesh(
	UDynamicMesh* SourceMesh, 
	UDynamicMesh* TargetMesh, 
	UGeometryScriptDebug* Debug)
{
	if (SourceMesh == nullptr)
	{
		AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyBonesFromMesh_InvalidSourceMesh", "CopyBonesFromMesh: SourceMesh is Null"));
		return TargetMesh;
	}

	if (TargetMesh == nullptr)
	{
		AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyBonesFromMesh_InvalidTargetMesh", "CopyBonesFromMesh: TargetMesh is Null"));
		return TargetMesh;
	}

	SourceMesh->EditMesh([&](const FDynamicMesh3& ReadMesh)
	{
		TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
		{
			if (!ReadMesh.HasAttributes() || !ReadMesh.Attributes()->HasBones())
			{
				AppendWarning(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyBonesFromMesh_SourceMeshHasNoBones", "SourceMesh has no bone attributes"));
				return;
			}

			if (!ReadMesh.Attributes()->CheckBoneValidity(EValidityCheckFailMode::ReturnOnly))
			{
				AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyBonesFromMesh_InvalidSourceMeshBones", "SourceMesh has invalid bone attributes"));
				return;
			}

			if (!EditMesh.HasAttributes())
			{
				EditMesh.EnableAttributes();
			}
				
			EditMesh.Attributes()->CopyBoneAttributes(*ReadMesh.Attributes());
		}, EDynamicMeshChangeType::AttributeEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
	});

	return TargetMesh;
}

UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::DiscardBonesFromMesh(
	UDynamicMesh* TargetMesh, 
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("DiscardBonesFromMesh_InvalidTargetMesh", "DiscardBonesFromMesh: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
	{
		if (EditMesh.HasAttributes())
		{
			EditMesh.Attributes()->DisableBones();
		}
		
	}, EDynamicMeshChangeType::AttributeEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}

UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::GetBoneIndex(
	UDynamicMesh* TargetMesh,
	FName BoneName,
	bool& bIsValidBoneName,
	int& BoneIndex,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetBoneIndex_InvalidTargetMesh", "GetBoneIndex: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->ProcessMesh([&](const FDynamicMesh3& EditMesh)
	{	
		if (!EditMesh.HasAttributes() || !EditMesh.Attributes()->HasBones())
		{
			AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetBoneIndex_TargetMeshHasNoBones", "GetBoneIndex: TargetMesh has no bone attributes"));
			return;
		}

		// INDEX_NONE if BoneName doesn't exist in the bone names attribute
		BoneIndex = EditMesh.Attributes()->GetBoneNames()->GetAttribValues().Find(BoneName);
		
		bIsValidBoneName = BoneIndex != INDEX_NONE;

	});

	return TargetMesh;
}

UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::GetRootBoneName(
	UDynamicMesh* TargetMesh,
	FName& BoneName,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetRootBoneName_InvalidTargetMesh", "GetRootBoneName: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->ProcessMesh([&](const FDynamicMesh3& EditMesh)
	{	
		if (!EditMesh.HasAttributes() || !EditMesh.Attributes()->HasBones())
		{
			AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetRootBoneName_TargetMeshHasNoBones", "GetRootBoneName: TargetMesh has no bone attributes"));
			return;
		}

		if (EditMesh.Attributes()->GetBoneNames()->GetAttribValues().IsEmpty())
		{
			AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetRootBoneName_TargetMeshHasEmptySkeleton", "GetRootBoneName: TargetMesh has bone attributes set, but they are empty (doesn't contain a single bone)"));
			return;
		}

		// Root bone's parent will be INDEX_NONE
		const int32 BoneIdx = EditMesh.Attributes()->GetBoneParentIndices()->GetAttribValues().Find(INDEX_NONE);

		if (BoneIdx == INDEX_NONE)
		{
			AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetRootBoneName_TargetMeshHasNoRootBone", "GetRootBoneName: TargetMesh has no root bone"));
			return;
		}

		BoneName = EditMesh.Attributes()->GetBoneNames()->GetAttribValues()[BoneIdx];
	});

	return TargetMesh;
}

UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::GetBoneChildren(
	UDynamicMesh* TargetMesh,
	FName BoneName,
	bool bRecursive,
	bool& bIsValidBoneName,
	TArray<FGeometryScriptBoneInfo>& ChildrenInfo,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetBoneChildren_InvalidTargetMesh", "GetBoneChildren: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->ProcessMesh([&](const FDynamicMesh3& EditMesh)
	{	
		if (!EditMesh.HasAttributes() || !EditMesh.Attributes()->HasBones())
		{
			AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetBoneChildren_TargetMeshHasNoBones", "GetBoneChildren: TargetMesh has no bone attributes"));
			return;
		}
		
		const int32 NumBones = EditMesh.Attributes()->GetNumBones();
		const TArray<FName>& NamesAttrib = EditMesh.Attributes()->GetBoneNames()->GetAttribValues();
		const TArray<int32>& ParentsAttrib = EditMesh.Attributes()->GetBoneParentIndices()->GetAttribValues();
		const TArray<FTransform>& TransformsAttrib = EditMesh.Attributes()->GetBonePoses()->GetAttribValues();
		const TArray<FVector4f>& ColorsAttrib = EditMesh.Attributes()->GetBoneColors()->GetAttribValues();

		// INDEX_NONE if BoneName doesn't exist in the bone names attribute
		const int32 BoneIndex = NamesAttrib.Find(BoneName);
		
		bIsValidBoneName = BoneIndex != INDEX_NONE;

		if (bIsValidBoneName)
		{	
			ChildrenInfo.Reset();

			TArray<int32> ChildrenIndices;

			FMeshBones::GetBoneChildren(EditMesh, BoneIndex, ChildrenIndices, bRecursive);
			
			// Get all information about the children
			ChildrenInfo.SetNum(ChildrenIndices.Num());
			for (int32 Idx = 0; Idx < ChildrenIndices.Num(); ++Idx)
			{
				const int32 ChildIdx = ChildrenIndices[Idx];
				FGeometryScriptBoneInfo& Entry = ChildrenInfo[Idx];
				Entry.Index = ChildIdx;
				Entry.Name = NamesAttrib[ChildIdx];
				Entry.ParentIndex = ParentsAttrib[ChildIdx];
				Entry.LocalTransform = TransformsAttrib[ChildIdx];
				Entry.WorldTransform = Entry.LocalTransform;
				Entry.Color = ColorsAttrib[ChildIdx];

				int32 CurParentIndex = ParentsAttrib[ChildIdx];
				while (CurParentIndex != INDEX_NONE) // until we didn't reach the root bone
				{
					Entry.WorldTransform = Entry.WorldTransform * TransformsAttrib[CurParentIndex];
					CurParentIndex = ParentsAttrib[CurParentIndex];
				}
			}
		}
	});

	return TargetMesh;
}

UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::GetBoneInfo(
		UDynamicMesh* TargetMesh,
		FName BoneName,
		bool& bIsValidBoneName,
		FGeometryScriptBoneInfo& BoneInfo,
		UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetBoneInfo_InvalidTargetMesh", "GetBoneInfo: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->ProcessMesh([&](const FDynamicMesh3& EditMesh)
	{	
		if (!EditMesh.HasAttributes() || !EditMesh.Attributes()->HasBones())
		{
			AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetBoneInfo_TargetMeshHasNoBones", "GetBoneInfo: TargetMesh has no bone attributes"));
			return;
		}
		
		
		// INDEX_NONE if BoneName doesn't exist in the bone names attribute
		int32 BoneIndex = EditMesh.Attributes()->GetBoneNames()->GetAttribValues().Find(BoneName);

		bIsValidBoneName = BoneIndex != INDEX_NONE;

		if (bIsValidBoneName)
		{	
			const TArray<FName>& NamesAttrib = EditMesh.Attributes()->GetBoneNames()->GetAttribValues();
			const TArray<int32>& ParentsAttrib = EditMesh.Attributes()->GetBoneParentIndices()->GetAttribValues();
			const TArray<FTransform>& TransformsAttrib = EditMesh.Attributes()->GetBonePoses()->GetAttribValues();
			const TArray<FVector4f>& ColorsAttrib = EditMesh.Attributes()->GetBoneColors()->GetAttribValues();

			BoneInfo.Index = BoneIndex;
			BoneInfo.Name = NamesAttrib[BoneIndex];
			BoneInfo.ParentIndex = ParentsAttrib[BoneIndex];
			BoneInfo.LocalTransform = TransformsAttrib[BoneIndex];
			BoneInfo.WorldTransform = BoneInfo.LocalTransform;
			BoneInfo.Color = ColorsAttrib[BoneIndex];

			int32 CurParentIndex = ParentsAttrib[BoneIndex];
			while (CurParentIndex != INDEX_NONE) // until we didn't reach the root bone
			{
				BoneInfo.WorldTransform = BoneInfo.WorldTransform * TransformsAttrib[CurParentIndex];
				CurParentIndex = ParentsAttrib[CurParentIndex];
			}
		}
	});

	return TargetMesh;
}

UDynamicMesh* UGeometryScriptLibrary_MeshBoneWeightFunctions::GetAllBonesInfo(
		UDynamicMesh* TargetMesh,
		TArray<FGeometryScriptBoneInfo>& BonesInfo,
		UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetAllBonesInfo_InvalidTargetMesh", "GetAllBonesInfo: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->ProcessMesh([&](const FDynamicMesh3& EditMesh)
	{	
		if (!EditMesh.HasAttributes() || !EditMesh.Attributes()->HasBones())
		{
			AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetAllBonesInfo_TargetMeshHasNoBones", "GetAllBonesInfo: TargetMesh has no bone attributes"));
			return;
		}
		
		const int32 NumBones = EditMesh.Attributes()->GetNumBones();

		BonesInfo.SetNum(NumBones);

		const TArray<FName>& NamesAttrib = EditMesh.Attributes()->GetBoneNames()->GetAttribValues();
		const TArray<int32>& ParentsAttrib = EditMesh.Attributes()->GetBoneParentIndices()->GetAttribValues();
		const TArray<FTransform>& TransformsAttrib = EditMesh.Attributes()->GetBonePoses()->GetAttribValues();
		const TArray<FVector4f>& ColorsAttrib = EditMesh.Attributes()->GetBoneColors()->GetAttribValues();

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			FGeometryScriptBoneInfo& Entry = BonesInfo[BoneIndex];
			
			Entry.Index = BoneIndex;
			Entry.Name = NamesAttrib[BoneIndex];
			Entry.ParentIndex = ParentsAttrib[BoneIndex];
			Entry.LocalTransform = TransformsAttrib[BoneIndex];
			Entry.WorldTransform = Entry.LocalTransform;
			Entry.Color = ColorsAttrib[BoneIndex];

			int32 CurParentIndex = ParentsAttrib[BoneIndex];
			while (CurParentIndex != INDEX_NONE) // until we didn't reach the root bone
			{
				Entry.WorldTransform = Entry.WorldTransform * TransformsAttrib[CurParentIndex];
				CurParentIndex = ParentsAttrib[CurParentIndex];
			}
		}
	});

	return TargetMesh;
}

#undef LOCTEXT_NAMESPACE

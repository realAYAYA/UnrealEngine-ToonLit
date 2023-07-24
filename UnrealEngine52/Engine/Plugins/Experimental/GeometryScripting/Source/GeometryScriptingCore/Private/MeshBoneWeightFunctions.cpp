// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/MeshBoneWeightFunctions.h"

#include "Animation/Skeleton.h"
#include "BoneWeights.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "SkinningOps/SkinBindingOp.h"
#include "UDynamicMesh.h"

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


#undef LOCTEXT_NAMESPACE

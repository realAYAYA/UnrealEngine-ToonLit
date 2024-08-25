// Copyright Epic Games, Inc. All Rights Reserved.

#include "StaticToSkeletalMeshConverter.h"

#if WITH_EDITOR

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/StaticMesh.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "LODUtilities.h"
#include "MeshDescription.h"
#include "MeshUtilities.h"
#include "Misc/Guid.h"
#include "Modules/ModuleManager.h"
#include "ReferenceSkeleton.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshAttributes.h"


DEFINE_LOG_CATEGORY_STATIC(LogStaticToSkeletalMeshConverter, Log, All);

static const FName RootBoneName("Root");
static const TCHAR* JointBaseName(TEXT("Joint"));


bool FStaticToSkeletalMeshConverter::InitializeSkeletonFromStaticMesh(
	USkeleton* InSkeleton,
	const UStaticMesh* InStaticMesh,
	const FVector& InRelativeRootPosition
	)
{
	if (!ensure(InSkeleton))
	{
		return false;
	}
	
	if (InSkeleton->GetReferenceSkeleton().GetNum() != 0)
	{
		UE_LOG(LogStaticToSkeletalMeshConverter, Error, TEXT("Skeleton '%s' is not empty"), *InSkeleton->GetPathName());
		return false;
	}
	
	if (!ensure(InStaticMesh))
	{
		return false;
	}

	const FBox Bounds = InStaticMesh->GetBoundingBox();
	const FVector RootPosition = Bounds.Min + (Bounds.Max - Bounds.Min) * InRelativeRootPosition;
	FTransform RootTransform(FTransform::Identity);
	RootTransform.SetTranslation(RootPosition);

	FReferenceSkeletonModifier Modifier(InSkeleton);
	Modifier.Add(FMeshBoneInfo(RootBoneName, RootBoneName.ToString(), INDEX_NONE), RootTransform);

	return true;
}


bool FStaticToSkeletalMeshConverter::InitializeSkeletonFromStaticMesh(
	USkeleton* InSkeleton,
	const UStaticMesh* InStaticMesh,
	const FVector& InRelativeRootPosition,
	const FVector& InRelativeEndEffectorPosition,
	const int32 InIntermediaryJointCount
	)
{
	if (!ensure(InSkeleton))
	{
		return false;
	}
	
	if (InSkeleton->GetReferenceSkeleton().GetNum() != 0)
	{
		UE_LOG(LogStaticToSkeletalMeshConverter, Error, TEXT("Skeleton '%s' is not empty"), *InSkeleton->GetPathName());
		return false;
	}

	if (!ensure(InStaticMesh))
	{
		return false;
	}
	
	if (FMath::IsNearlyZero(FVector::DistSquared(InRelativeEndEffectorPosition, InRelativeRootPosition)))
	{
		return InitializeSkeletonFromStaticMesh(InSkeleton, InStaticMesh, InRelativeRootPosition);
	}

	const FBox Bounds = InStaticMesh->GetBoundingBox();
	const FVector RootPosition = Bounds.Min + (Bounds.Max - Bounds.Min) * InRelativeRootPosition;
	const FVector EndEffectorPosition = Bounds.Min + (Bounds.Max - Bounds.Min) * InRelativeEndEffectorPosition;

	// Find a rough rotation we can use
	const FQuat Rotation = FQuat::FindBetweenVectors(FVector::ZAxisVector, EndEffectorPosition - RootPosition).GetNormalized(); 
	
	FTransform ParentTransform(FTransform::Identity);
	ParentTransform.SetTranslation(RootPosition);
	ParentTransform.SetRotation(Rotation);

	FReferenceSkeletonModifier Modifier(InSkeleton);
	Modifier.Add(FMeshBoneInfo(RootBoneName, RootBoneName.ToString(), INDEX_NONE), ParentTransform);

	for (int32 JointIndex = 0; JointIndex <= InIntermediaryJointCount; JointIndex++)
	{
		const double T = (JointIndex + 1.0) / (InIntermediaryJointCount + 2.0);
		FTransform PointTransform(ParentTransform);
		PointTransform.SetTranslation(RootPosition + (EndEffectorPosition - RootPosition) * T);

		FString JointName = FString::Printf(TEXT("%s_%d"), JointBaseName, JointIndex + 1);
		Modifier.Add(FMeshBoneInfo(FName(JointName), JointName, JointIndex), PointTransform * ParentTransform.Inverse());
		ParentTransform = PointTransform;
	}

	return true;
}

static void CopyBuildSettings(
	const FMeshBuildSettings& InStaticMeshBuildSettings,
	FSkeletalMeshBuildSettings& OutSkeletalMeshBuildSettings
	)
{
	OutSkeletalMeshBuildSettings.bRecomputeNormals = InStaticMeshBuildSettings.bRecomputeNormals;
	OutSkeletalMeshBuildSettings.bRecomputeTangents = InStaticMeshBuildSettings.bRecomputeTangents;
	OutSkeletalMeshBuildSettings.bUseMikkTSpace = InStaticMeshBuildSettings.bUseMikkTSpace;
	OutSkeletalMeshBuildSettings.bComputeWeightedNormals = InStaticMeshBuildSettings.bComputeWeightedNormals;
	OutSkeletalMeshBuildSettings.bRemoveDegenerates = InStaticMeshBuildSettings.bRemoveDegenerates;
	OutSkeletalMeshBuildSettings.bUseHighPrecisionTangentBasis = InStaticMeshBuildSettings.bUseHighPrecisionTangentBasis;
	OutSkeletalMeshBuildSettings.bUseFullPrecisionUVs = InStaticMeshBuildSettings.bUseFullPrecisionUVs;
	OutSkeletalMeshBuildSettings.bUseBackwardsCompatibleF16TruncUVs = InStaticMeshBuildSettings.bUseBackwardsCompatibleF16TruncUVs;
	// The rest we leave at defaults.
}

static SkeletalMeshOptimizationImportance ConvertOptimizationImportance(
	 EMeshFeatureImportance::Type InStaticMeshImportance)
{
	switch(InStaticMeshImportance)
	{
	default:
	case EMeshFeatureImportance::Off:		return SMOI_Highest;
	case EMeshFeatureImportance::Lowest:	return SMOI_Lowest;
	case EMeshFeatureImportance::Low:		return SMOI_Low;
	case EMeshFeatureImportance::Normal:	return SMOI_Normal;
	case EMeshFeatureImportance::High:		return SMOI_High;
	case EMeshFeatureImportance::Highest:	return SMOI_Highest;
	}
}

static void CopyReductionSettings(
	const FMeshReductionSettings& InStaticMeshReductionSettings,
	FSkeletalMeshOptimizationSettings& OutSkeletalMeshReductionSettings
	)
{
	// Copy the reduction settings as closely as we can. 
	OutSkeletalMeshReductionSettings.NumOfTrianglesPercentage = InStaticMeshReductionSettings.PercentTriangles;
	OutSkeletalMeshReductionSettings.NumOfVertPercentage = InStaticMeshReductionSettings.PercentVertices;
	
	OutSkeletalMeshReductionSettings.WeldingThreshold = InStaticMeshReductionSettings.WeldingThreshold;
	OutSkeletalMeshReductionSettings.NormalsThreshold = InStaticMeshReductionSettings.HardAngleThreshold;
	OutSkeletalMeshReductionSettings.bRecalcNormals = InStaticMeshReductionSettings.bRecalculateNormals;
	
	OutSkeletalMeshReductionSettings.BaseLOD = InStaticMeshReductionSettings.BaseLODModel;
	
	OutSkeletalMeshReductionSettings.SilhouetteImportance = ConvertOptimizationImportance(InStaticMeshReductionSettings.SilhouetteImportance);
	OutSkeletalMeshReductionSettings.TextureImportance = ConvertOptimizationImportance(InStaticMeshReductionSettings.TextureImportance);
	OutSkeletalMeshReductionSettings.ShadingImportance = ConvertOptimizationImportance(InStaticMeshReductionSettings.ShadingImportance);
	
	switch(InStaticMeshReductionSettings.TerminationCriterion)
	{
	case EStaticMeshReductionTerimationCriterion::Triangles:
		OutSkeletalMeshReductionSettings.TerminationCriterion = SMTC_NumOfTriangles;
		break;
	case EStaticMeshReductionTerimationCriterion::Vertices:
		OutSkeletalMeshReductionSettings.TerminationCriterion = SMTC_NumOfVerts;
		break;
	case EStaticMeshReductionTerimationCriterion::Any:
		OutSkeletalMeshReductionSettings.TerminationCriterion = SMTC_TriangleOrVert;
		break;
	}
}



static bool AddLODFromMeshDescription(
	FMeshDescription&& InMeshDescription,
	USkeletalMesh* InSkeletalMesh,
	IMeshUtilities& InMeshUtilities
	)
{
	FSkeletalMeshModel* ImportedModels = InSkeletalMesh->GetImportedModel();
	const int32 LODIndex = ImportedModels->LODModels.Num(); 
	ImportedModels->LODModels.Add(new FSkeletalMeshLODModel);
	if (!ensure(ImportedModels->LODModels.Num() == InSkeletalMesh->GetLODNum()))
	{
		return false;
	}

	FSkeletalMeshImportData SkeletalMeshImportGeometry = FSkeletalMeshImportData::CreateFromMeshDescription(InMeshDescription);

	InSkeletalMesh->CreateMeshDescription(LODIndex, MoveTemp(InMeshDescription));
	InSkeletalMesh->CommitMeshDescription(LODIndex);
	
	FSkeletalMeshLODModel& SkeletalMeshModel = ImportedModels->LODModels.Last();

	// We need at least one set of texture coordinates. Always.
	SkeletalMeshModel.NumTexCoords = FMath::Max<uint32>(1, SkeletalMeshImportGeometry.NumTexCoords);
	
	// Data needed by BuildSkeletalMesh
	TArray<FVector3f> LODPoints;
	TArray<SkeletalMeshImportData::FMeshWedge> LODWedges;
	TArray<SkeletalMeshImportData::FMeshFace> LODFaces;
	TArray<SkeletalMeshImportData::FVertInfluence> LODInfluences;
	TArray<int32> LODPointToRawMap;
	SkeletalMeshImportGeometry.CopyLODImportData( LODPoints, LODWedges, LODFaces, LODInfluences, LODPointToRawMap );

	IMeshUtilities::MeshBuildOptions BuildOptions;
	BuildOptions.TargetPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	BuildOptions.FillOptions(InSkeletalMesh->GetLODInfoArray().Last().BuildSettings);

	TArray<FText> WarningMessages;
	if (!InMeshUtilities.BuildSkeletalMesh(SkeletalMeshModel, InSkeletalMesh->GetPathName(), InSkeletalMesh->GetRefSkeleton(), LODInfluences, LODWedges, LODFaces, LODPoints, LODPointToRawMap, BuildOptions, &WarningMessages, nullptr))
	{
		for(const FText& Message: WarningMessages)
		{
			UE_LOG(LogStaticToSkeletalMeshConverter, Warning, TEXT("%s"), *Message.ToString());
		}
		return false;
	}
	
	return true;
}


static bool AddLODFromStaticMeshSourceModel(
	const FStaticMeshSourceModel& InStaticMeshSourceModel,
	USkeletalMesh* InSkeletalMesh,
	const FBoneIndexType InBoneIndex,
	IMeshUtilities& InMeshUtilities
	)
{
	// Always copy the build and reduction settings. 
	FSkeletalMeshLODInfo& SkeletalLODInfo = InSkeletalMesh->AddLODInfo();

	SkeletalLODInfo.ScreenSize = InStaticMeshSourceModel.ScreenSize;
	CopyBuildSettings(InStaticMeshSourceModel.BuildSettings, SkeletalLODInfo.BuildSettings);
	CopyReductionSettings(InStaticMeshSourceModel.ReductionSettings, SkeletalLODInfo.ReductionSettings);

	FSkeletalMeshModel* ImportedModels = InSkeletalMesh->GetImportedModel();
	const int32 LODIndex = ImportedModels->LODModels.Num(); 

	if (InStaticMeshSourceModel.IsMeshDescriptionValid())
	{
		FMeshDescription SkeletalMeshGeometry;
		if (!InStaticMeshSourceModel.CloneMeshDescription(SkeletalMeshGeometry))
		{
			return false;
		}
		
		FSkeletalMeshAttributes SkeletalMeshAttributes(SkeletalMeshGeometry);
		SkeletalMeshAttributes.Register();
		
		// Full binding to the root bone.
		FSkinWeightsVertexAttributesRef SkinWeights = SkeletalMeshAttributes.GetVertexSkinWeights();
		UE::AnimationCore::FBoneWeight RootInfluence(InBoneIndex, 1.0f);
		UE::AnimationCore::FBoneWeights RootBinding = UE::AnimationCore::FBoneWeights::Create({RootInfluence});
		
		for (const FVertexID VertexID: SkeletalMeshGeometry.Vertices().GetElementIDs())
		{
			SkinWeights.Set(VertexID, RootBinding);
		}

		if (!AddLODFromMeshDescription(MoveTemp(SkeletalMeshGeometry), InSkeletalMesh, InMeshUtilities))
		{
			return false;
		}
	}
	else
	{
		ImportedModels->LODModels.Add(new FSkeletalMeshLODModel);
		
		FSkeletalMeshUpdateContext UpdateContext;
		UpdateContext.SkeletalMesh = InSkeletalMesh;
		
		FLODUtilities::SimplifySkeletalMeshLOD(UpdateContext, LODIndex, GetTargetPlatformManagerRef().GetRunningTargetPlatform());
	}
	
	return true;
}


static bool HasVertexColors(
	const USkeletalMesh* InSkeletalMesh
	)
{
	for (const FSkeletalMeshLODModel& LODModel: InSkeletalMesh->GetImportedModel()->LODModels)
	{
		for (const FSkelMeshSection& Section: LODModel.Sections)
		{
			for (const FSoftSkinVertex& Vertex: Section.SoftVertices)
			{
				if (Vertex.Color != FColor::White)
				{
					return true;
				}
			}
		}
	}
	return false;
}


bool FStaticToSkeletalMeshConverter::InitializeSkeletalMeshFromStaticMesh(
	USkeletalMesh* InSkeletalMesh,
	const UStaticMesh* InStaticMesh,
	const FReferenceSkeleton& InReferenceSkeleton,
	const FName InBindBone 
)
{
	if (!ensure(InSkeletalMesh))
	{
		return false;
	}

	if (!InSkeletalMesh->GetImportedModel()->LODModels.IsEmpty())
	{
		UE_LOG(LogStaticToSkeletalMeshConverter, Error, TEXT("Skeletal mesh '%s' is not empty"), *InSkeletalMesh->GetPathName());
		return false;
	}

	if (!ensure(InStaticMesh))
	{
		return false;
	}

	int32 BoneIndex = 0;
	if (!InBindBone.IsNone())
	{
		BoneIndex = InReferenceSkeleton.FindRawBoneIndex(InBindBone);
		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogStaticToSkeletalMeshConverter, Error, TEXT("Bone '%s' not found in skeleton."), *InBindBone.ToString());
			return false;
		}
	}

	// This ensures that the render data gets built before we return, by calling PostEditChange when we fall out of scope.
	FScopedSkeletalMeshPostEditChange ScopedPostEditChange( InSkeletalMesh );
	InSkeletalMesh->PreEditChange( nullptr );
	InSkeletalMesh->SetRefSkeleton(InReferenceSkeleton);
	
	// Calculate the initial pose from the reference skeleton.
	InSkeletalMesh->CalculateInvRefMatrices();

	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>( "MeshUtilities" );
	
	// Copy the LODs and LOD settings over (as close as we can).
	bool bFirstSourceModel = true;
	for (const FStaticMeshSourceModel& StaticMeshSourceModel: InStaticMesh->GetSourceModels())
	{
		if (!AddLODFromStaticMeshSourceModel(
			StaticMeshSourceModel, InSkeletalMesh, static_cast<FBoneIndexType>(BoneIndex), MeshUtilities))
		{
			// If we didn't get a model for LOD index 0, we don't have a mesh. Bail out.
			if (bFirstSourceModel)
			{
				return false;
			}

			// Otherwise, we have a model, so let's continue with what we have.
			break;
		}
		bFirstSourceModel = false;
	}
	
	// Convert the materials over.
	TArray<FSkeletalMaterial> Materials;
	for (const FStaticMaterial& StaticMaterial: InStaticMesh->GetStaticMaterials())
	{
		FSkeletalMaterial Material(
			StaticMaterial.MaterialInterface,
			StaticMaterial.MaterialSlotName);
		
		Materials.Add(Material);
	}

	InSkeletalMesh->SetMaterials(Materials);

	if (HasVertexColors(InSkeletalMesh))
	{
		InSkeletalMesh->SetHasVertexColors(true);	
		InSkeletalMesh->SetVertexColorGuid(FGuid::NewGuid());
	}
	
	// Set the bounds from the static mesh, including the extensions, otherwise it won't render properly (among other things).
	InSkeletalMesh->SetImportedBounds( InStaticMesh->GetBounds() );
	InSkeletalMesh->SetPositiveBoundsExtension(InStaticMesh->GetPositiveBoundsExtension());
	InSkeletalMesh->SetNegativeBoundsExtension(InStaticMesh->GetNegativeBoundsExtension());

	return true;
}


static bool ValidateSkinWeightAttribute(
	const FMeshDescription& InMeshDescription,
	const FReferenceSkeleton& InReferenceSkeleton
	)
{
	using namespace UE::AnimationCore;
	FSkeletalMeshConstAttributes MeshAttributes{InMeshDescription};

	TArray<FName> Profiles = MeshAttributes.GetSkinWeightProfileNames(); 
	if (Profiles.IsEmpty())
	{
		UE_LOG(LogStaticToSkeletalMeshConverter, Error, TEXT("Mesh description doesn't have a skin weight attribute."));
		return false;
	}

	FBoneIndexType BoneIndexMax = static_cast<FBoneIndexType>(InReferenceSkeleton.GetRawBoneNum());
		
	// We use the first profile. Usually that's the default profile, unless we have nothing but alternate profiles.
	FSkinWeightsVertexAttributesConstRef VertexSkinWeights = MeshAttributes.GetVertexSkinWeights(Profiles[0]);
	for (const FVertexID VertexID: InMeshDescription.Vertices().GetElementIDs())
	{
		for (FBoneWeight BoneWeight: VertexSkinWeights.Get(VertexID))
		{
			if (BoneWeight.GetBoneIndex() >= BoneIndexMax)
			{
				UE_LOG(LogStaticToSkeletalMeshConverter, Error, TEXT("Mesh description's skin weight refers to a non-existent bone (%d of %d)."), BoneWeight.GetBoneIndex(), BoneIndexMax);
				return false;
			}
		}
	}
	return true;
}


bool FStaticToSkeletalMeshConverter::InitializeSkeletalMeshFromMeshDescriptions(
	USkeletalMesh* InSkeletalMesh,
	TArrayView<const FMeshDescription*> InMeshDescriptions,
	TConstArrayView<FSkeletalMaterial> InMaterials,
	const FReferenceSkeleton& InReferenceSkeleton,
	const bool bInRecomputeNormals,
	const bool bInRecomputeTangents
)	
{
	if (!ensure(InSkeletalMesh))
	{
		return false;
	}

	if (!InSkeletalMesh->GetImportedModel()->LODModels.IsEmpty())
	{
		UE_LOG(LogStaticToSkeletalMeshConverter, Error, TEXT("Skeletal mesh '%s' is not empty"), *InSkeletalMesh->GetPathName());
		return false;
	}

	if (InMeshDescriptions.IsEmpty())
	{
		UE_LOG(LogStaticToSkeletalMeshConverter, Error, TEXT("No mesh descriptions given"));
		return false;
	}

	// Ensure all mesh descriptions have a skin weight attribute.
	for (const FMeshDescription* MeshDescription: InMeshDescriptions)
	{
		if (!ValidateSkinWeightAttribute(*MeshDescription, InReferenceSkeleton))
		{
			return false;
		}
	}

	// Set the materials before we start converting. We'll add dummy materials afterward if there are more sections
	// than materials in any of the LODs. Not the best system, but the best we have for now.
	InSkeletalMesh->SetMaterials(TArray<FSkeletalMaterial>{InMaterials});

	// This ensures that the render data gets built before we return, by calling PostEditChange when we fall out of scope.
	{
		FScopedSkeletalMeshPostEditChange ScopedPostEditChange( InSkeletalMesh );
		InSkeletalMesh->PreEditChange( nullptr );
		InSkeletalMesh->SetRefSkeleton(InReferenceSkeleton);
		
		// Calculate the initial pose from the reference skeleton.
		InSkeletalMesh->CalculateInvRefMatrices();

		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>( "MeshUtilities" );
		bool bFirstSourceModel = true;
		
		for (const FMeshDescription* MeshDescription: InMeshDescriptions)
		{
			// Add default LOD build settings.
			FSkeletalMeshLODInfo& SkeletalLODInfo = InSkeletalMesh->AddLODInfo();
			SkeletalLODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
			SkeletalLODInfo.ReductionSettings.NumOfVertPercentage = 1.0f;
			SkeletalLODInfo.ReductionSettings.MaxDeviationPercentage = 0.0f;
			SkeletalLODInfo.LODHysteresis = 0.02f;

			SkeletalLODInfo.BuildSettings.bRecomputeNormals = bInRecomputeNormals;
			SkeletalLODInfo.BuildSettings.bRecomputeTangents = bInRecomputeTangents;

			FMeshDescription ClonedDescription(*MeshDescription);
			if (!AddLODFromMeshDescription(MoveTemp(ClonedDescription), InSkeletalMesh, MeshUtilities))
			{
				// If we didn't get a model for LOD index 0, we don't have a mesh. Bail out.
				if (bFirstSourceModel)
				{
					return false;
				}

				// Otherwise, we have a model, so let's continue with what we have.
				break;
			}
			bFirstSourceModel = false;
		}
	}

	// Compute the bbox, now that we have the model mesh generated. 
	FBox3f BoundingBox{ForceInit};
	int32 MaxSectionCount = 0;
	for (const FSkeletalMeshLODModel& MeshModel: InSkeletalMesh->GetImportedModel()->LODModels)
	{
		MaxSectionCount = FMath::Max(MaxSectionCount, MeshModel.Sections.Num());

		// Compute the overall bbox.
		for (const FSkelMeshSection& Section: MeshModel.Sections)
		{
			for (const FSoftSkinVertex& Vertex: Section.SoftVertices)
			{
				BoundingBox += Vertex.Position;
			}
		}
	}

	// If we're short on materials, compared to sections, add dummy materials to fill in the gap. Not ideal, but
	// best we can do for now.
	const TArray<FSkeletalMaterial>& ExistingMaterials = InSkeletalMesh->GetMaterials();
	if (MaxSectionCount > ExistingMaterials.Num())
	{
		TArray<FSkeletalMaterial> NewMaterials{ExistingMaterials};
		for (int32 Index = ExistingMaterials.Num(); Index < MaxSectionCount; Index++)
		{
			NewMaterials.Add(FSkeletalMaterial{});
		}
		InSkeletalMesh->SetMaterials(NewMaterials);
	}

	if (HasVertexColors(InSkeletalMesh))
	{
		InSkeletalMesh->SetHasVertexColors(true);	
		InSkeletalMesh->SetVertexColorGuid(FGuid::NewGuid());
	}
	
	InSkeletalMesh->SetImportedBounds( FBox3d{BoundingBox} );

	return true;
}

#endif

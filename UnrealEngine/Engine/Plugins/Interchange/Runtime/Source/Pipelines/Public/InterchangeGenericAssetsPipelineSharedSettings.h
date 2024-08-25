// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangePipelineBase.h"
#include "Math/Color.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"

#include "InterchangeGenericAssetsPipelineSharedSettings.generated.h"

class UInterchangeSkeletonFactoryNode;
class USkeleton;

/** Enumerates the options for importing all meshes as one type. */
UENUM(BlueprintType)
enum class EInterchangeForceMeshType : uint8
{
	/** Import both static meshes and skeletal meshes from the source without converting them. */
	IFMT_None UMETA(DisplayName = "None"),
	/** Import all meshes from the source as static meshes. */
	IFMT_StaticMesh UMETA(DisplayName = "Static Mesh"),
	/** Import all meshes from the source as skeletal meshes. */
	IFMT_SkeletalMesh UMETA(DisplayName = "Skeletal Mesh"),

	IFMT_MAX
};

UENUM(BlueprintType)
enum class EInterchangeVertexColorImportOption : uint8
{
	/** Import the mesh using the vertex colors from the translated source. */
	IVCIO_Replace UMETA(DisplayName = "Replace"),
	/** Ignore vertex colors from the translated source. In case of a reimport, keep the existing mesh vertex colors. */
	IVCIO_Ignore UMETA(DisplayName = "Ignore"),
	/** Override all vertex colors with the specified color. */
	IVCIO_Override UMETA(DisplayName = "Override"),

	IVCIO_MAX
};


UCLASS(BlueprintType, hidedropdown)
class INTERCHANGEPIPELINES_API UInterchangeGenericCommonMeshesProperties : public UInterchangePipelineBase
{
	GENERATED_BODY()
public:
	//////	COMMON_MESHES_CATEGORY Properties //////

	/** If set, imports all meshes in the source as either static meshes or skeletal meshes. */
 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes")
 	EInterchangeForceMeshType ForceAllMeshAsType = EInterchangeForceMeshType::IFMT_None;

	/**
	 * If enabled, and some static mesh transforms are animated, the pipeline will convert the static mesh into a rigid skeletal mesh.
	 * This setting is only used if the Force All Meshes As Type setting is set to "None".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes")
	bool bAutoDetectMeshType = true;

	/** If enabled, any existing LODs for meshes are imported. This setting is only used if the Bake Meshes setting is also enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes")
	bool bImportLods = true;

	/** If enabled, meshes are baked with the scene instance hierarchy transform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes")
	bool bBakeMeshes = true;
	
	/** If checked, sections with matching materials are kept separate and will not get combined. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes")
	bool bKeepSectionsSeparate = false;
	
	/** Specify how vertex colors should be imported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes")
	EInterchangeVertexColorImportOption VertexColorImportOption = EInterchangeVertexColorImportOption::IVCIO_Replace;

	/** Specify an override color for use when the Vertex Color Import Option setting is set to Override. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes")
	FColor VertexOverrideColor;

	/** If enabled, normals in the imported mesh are ignored and recomputed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes", meta = (SubCategory = "Build"))
	bool bRecomputeNormals = true;

	/** If enabled, tangents in the imported mesh are ignored and recomputed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes", meta = (SubCategory = "Build"))
	bool bRecomputeTangents = true;

	/** If enabled, tangents are recomputed using MikkTSpace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes", meta = (SubCategory = "Build", editcondition = "bRecomputeTangents"))
	bool bUseMikkTSpace = true;

	/** If enabled, normals are computed using the surface area and the corner angle of the triangle as a ratio. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes", meta = (SubCategory = "Build", editcondition = "bRecomputeNormals"))
	bool bComputeWeightedNormals = false;

	/** If true, tangents are stored at 16-bit vs 8-bit precision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes", meta = (SubCategory = "Build"))
	bool bUseHighPrecisionTangentBasis = false;

	/** If true, UVs are stored at full floating-point precision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes", meta = (SubCategory = "Build"))
	bool bUseFullPrecisionUVs = false;

	/** If enabled, UVs are converted to 16-bit by a legacy truncation process instead of the default rounding process. This may avoid differences when reimporting older content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes", meta = (SubCategory = "Build"))
	bool bUseBackwardsCompatibleF16TruncUVs = false;

	/** If true, degenerate triangles are removed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Meshes", meta = (SubCategory = "Build"))
	bool bRemoveDegenerates = false;
#if WITH_EDITOR
	virtual bool IsPropertyChangeNeedRefresh(const FPropertyChangedEvent& PropertyChangedEvent) override
	{
		if (PropertyChangedEvent.Property->GetName() == GET_MEMBER_NAME_CHECKED(UInterchangeGenericCommonMeshesProperties, ForceAllMeshAsType))
		{
			return true;
		}
		if (PropertyChangedEvent.Property->GetName() == GET_MEMBER_NAME_CHECKED(UInterchangeGenericCommonMeshesProperties, bAutoDetectMeshType))
		{
			return true;
		}
		if (PropertyChangedEvent.Property->GetName() == GET_MEMBER_NAME_CHECKED(UInterchangeGenericCommonMeshesProperties, bBakeMeshes))
		{
			return true;
		}
		return Super::IsPropertyChangeNeedRefresh(PropertyChangedEvent);
	}
#endif //WITH_EDITOR
};

UCLASS(BlueprintType, hidedropdown, Experimental)
class INTERCHANGEPIPELINES_API UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties : public UInterchangePipelineBase
{
	GENERATED_BODY()
public:
	/** If enabled, only animations are imported from the source. You must also set a valid skeleton. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Skeletal Meshes and Animations")
	bool bImportOnlyAnimations = false;

	/** Skeleton to use for imported asset. When importing a skeletal mesh, leaving this as "None" will create a new skeleton. When importing an animation, this must be specified. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Skeletal Meshes and Animations")
	TWeakObjectPtr<USkeleton> Skeleton;

	/** 
	 * If enabled, meshes nested in bone hierarchies will be imported as meshes instead of being converted to bones. If the meshes are not skinned, they are
	 * added to the skeletal mesh and removed from the list of static meshes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Skeletal Meshes and Animations")
	bool bImportMeshesInBoneHierarchy = true;

	/** If enabled, frame 0 is used as the reference pose for skeletal meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Skeletal Meshes and Animations")
	bool bUseT0AsRefPose = false;

	/** If enabled, all static meshes that have morph targets will be imported as skeletal meshes instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes")
	bool bConvertStaticsWithMorphTargetsToSkeletals = false;
#if WITH_EDITOR
	virtual bool IsPropertyChangeNeedRefresh(const FPropertyChangedEvent& PropertyChangedEvent) override
	{
		if (PropertyChangedEvent.Property->GetName() == GET_MEMBER_NAME_CHECKED(UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties, bConvertStaticsWithMorphTargetsToSkeletals))
		{
			return true;
		}
		else if (PropertyChangedEvent.Property->GetName() == GET_MEMBER_NAME_CHECKED(UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties, bImportMeshesInBoneHierarchy))
		{
			return true;
		}
		else if (PropertyChangedEvent.Property->GetName() == GET_MEMBER_NAME_CHECKED(UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties, Skeleton))
		{
			return true;
		}
		return Super::IsPropertyChangeNeedRefresh(PropertyChangedEvent);
	}
#endif //WITH_EDITOR
	virtual bool IsSettingsAreValid(TOptional<FText>& OutInvalidReason) const override
	{
		if (bImportOnlyAnimations && !Skeleton.IsValid())
		{
			OutInvalidReason = NSLOCTEXT("UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties", "SkeletonMustBeSpecified", "When importing only animations, a valid skeleton must be set.");
			return false;
		}
		return Super::IsSettingsAreValid(OutInvalidReason);
	}

	/** Create a UInterchangeSkeletonFactorynode */
	UInterchangeSkeletonFactoryNode* CreateSkeletonFactoryNode(UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& RootJointUid);
};

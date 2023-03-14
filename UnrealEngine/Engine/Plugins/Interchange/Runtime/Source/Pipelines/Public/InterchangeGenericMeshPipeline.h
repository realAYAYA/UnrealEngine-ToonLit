// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangeGenericAssetsPipelineSharedSettings.h"
#include "InterchangePipelineBase.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "InterchangeSkeletalMeshFactoryNode.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeGenericMeshPipeline.generated.h"

class UInterchangeGenericAssetsPipeline;
class UInterchangeMeshNode;
class UInterchangePipelineMeshesUtilities;
class UInterchangeSkeletalMeshFactoryNode;
class UInterchangeSkeletalMeshLodDataNode;
class UInterchangeSkeletonFactoryNode;
class UInterchangeStaticMeshFactoryNode;
class UInterchangeStaticMeshLodDataNode;
class UPhysicsAsset;

/* Hide drop down will make sure the class is not showing in the class picker */
UCLASS(BlueprintType, hidedropdown)
class INTERCHANGEPIPELINES_API UInterchangeGenericMeshPipeline : public UInterchangePipelineBase
{
	GENERATED_BODY()

public:

	//Common Meshes Properties Settings Pointer
	UPROPERTY(Transient)
	TWeakObjectPtr<UInterchangeGenericCommonMeshesProperties> CommonMeshesProperties;

	//Common SkeletalMeshes And Animations Properties Settings Pointer
	UPROPERTY(Transient)
	TWeakObjectPtr<UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties> CommonSkeletalMeshesAndAnimationsProperties;
	
	//////	STATIC_MESHES_CATEGORY Properties //////

	/** If enabled, import the static mesh asset found in the sources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes")
	bool bImportStaticMeshes = true;

	/** If enabled, all translated static mesh nodes will be imported as a single static mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes")
	bool bCombineStaticMeshes = false;

	/**
	 * If enabled, meshes with certain prefixes will be imported as collision primitives for the mesh with the corresponding unprefixed name.
	 * 
	 * Supported prefixes are:
	 * UBX_ Box collision
	 * UCP_ Capsule collision
	 * USP_ Sphere collision
	 * UCX_ Convex collision
	 */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes")
	bool bImportCollisionAccordingToMeshName = true;

	/** If enabled, each UCX collision mesh will be imported as a single convex hull. If disabled, a UCX mesh will be decomposed into its separate pieces and a convex hull generated for each. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes")
	bool bOneConvexHullPerUCX = true;

	//////	Static Meshes Build settings Properties //////

	/** If enabled this option will allow you to use Nanite rendering at runtime. Can only be used with simple opaque materials. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes", meta = (SubCategory = "Build"))
	bool bBuildNanite = false;

	/** If enabled this option will make sure the staticmesh build a reverse index buffer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes", meta = (SubCategory = "Build"))
	bool bBuildReversedIndexBuffer = true;
	
	/** If enabled this option will generate lightmap for this staticmesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes", meta = (SubCategory = "Build"))
	bool bGenerateLightmapUVs = true;
	
	/** 
	 * Whether to generate the distance field treating every triangle hit as a front face.  
	 * When enabled prevents the distance field from being discarded due to the mesh being open, but also lowers Distance Field AO quality.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes", meta=(SubCategory = "Build", DisplayName="Two-Sided Distance Field Generation"))
	bool bGenerateDistanceFieldAsIfTwoSided = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes", meta = (SubCategory = "Build", DisplayName="Enable Physical Material Mask"))
	bool bSupportFaceRemap = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes", meta = (SubCategory = "Build"))
	int32 MinLightmapResolution = 64;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes", meta = (SubCategory = "Build", DisplayName="Source Lightmap Index"))
	int32 SrcLightmapIndex = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes", meta = (SubCategory = "Build", DisplayName="Destination Lightmap Index"))
	int32 DstLightmapIndex = 1;
	
	/** The local scale applied when building the mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes", meta = (SubCategory = "Build", DisplayName="Build Scale"))
	FVector BuildScale3D = FVector(1.0);
	
	/** 
	 * Scale to apply to the mesh when allocating the distance field volume texture.
	 * The default scale is 1, which is assuming that the mesh will be placed unscaled in the world.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes", meta = (SubCategory = "Build"))
	float DistanceFieldResolutionScale = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes", meta = (SubCategory = "Build"))
	TWeakObjectPtr<class UStaticMesh> DistanceFieldReplacementMesh = nullptr;
	
	/** 
	 * Max Lumen mesh cards to generate for this mesh.
	 * More cards means that surface will have better coverage, but will result in increased runtime overhead.
	 * Set to 0 in order to disable mesh card generation for this mesh.
	 * Default is 12.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Meshes", meta = (SubCategory = "Build"))
	int32 MaxLumenMeshCards = 12;

	//////	SKELETAL_MESHES_CATEGORY Properties //////

	/** If enable, import the animation asset find in the sources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeletal Meshes")
	bool bImportSkeletalMeshes = true;

	/** Re-import partially or totally a skeletal mesh. You can choose betwwen geometry, skinning or everything.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeletal Meshes", meta = (DisplayName = "Import Content Type"))
	EInterchangeSkeletalMeshContentType SkeletalMeshImportContentType;
	
	/** The value of the content type during the last import. This cannot be edited and is set only on successful import or re-import*/
	UPROPERTY()
	EInterchangeSkeletalMeshContentType LastSkeletalMeshImportContentType;

	/** If enable all translated skinned mesh node will be imported has a one skeletal mesh, note that it can still create several skeletal mesh for each different skeleton root joint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeletal Meshes")
	bool bCombineSkeletalMeshes = true;

	/** If enable any morph target shape will be imported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeletal Meshes")
	bool bImportMorphTargets = true;

	/** Enable this option to update Skeleton (of the mesh)'s reference pose. Mesh's reference pose is always updated.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeletal Meshes")
	bool bUpdateSkeletonReferencePose = false;

	/** If checked, create new PhysicsAsset if it doesn't have it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeletal Meshes")
	bool bCreatePhysicsAsset = true;

	/** If this is set, use this specified PhysicsAsset. If its not set and bCreatePhysicsAsset is false, the importer will not generate or set any physic asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeletal Meshes", meta = (editcondition = "!bCreatePhysicsAsset"))
	TWeakObjectPtr<UPhysicsAsset> PhysicsAsset;

	/** Threshold use to decide if two vertex position are equal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeletal Meshes", meta = (SubCategory = "Build"))
	float ThresholdPosition = 0.00002f;
	
	/** Threshold use to decide if two normal, tangents or bi-normals are equal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeletal Meshes", meta = (SubCategory = "Build"))
	float ThresholdTangentNormal = 0.00002f;
	
	/** Threshold use to decide if two UVs are equal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeletal Meshes", meta = (SubCategory = "Build"))
	float ThresholdUV = 0.0009765625f;
	
	/** Threshold to compare vertex position equality when computing morph target deltas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skeletal Meshes", meta = (SubCategory = "Build"))
	float MorphThresholdPosition = 0.015f;

	virtual void AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset) override;

	virtual void PreDialogCleanup(const FName PipelineStackName) override;

protected:
	virtual void ExecutePreImportPipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas) override;

	virtual void ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* InBaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport) override;

	virtual bool CanExecuteOnAnyThread(EInterchangePipelineTask PipelineTask) override
	{
		return true;
	}

	virtual void SetReimportSourceIndex(UClass* ReimportObjectClass, const int32 SourceFileIndex) override;

#if WITH_EDITOR
	/**
	 * This function return true if all UPROPERTYs of the @Struct exist in the provided @Classes.
	 * @Struct UPROPERTY tested must be: not transient, editable
	 * 
	 * @param Classes - The array of UClass that should contains the Struct properties
	 * @param Struct - The struct that has the referenced properties
	 * 
	 */
	static bool DoClassesIncludeAllEditableStructProperties(const TArray<const UClass*>& Classes, const UStruct* Struct);
#endif

private:

	/* Meshes utilities, to parse the translated graph and extract the meshes informations. */
	TObjectPtr<UInterchangePipelineMeshesUtilities> PipelineMeshesUtilities = nullptr;

	/************************************************************************/
	/* Skeletal mesh API BEGIN                                              */

	/**
	 * This function will create any skeletalmesh we need to create according to the pipeline options
	 */
	void ExecutePreImportPipelineSkeletalMesh();

	/** Skeleton factory assets nodes */
	TArray<UInterchangeSkeletonFactoryNode*> SkeletonFactoryNodes;

	/** Create a UInterchangeSkeletonFactorynode */
	UInterchangeSkeletonFactoryNode* CreateSkeletonFactoryNode(const FString& RootJointUid);

	/** Skeletal mesh factory assets nodes */
	TArray<UInterchangeSkeletalMeshFactoryNode*> SkeletalMeshFactoryNodes;

	/**
	 * This function can create a UInterchangeSkeletalMeshFactoryNode
	 * @param MeshUidsPerLodIndex - The MeshUids can represent a SceneNode pointing on a MeshNode or directly a MeshNode
	 */
	UInterchangeSkeletalMeshFactoryNode* CreateSkeletalMeshFactoryNode(const FString& RootJointUid, const TMap<int32, TArray<FString>>& MeshUidsPerLodIndex);

	/** This function can create a UInterchangeSkeletalMeshLodDataNode which represent the LOD data need by the factory to create a lod mesh */
	UInterchangeSkeletalMeshLodDataNode* CreateSkeletalMeshLodDataNode(const FString& NodeName, const FString& NodeUniqueID);

	/**
	 * This function add all lod data node to the skeletal mesh.
	 * @param NodeUidsPerLodIndex - The NodeUids can be a UInterchangeSceneNode or a UInterchangeMeshNode. The scene node can bake each instance of the mesh versus the mesh node will import only the modelled mesh.
	 */
	void AddLodDataToSkeletalMesh(const UInterchangeSkeletonFactoryNode* SkeletonFactoryNode, UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode, const TMap<int32, TArray<FString>>& NodeUidsPerLodIndex);

	/**
	 * This function will finish creating the skeletalmesh asset
	 */
	void PostImportSkeletalMesh(UObject* CreatedAsset, const UInterchangeFactoryBaseNode* FactoryNode);

	/**
	 * This function will finish creating the physics asset with the skeletalmesh render data
	 */
	void PostImportPhysicsAssetImport(UObject* CreatedAsset, const UInterchangeFactoryBaseNode* FactoryNode);
public:
	
	/** Specialize for skeletalmesh */
	void ImplementUseSourceNameForAssetOptionSkeletalMesh(const int32 MeshesImportedNodeCount, const bool bUseSourceNameForAsset);

private:
	/* Skeletal mesh API END                                                */
	/************************************************************************/


	/************************************************************************/
	/* Static mesh API BEGIN                                              */

	/**
	 * This function will create any skeletalmesh we need to create according to the pipeline options
	 */
	void ExecutePreImportPipelineStaticMesh();

	/** Static mesh factory assets nodes */
	TArray<UInterchangeStaticMeshFactoryNode*> StaticMeshFactoryNodes;

	/**
	 * This function can create a UInterchangeStaticMeshFactoryNode
	 * @param MeshUidsPerLodIndex - The MeshUids can represent a SceneNode pointing on a MeshNode or directly a MeshNode
	 */
	UInterchangeStaticMeshFactoryNode* CreateStaticMeshFactoryNode(const TMap<int32, TArray<FString>>& MeshUidsPerLodIndex);

	/** This function can create a UInterchangeStaticMeshLodDataNode which represents the LOD data needed by the factory to create a lod mesh */
	UInterchangeStaticMeshLodDataNode* CreateStaticMeshLodDataNode(const FString& NodeName, const FString& NodeUniqueID);

	/**
	 * This function add all lod data nodes to the static mesh.
	 * @param NodeUidsPerLodIndex - The NodeUids can be a UInterchangeSceneNode or a UInterchangeMeshNode. The scene node can bake each instance of the mesh versus the mesh node will import only the modelled mesh.
	 */
	void AddLodDataToStaticMesh(UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode, const TMap<int32, TArray<FString>>& NodeUidsPerLodIndex);

	/**
	 * Return a reasonable UID and display label for a new mesh factory node.
	 */
	bool MakeMeshFactoryNodeUidAndDisplayLabel(const TMap<int32, TArray<FString>>& MeshUidsPerLodIndex, int32 LodIndex, FString& NewMeshUid, FString& DisplayLabel);

	/* Static mesh API END                                                */
	/************************************************************************/

private:

	UInterchangeBaseNodeContainer* BaseNodeContainer = nullptr;
	TArray<const UInterchangeSourceData*> SourceDatas;

};



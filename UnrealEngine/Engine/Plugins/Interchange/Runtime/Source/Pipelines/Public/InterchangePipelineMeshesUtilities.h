// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeMaterialFactoryNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#include "InterchangePipelineMeshesUtilities.generated.h"

class UInterchangeBaseNodeContainer;
class UInterchangeMeshNode;
class UInterchangeSceneNode;

/*
* This container exists only because UPROPERTY cannot support nested container. See FInterchangeMeshInstance.
*/
USTRUCT(BlueprintType)
struct FInterchangeLodSceneNodeContainer
{
	GENERATED_BODY()

	/**
	 * Each scene node here represents a mesh scene node. If it represents a LOD group, there may be more then one mesh scene node for a specific LOD index.
	 */
	UPROPERTY(EditAnywhere, Category = "Interchange | Pipeline | MeshesInstance")
	TArray<TObjectPtr<const UInterchangeSceneNode>> SceneNodes;
};

/*
* A mesh instance is a description of a translated scene node that points to a translated mesh asset.
* A mesh instance that points to an LOD group can have many LODs and many scene mesh nodes per LOD index.
* A mesh instance that points to a mesh node will have only LOD 0 and will point to one scene mesh node.
*/
USTRUCT(BlueprintType)
struct FInterchangeMeshInstance
{
	GENERATED_BODY()

	FInterchangeMeshInstance()
	{
		LodGroupNode = nullptr;
		bReferenceSkinnedMesh = false;
		bReferenceMorphTarget = false;
		bHasMorphTargets = false;
	}
	/**
	 * This ID represents either a LOD group scene node UID or a mesh scene node UID.
	 */
	UPROPERTY(EditAnywhere, Category = "Interchange | Pipeline | MeshesInstance")
	FString MeshInstanceUid;

	/**
	 * This member is null unless the mesh instance represents a LOD group.
	 */
	UPROPERTY(EditAnywhere, Category = "Interchange | Pipeline | MeshesInstance")
	TObjectPtr<const UInterchangeSceneNode> LodGroupNode;

	UPROPERTY(EditAnywhere, Category = "Interchange | Pipeline | MeshesInstance")
	bool bReferenceSkinnedMesh;

	UPROPERTY(EditAnywhere, Category = "Interchange | Pipeline | MeshesInstance")
	bool bReferenceMorphTarget;

	UPROPERTY(EditAnywhere, Category = "Interchange | Pipeline | MeshesInstance")
	bool bHasMorphTargets;

	/**
	 * Each scene node here represents a mesh scene node. If it represents a LOD group, there may be more then one mesh scene node for a specific LOD index.
	 */
	UPROPERTY(EditAnywhere, Category = "Interchange | Pipeline | MeshesInstance")
	TMap<int32, FInterchangeLodSceneNodeContainer> SceneNodePerLodIndex;

	/**
	 * All mesh geometry referenced by this MeshInstance.
	 */
	UPROPERTY(EditAnywhere, Category = "Interchange | Pipeline | MeshesGeometry")
	TArray<FString> ReferencingMeshGeometryUids;
};

/*
* A mesh geometry is a description of a translated mesh asset node that defines a geometry.
*/
USTRUCT(BlueprintType)
struct FInterchangeMeshGeometry
{
	GENERATED_BODY()

	FInterchangeMeshGeometry()
	{
		MeshNode = nullptr;
	}

	/**
	 * The unique ID of the UInterchangeMeshNode represented by this structure.
	 */
	UPROPERTY(EditAnywhere, Category = "Interchange | Pipeline | MeshesGeometry")
	FString MeshUid;

	/**
	 * The UInterchangeMeshNode pointer represented by this structure.
	 */
	UPROPERTY(EditAnywhere, Category = "Interchange | Pipeline | MeshesGeometry")
	TObjectPtr<const UInterchangeMeshNode> MeshNode = nullptr;

	/**
	 * All mesh instances that refer to this UInterchangeMeshNode pointer.
	 */
	UPROPERTY(EditAnywhere, Category = "Interchange | Pipeline | MeshesGeometry")
	TArray<FString> ReferencingMeshInstanceUids;

	/**
	 * A list of all scene nodes that represent sockets attached to this mesh.
	 */
	UPROPERTY(EditAnywhere, Category = "Interchange | Pipeline | MeshesGeometry")
	TArray<FString> AttachedSocketUids;
};

/*
* Represents the context UInterchangePipelineMeshesUtilities will use when the client queries data.
*/
USTRUCT(BlueprintType)
struct FInterchangePipelineMeshesUtilitiesContext
{
	GENERATED_BODY()

	/**
	 * If enabled, all static meshes are converted to skeletal meshes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange | Pipeline | MeshesContext")
	bool bConvertStaticMeshToSkeletalMesh = false;

	/**
	 * If enabled, all skeletal meshes are converted to static meshes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange | Pipeline | MeshesContext")
	bool bConvertSkeletalMeshToStaticMesh = false;

	/**
	 * If enabled, all static meshes that have morph targets will be imported as skeletal meshes instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange | Pipeline | MeshesContext", meta = (DisplayName = "Convert Static Meshes with Morph Targets to Skeletal Meshes"))
	bool bConvertStaticsWithMorphTargetsToSkeletals = false;

	/**
	 * If enabled, meshes nested in bone hierarchies are imported as meshes instead of being converted to bones. If the meshes are not skinned, they are
	 * added to the skeletal mesh and removed from the list of static meshes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange | Pipeline | MeshesContext")
	bool bImportMeshesInBoneHierarchy = true;

	/**
	 * When querying geometry, this flag will not add MeshGeometry if there is a scene node that points to a geometry.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange | Pipeline | MeshesContext")
	bool bQueryGeometryOnlyIfNoInstance = true;

	bool IsStaticMeshInstance(const FInterchangeMeshInstance& MeshInstance, UInterchangeBaseNodeContainer* BaseNodeContainer);
	bool IsSkeletalMeshInstance(const FInterchangeMeshInstance& MeshInstance, UInterchangeBaseNodeContainer* BaseNodeContainer);
	bool IsSkeletalMeshInstance(const FInterchangeMeshInstance& MeshInstance, UInterchangeBaseNodeContainer* BaseNodeContainer, bool& bOutIsStaticMeshNestedInSkeleton);
	bool IsStaticMeshGeometry(const FInterchangeMeshGeometry& MeshGeometry);
	bool IsSkeletalMeshGeometry(const FInterchangeMeshGeometry& MeshGeometry);
};

/**/
UCLASS(BlueprintType)
class INTERCHANGEPIPELINES_API UInterchangePipelineMeshesUtilities : public UObject
{
	GENERATED_BODY()
public:
	/**
	* Create an instance of UInterchangePipelineMeshesUtilities.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	static UInterchangePipelineMeshesUtilities* CreateInterchangePipelineMeshesUtilities(UInterchangeBaseNodeContainer* BaseNodeContainer);

	/**
	* Get the unique IDs of all mesh instances.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	void GetAllMeshInstanceUids(TArray<FString>& MeshInstanceUids) const;

	/**
	* Iterate over all mesh instances.
	*/
	void IterateAllMeshInstance(TFunctionRef<void(const FInterchangeMeshInstance&)> IterationLambda) const;

	/**
	* Get the unique IDs of all skinned mesh instances.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	void GetAllSkinnedMeshInstance(TArray<FString>& MeshInstanceUids) const;

	/**
	* Iterate over all skinned mesh instances.
	*/
	void IterateAllSkinnedMeshInstance(TFunctionRef<void(const FInterchangeMeshInstance&)> IterationLambda) const;

	/**
	* Get the unique IDs of all static mesh instances.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	void GetAllStaticMeshInstance(TArray<FString>& MeshInstanceUids) const;

	/**
	* Iterate over all static mesh instances.
	*/
	void IterateAllStaticMeshInstance(TFunctionRef<void(const FInterchangeMeshInstance&)> IterationLambda) const;

	/**
	* Get the unique IDs of all mesh geometry.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	void GetAllMeshGeometry(TArray<FString>& MeshGeometryUids) const;
		
	/**
	* Iterate over all mesh geometry.
	*/
	void IterateAllMeshGeometry(TFunctionRef<void(const FInterchangeMeshGeometry&)> IterationLambda) const;

	/**
	* Get the unique IDs of all skinned mesh geometry.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	void GetAllSkinnedMeshGeometry(TArray<FString>& MeshGeometryUids) const;

	/**
	* Iterate over all skinned mesh geometry.
	*/
	void IterateAllSkinnedMeshGeometry(TFunctionRef<void(const FInterchangeMeshGeometry&)> IterationLambda) const;

	/**
	* Get the unique IDs of all static mesh geometry.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	void GetAllStaticMeshGeometry(TArray<FString>& MeshGeometryUids) const;

	/**
	* Iterate over all static mesh geometry.
	*/
	void IterateAllStaticMeshGeometry(TFunctionRef<void(const FInterchangeMeshGeometry&)> IterationLambda) const;

	/**
	* Get the unique IDs of all non-instanced mesh geometry.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	void GetAllMeshGeometryNotInstanced(TArray<FString>& MeshGeometryUids) const;

	/**
	* Iterate over all non-instanced mesh geometry.
	*/
	void IterateAllMeshGeometryNotIntanced(TFunctionRef<void(const FInterchangeMeshGeometry&)> IterationLambda) const;

	/**
	* Return true if there is an existing FInterchangeMeshInstance that matches the MeshInstanceUid key.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	bool IsValidMeshInstanceUid(const FString& MeshInstanceUid) const;

	/**
	* Get the instanced mesh from the unique ID.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	const FInterchangeMeshInstance& GetMeshInstanceByUid(const FString& MeshInstanceUid) const;

	/**
	* Return true if there is an existing FInterchangeMeshGeometry that matches the MeshInstanceUid key.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	bool IsValidMeshGeometryUid(const FString& MeshGeometryUid) const;

	/**
	* Get the geometry mesh from the unique ID.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	const FInterchangeMeshGeometry& GetMeshGeometryByUid(const FString& MeshGeometryUid) const;

	/**
	* Get all instanced mesh UIDs that use the mesh geometry unique ID.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	void GetAllMeshInstanceUidsUsingMeshGeometryUid(const FString& MeshGeometryUid, TArray<FString>& MeshInstanceUids) const;

	/**
	* Iterate over all instanced mesh UIDs that use the mesh geometry unique ID.
	*/
	void IterateAllMeshInstanceUsingMeshGeometry(const FString& MeshGeometryUid, TFunctionRef<void(const FInterchangeMeshInstance&)> IterationLambda) const;

	/**
	* Return a list of skinned FInterchangeMeshInstance UIDs that can be combined together.
	* We cannot create a skinned mesh that has multiple skeleton root nodes. This function returns a combined MeshInstance per skeleton roots.
	*/
	void GetCombinedSkinnedMeshInstances(TMap<FString, TArray<FString>>& OutMeshInstanceUidsPerSkeletonRootUid) const;
	
	/**
	* Return the skeleton root node UID. This is the UID for a UInterchangeSceneNode that has a "Joint" specialized type.
	* Return an empty string if the MeshInstanceUid parameter points to nothing.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	FString GetMeshInstanceSkeletonRootUid(const FString& MeshInstanceUid) const;

	FString GetMeshInstanceSkeletonRootUid(const FInterchangeMeshInstance& MeshInstance) const;

	/**
	* Return the skeleton root node UID. This is the UID for a UInterchangeSceneNode that has a "Joint" specialized type.
	* Return an empty string if the MeshGeometryUid parameter points to nothing.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	FString GetMeshGeometrySkeletonRootUid(const FString& MeshGeometryUid) const;

	FString GetMeshGeometrySkeletonRootUid(const FInterchangeMeshGeometry& MeshGeometry) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline | Meshes")
	void SetContext(const FInterchangePipelineMeshesUtilitiesContext& Context) const
	{
		CurrentDataContext = Context;
	}
	
protected:
	TMap<FString, FInterchangeMeshGeometry> MeshGeometriesPerMeshUid;
	TMap<FString, FInterchangeMeshInstance> MeshInstancesPerMeshInstanceUid;
	TMap<FString, FString> SkeletonRootUidPerMeshUid;

	UInterchangeBaseNodeContainer* BaseNodeContainer;

	mutable FInterchangePipelineMeshesUtilitiesContext CurrentDataContext;
};

namespace UE::Interchange::MeshesUtilities
{

	/**
	 * Applies material slot dependencies stored in SlotMaterialDependencies to FactoryNode.
	 * If the caller want to support bKeepSectionSeparate feature it must provide a valid ExistingSlotMaterialDependenciesPtr.
	 */
	template<class T>
	void ApplySlotMaterialDependencies(T& FactoryNode
		, const TMap<FString, FString>& SlotMaterialDependencies
		, const UInterchangeBaseNodeContainer& NodeContainer
		, TMap<FString, FString> *ExistingSlotMaterialDependenciesPtr)
	{
		bool bKeepSectionsSeparate = false;
		int32 IndexCounter = 0; //Only use when bKeepSectionsSeparate is true
		if (ExistingSlotMaterialDependenciesPtr)
		{
			FactoryNode.GetCustomKeepSectionsSeparate(bKeepSectionsSeparate);
			IndexCounter = ExistingSlotMaterialDependenciesPtr->Num();
		}
		for (const TPair<FString, FString>& SlotMaterialDependency : SlotMaterialDependencies)
		{
			FString NewSlotName = SlotMaterialDependency.Key;
			if (bKeepSectionsSeparate && ExistingSlotMaterialDependenciesPtr)
			{
				if (ExistingSlotMaterialDependenciesPtr->Contains(NewSlotName))
				{
					NewSlotName += TEXT("_Section") + FString::FromInt(IndexCounter);
				}
				ExistingSlotMaterialDependenciesPtr->Add(NewSlotName, SlotMaterialDependency.Value);
				IndexCounter++;
			}
			const FString MaterialFactoryNodeUid = UInterchangeBaseMaterialFactoryNode::GetMaterialFactoryNodeUidFromMaterialNodeUid(SlotMaterialDependency.Value);
			FactoryNode.SetSlotMaterialDependencyUid(NewSlotName, MaterialFactoryNodeUid);
			if (UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode = Cast<UInterchangeBaseMaterialFactoryNode>(NodeContainer.GetFactoryNode(MaterialFactoryNodeUid)))
			{
				bool IsMaterialImportEnabled = true;
				MaterialFactoryNode->GetCustomIsMaterialImportEnabled(IsMaterialImportEnabled);
				MaterialFactoryNode->SetEnabled(IsMaterialImportEnabled);

				// Create a factory dependency so Material asset are imported before the static mesh asset
				TArray<FString> FactoryDependencies;
				FactoryNode.GetFactoryDependencies(FactoryDependencies);
				if (!FactoryDependencies.Contains(MaterialFactoryNodeUid))
				{
					FactoryNode.AddFactoryDependencyUid(MaterialFactoryNodeUid);
				}
			}
		}

		if (ExistingSlotMaterialDependenciesPtr)
		{
			ExistingSlotMaterialDependenciesPtr->Append(SlotMaterialDependencies);
		}
	}

	template<class T>
	void ReorderSlotMaterialDependencies(T& FactoryNode, const UInterchangeBaseNodeContainer& NodeContainer)
	{
		TMap<FString, FString> SlotMaterialDependencies;
		FactoryNode.GetSlotMaterialDependencies(SlotMaterialDependencies);

		//Empty all slot dependencies, they will be added backin the correct order
		FactoryNode.ResetSlotMaterialDependencies();

		TArray<FString> KeyReorder;
		KeyReorder.Reserve(SlotMaterialDependencies.Num());
		TArray<FString> MissingSuffixMaterialNames;
		MissingSuffixMaterialNames.Reserve(SlotMaterialDependencies.Num());
		//Reorder material using the skinXX workflow
		for (const TPair<FString, FString>& SlotMaterialDependency : SlotMaterialDependencies)
		{
			bool bHasSuffix = false;
			const FString& MaterialName = SlotMaterialDependency.Key;
			if (MaterialName.Len() > 6)
			{
				int32 Offset = MaterialName.Find(TEXT("_SKIN"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				if (Offset != INDEX_NONE)
				{
					// Chop off the material name so we are left with the number in _SKINXX
					FString SkinXXNumber = MaterialName.Right(MaterialName.Len() - (Offset + 1)).RightChop(4);
					if (SkinXXNumber.IsNumeric())
					{
						int32 TmpIndex = FPlatformString::Atoi(*SkinXXNumber);
						if (TmpIndex >= 0)
						{
							while (KeyReorder.Num() <= TmpIndex)
							{
								KeyReorder.AddDefaulted();
							}
							check(KeyReorder.IsValidIndex(TmpIndex));
							if (KeyReorder[TmpIndex].IsEmpty())
							{
								bHasSuffix = true;
								KeyReorder[TmpIndex] = MaterialName;
								continue;
							}
						}
					}
				}
			}
			
			if (!bHasSuffix)
			{
				MissingSuffixMaterialNames.Add(MaterialName);
			}
		}

		//The map is MaterialName, MaterialUid
		TMap<FString, FString> ReorderedSlotMaterialDependencies;
		ReorderedSlotMaterialDependencies.Reserve(SlotMaterialDependencies.Num());

		//Start by adding the KeyReorder material
		for (const FString& MaterialName : KeyReorder)
		{
			if (MaterialName.IsEmpty())
			{
				continue;
			}
			const FString& SlotMaterialUid = SlotMaterialDependencies.FindChecked(MaterialName);
			ReorderedSlotMaterialDependencies.Add(MaterialName, SlotMaterialUid);
		}
		//Add the missing suffix material
		for (const FString& MaterialName : MissingSuffixMaterialNames)
		{
			const FString& SlotMaterialUid = SlotMaterialDependencies.FindChecked(MaterialName);
			ReorderedSlotMaterialDependencies.Add(MaterialName, SlotMaterialUid);
		}

		check(ReorderedSlotMaterialDependencies.Num() == SlotMaterialDependencies.Num());

		for (const TPair<FString, FString>& SlotMaterialDependency : ReorderedSlotMaterialDependencies)
		{
			FactoryNode.SetSlotMaterialDependencyUid(SlotMaterialDependency.Key, SlotMaterialDependency.Value);
		}
	}
}
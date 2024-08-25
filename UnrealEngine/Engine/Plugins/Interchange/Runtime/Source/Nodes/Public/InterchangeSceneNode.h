// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Nodes/InterchangeBaseNode.h"

#include "InterchangeSceneNode.generated.h"

class UInterchangeBaseNodeContainer;
//Interchange namespace
namespace UE
{
	namespace Interchange
	{

		struct INTERCHANGENODES_API FSceneNodeStaticData : public FBaseNodeStaticData
		{
			static const FAttributeKey& GetNodeSpecializeTypeBaseKey();
			static const FAttributeKey& GetMaterialDependencyUidsBaseKey();
			static const FString& GetTransformSpecializeTypeString();
			static const FString& GetJointSpecializeTypeString();
			static const FString& GetLodGroupSpecializeTypeString();
			static const FString& GetSlotMaterialDependenciesString();
			static const FString& GetMorphTargetCurveWeightsKey();
		};

	}//ns Interchange
}//ns UE

/**
 * The scene node represents a transform node in the scene.
 * Scene nodes can have user-defined attributes. Use UInterchangeUserDefinedAttributesAPI to get and set user-defined attribute data.
 */
UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangeSceneNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

public:
	UInterchangeSceneNode();

	/**
	 * Override Serialize() to restore SlotMaterialDependencies on load.
	 */
	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		if (Ar.IsLoading() && bIsInitialized)
		{
			SlotMaterialDependencies.RebuildCache();
		}
	}

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	virtual FString GetTypeName() const override;

#if WITH_EDITOR
	virtual FString GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;

	virtual FString GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;
#endif //WITH_EDITOR
	/**
	 * Icon names are created by adding "InterchangeIcon_" in front of the specialized type. If there is no special type, the function will return NAME_None, which will use the default icon.
	 */
	virtual FName GetIconName() const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	bool IsSpecializedTypeContains(const FString& SpecializedType) const;

	/** Get the specialized type this scene node represents (for example, Joint or LODGroup). */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	int32 GetSpecializedTypeCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	void GetSpecializedType(const int32 Index, FString& OutSpecializedType) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	void GetSpecializedTypes(TArray<FString>& OutSpecializedTypes) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	bool AddSpecializedType(const FString& SpecializedType);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	bool RemoveSpecializedType(const FString& SpecializedType);

	/** Get which asset, if any, a scene node is instantiating. Return false if the Attribute was not set previously.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	bool GetCustomAssetInstanceUid(FString& AttributeValue) const;

	/** Add an asset for this scene node to instantiate. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	bool SetCustomAssetInstanceUid(const FString& AttributeValue);


	/**
	 * Get the default scene node local transform.
	 * The default transform is the local transform of the node (no bind pose, no time evaluation).
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	bool GetCustomLocalTransform(FTransform& AttributeValue) const;

	/**
	 * Set the default scene node local transform.
	 * The default transform is the local transform of the node (no bind pose, no time evaluation).
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	bool SetCustomLocalTransform(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FTransform& AttributeValue, bool bResetCache = true);

	/** Get the default scene node global transform. This value is computed from the local transforms of all parent scene nodes. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	bool GetCustomGlobalTransform(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FTransform& GlobalOffsetTransform, FTransform& AttributeValue, bool bForceRecache = false) const;


	/** Get the geometric offset. Any mesh attached to this scene node will be offset using this transform. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	bool GetCustomGeometricTransform(FTransform& AttributeValue) const;

	/** Set the geometric offset. Any mesh attached to this scene node will be offset using this transform. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene")
	bool SetCustomGeometricTransform(const FTransform& AttributeValue);

	/***********************************************************************************************
	* Skeleton bind bone API Begin
	* 
	* Bind pose transform is the transform of the joint when the binding with the mesh was done.
	* This attribute should be set only if this scene node represent a joint.
	* 
	* Time-zero transform is the transform of the node at time zero.
	* Pipelines often have the option to evaluate the joint at time zero to create the bind pose.
	* Time-zero bind pose is also used if the translator did not find any bind pose, or if we import
	* an unskinned mesh as a skeletal smesh (rigid mesh).
	*/

	/** Get the local transform of the bind pose scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Joint")
	bool GetCustomBindPoseLocalTransform(FTransform& AttributeValue) const;

	/** Set the local transform of the bind pose scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Joint")
	bool SetCustomBindPoseLocalTransform(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FTransform& AttributeValue, bool bResetCache = true);

	/** Get the global transform of the bind pose scene node. This value is computed from the local transforms of all parent bind poses. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Joint")
	bool GetCustomBindPoseGlobalTransform(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FTransform& GlobalOffsetTransform, FTransform& AttributeValue, bool bForceRecache = false) const;

	//Time zero transform is the transform of the node at time zero.
	//This is useful when there is no bind pose or when we import rigid mesh.

	/** Get the local transform of the time-zero scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Joint")
	bool GetCustomTimeZeroLocalTransform(FTransform& AttributeValue) const;

	/** Set the local transform of the time-zero scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Joint")
	bool SetCustomTimeZeroLocalTransform(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FTransform& AttributeValue, bool bResetCache = true);

	/** Get the global transform of the time-zero scene node. This value is computed from the local transforms of all parent time-zero scene nodes. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Joint")
	bool GetCustomTimeZeroGlobalTransform(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FTransform& GlobalOffsetTransform, FTransform& AttributeValue, bool bForceRecache = false) const;

	/*
	* Skeleton bone API End
	***********************************************************************************************/

	/** This static function ensures all the global transform caches are reset for all the UInterchangeSceneNode nodes in the UInterchangeBaseNodeContainer. */
	static void ResetAllGlobalTransformCaches(const UInterchangeBaseNodeContainer* BaseNodeContainer);

	/** This static function ensures all the global transform caches are reset for all the UInterchangeSceneNode nodes children in the UInterchangeBaseNodeContainer. */
	static void ResetGlobalTransformCachesOfNodeAndAllChildren(const UInterchangeBaseNodeContainer* BaseNodeContainer, const UInterchangeBaseNode* ParentNode);

	/**
	 * Retrieve the correspondence table between slot names and assigned materials for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Meshes")
	void GetSlotMaterialDependencies(TMap<FString, FString>& OutMaterialDependencies) const;

	/**
	 * Retrieve the Material dependency for a given slot of this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Meshes")
	bool GetSlotMaterialDependencyUid(const FString& SlotName, FString& OutMaterialDependency) const;

	/**
	 * Add the specified Material dependency to a specific slot name of this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Meshes")
	bool SetSlotMaterialDependencyUid(const FString& SlotName, const FString& MaterialDependencyUid);

	/**
	 * Remove the Material dependency associated with the given slot name from this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Meshes")
	bool RemoveSlotMaterialDependencyUid(const FString& SlotName);

	/** Set MorphTarget with given weight. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetMorphTargetCurveWeight(const FString& MorphTargetName, const float& Weight);

	/** Get MorphTargets and their weights. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	void GetMorphTargetCurveWeights(TMap<FString, float>& OutMorphTargetCurveWeights) const;


	/** Set the Animation Asset To Play by this Scene Node. Only relevant for SkeletalMeshActors (that is, SceneNodes that are instantiating Skeletal Meshes). */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomAnimationAssetUidToPlay(const FString& AttributeValue);
	/** Get the Animation Asset To Play by this Scene Node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomAnimationAssetUidToPlay(FString& AttributeValue) const;

private:

	bool GetGlobalTransformInternal(const UE::Interchange::FAttributeKey LocalTransformKey
		, TOptional<FTransform>& CacheTransform
		, const UInterchangeBaseNodeContainer* BaseNodeContainer
		, const FTransform& GlobalOffsetTransform
		, FTransform& AttributeValue
		, bool bForceRecache) const;

	//Scene node default local transforms.
	const UE::Interchange::FAttributeKey Macro_CustomLocalTransformKey = UE::Interchange::FAttributeKey(TEXT("LocalTransform"));
	
	//Scene node Local bind pose transforms (the specialize type should be set to joint)
	const UE::Interchange::FAttributeKey Macro_CustomBindPoseLocalTransformKey = UE::Interchange::FAttributeKey(TEXT("BindPoseLocalTransform"));
	
	//Scene node local transforms at time zero. This attribute is important for rigid mesh import or if the translator did not fill the bind pose.
	const UE::Interchange::FAttributeKey Macro_CustomTimeZeroLocalTransformKey = UE::Interchange::FAttributeKey(TEXT("TimeZeroLocalTransform"));
	
	//A scene node can have a transform apply to the mesh it reference.
	const UE::Interchange::FAttributeKey Macro_CustomGeometricTransformKey = UE::Interchange::FAttributeKey(TEXT("GeometricTransform"));

	//A scene node can reference an asset. Asset can be Mesh, Light, camera...
	const UE::Interchange::FAttributeKey Macro_CustomAssetInstanceUidKey = UE::Interchange::FAttributeKey(TEXT("AssetInstanceUid"));
	
	//A scene node can represent many special types
	UE::Interchange::TArrayAttributeHelper<FString> NodeSpecializeTypes;

	//A scene node can have is own set of materials for the mesh it reference.
	UE::Interchange::TMapAttributeHelper<FString, FString> SlotMaterialDependencies;

	//A scene node can have different MorphTarget curve settings:
	UE::Interchange::TMapAttributeHelper<FString, float> MorphTargetCurveWeights;

	//A scene node can reference an animation asset on top of base asset:
	const UE::Interchange::FAttributeKey Macro_CustomAnimationAssetUidToPlayKey = UE::Interchange::FAttributeKey(TEXT("AnimationAssetUidToPlay"));

	//mutable caches for global transforms
	mutable TOptional<FTransform> CacheGlobalTransform;
	mutable TOptional<FTransform> CacheBindPoseGlobalTransform;
	mutable TOptional<FTransform> CacheTimeZeroGlobalTransform;
};

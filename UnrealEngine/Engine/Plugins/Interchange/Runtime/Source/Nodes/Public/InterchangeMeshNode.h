// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeBaseNode.h"

#include "InterchangeMeshNode.generated.h"

//Interchange namespace
namespace UE
{
	namespace Interchange
	{

		struct INTERCHANGENODES_API FMeshNodeStaticData : public FBaseNodeStaticData
		{
			static const FAttributeKey& PayLoadKey();
			static const FAttributeKey& PayLoadTypeKey();
			static const FAttributeKey& IsSkinnedMeshKey();
			static const FAttributeKey& IsMorphTargetKey();
			static const FAttributeKey& MorphTargetNameKey();
			static const FAttributeKey& GetSkeletonDependenciesKey();
			static const FAttributeKey& GetMorphTargetDependenciesKey();
			static const FAttributeKey& GetSceneInstancesUidsKey();
			static const FAttributeKey& GetSlotMaterialDependenciesKey();
		};

	}//ns Interchange
}//ns UE

UENUM(BlueprintType)
enum class EInterchangeMeshPayLoadType : uint8
{
	NONE = 0,
	STATIC,
	SKELETAL,
	MORPHTARGET
};


USTRUCT(BlueprintType)
struct INTERCHANGENODES_API FInterchangeMeshPayLoadKey
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange | Mesh")
	FString UniqueId = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange | Mesh")
	EInterchangeMeshPayLoadType Type = EInterchangeMeshPayLoadType::NONE;

	FInterchangeMeshPayLoadKey() {}

	FInterchangeMeshPayLoadKey(const FString& InUniqueId, const EInterchangeMeshPayLoadType& InType)
		: UniqueId(InUniqueId)
		, Type(InType)
	{
	}
};


UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangeMeshNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

public:
	UInterchangeMeshNode();

#if WITH_EDITOR
	virtual FString GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;

	virtual FString GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;
#endif //WITH_EDITOR

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	virtual FString GetTypeName() const override;

	/**
	 * Icon names are created by adding "InterchangeIcon_" in front of the specialized type. If there is no special type, the function will return NAME_None, which will use the default icon.
	 */
	virtual FName GetIconName() const override;
	
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
	 * Return true if this node represents a skinned mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool IsSkinnedMesh() const;

	/**
	 * Set the IsSkinnedMesh attribute to determine whether this node represents a skinned mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetSkinnedMesh(const bool bIsSkinnedMesh);

	/**
	 * Return true if this node represents a morph target.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool IsMorphTarget() const;

	/**
	 * Set the IsMorphTarget attribute to determine whether this node represents a morph target.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetMorphTarget(const bool bIsMorphTarget);

	/**
	 * Get the morph target name.
	 * Return true if we successfully retrieved the MorphTargetName attribute.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool GetMorphTargetName(FString& OutMorphTargetName) const;

	/**
	 * Set the MorphTargetName attribute to determine the name of the morph target.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetMorphTargetName(const FString& MorphTargetName);

	virtual const TOptional<FInterchangeMeshPayLoadKey> GetPayLoadKey() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	virtual void SetPayLoadKey(const FString& PayLoadKey, const EInterchangeMeshPayLoadType& PayLoadType);
	
	/** Query the vertex count of this mesh. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool GetCustomVertexCount(int32& AttributeValue) const;

	/** Set the vertex count of this mesh. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetCustomVertexCount(const int32& AttributeValue);

	/** Query the polygon count of this mesh. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool GetCustomPolygonCount(int32& AttributeValue) const;

	/** Set the polygon count of this mesh. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetCustomPolygonCount(const int32& AttributeValue);

	/** Query the bounding box of this mesh. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool GetCustomBoundingBox(FBox& AttributeValue) const;

	/** Set the bounding box of this mesh. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetCustomBoundingBox(const FBox& AttributeValue);

	/** Query whether this mesh has vertex normals. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool GetCustomHasVertexNormal(bool& AttributeValue) const;

	/** Set the vertex normal attribute of this mesh. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetCustomHasVertexNormal(const bool& AttributeValue);

	/** Query whether this mesh has vertex bi-normals. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool GetCustomHasVertexBinormal(bool& AttributeValue) const;

	/** Set the vertex bi-normal attribute of this mesh. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetCustomHasVertexBinormal(const bool& AttributeValue);

	/** Query whether this mesh has vertex tangents. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool GetCustomHasVertexTangent(bool& AttributeValue) const;

	/** Set the vertex tangent attribute of this mesh. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetCustomHasVertexTangent(const bool& AttributeValue);

	/** Query whether this mesh has smoothing groups. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool GetCustomHasSmoothGroup(bool& AttributeValue) const;

	/** Set the smoothing group attribute of this mesh. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetCustomHasSmoothGroup(const bool& AttributeValue);

	/** Query whether this mesh has vertex colors. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool GetCustomHasVertexColor(bool& AttributeValue) const;

	/** Set the vertex color attribute of this mesh. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetCustomHasVertexColor(const bool& AttributeValue);

	/** Query the UV count of this mesh. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool GetCustomUVCount(int32& AttributeValue) const;

	/** Set the UV count attribute of this mesh. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetCustomUVCount(const int32& AttributeValue);

	/**
	 * Retrieve the number of skeleton dependencies for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	int32 GetSkeletonDependeciesCount() const;

	/**
	 * Retrieve the skeleton dependency for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	void GetSkeletonDependencies(TArray<FString>& OutDependencies) const;

	/**
	 * Retrieve the specified skeleton dependency for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	void GetSkeletonDependency(const int32 Index, FString& OutDependency) const;

	/**
	 * Add the specified skeleton dependency to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetSkeletonDependencyUid(const FString& DependencyUid);

	/**
	 * Remove the specified skeleton dependency from this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool RemoveSkeletonDependencyUid(const FString& DependencyUid);

	/**
	 * Retrieve the number of morph target dependencies for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	int32 GetMorphTargetDependeciesCount() const;

	/**
	 * Retrieve all morph target dependencies for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	void GetMorphTargetDependencies(TArray<FString>& OutDependencies) const;

	/**
	 * Retrieve the specified morph target dependency for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	void GetMorphTargetDependency(const int32 Index, FString& OutDependency) const;

	/**
	 * Add the specified morph target dependency to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetMorphTargetDependencyUid(const FString& DependencyUid);

	/**
	 * Remove the specified morph target dependency from this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool RemoveMorphTargetDependencyUid(const FString& DependencyUid);

	/**
	 * Retrieve the number of scene nodes instancing this mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	int32 GetSceneInstanceUidsCount() const;

	/**
	 * Retrieve the asset instances this scene node refers to.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	void GetSceneInstanceUids(TArray<FString>& OutDependencies) const;

	/**
	 * Retrieve the asset instance this scene node refers to.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	void GetSceneInstanceUid(const int32 Index, FString& OutDependency) const;

	/**
	 * Add the specified asset instance this scene node refers to.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool SetSceneInstanceUid(const FString& DependencyUid);

	/**
	 * Remove the specified asset instance this scene node refers to.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Mesh")
	bool RemoveSceneInstanceUid(const FString& DependencyUid);

	/**
	 * Retrieve the correspondence table between slot names and assigned materials for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	void GetSlotMaterialDependencies(TMap<FString, FString>& OutMaterialDependencies) const;

	/**
	 * Retrieve the specified Material dependency for a given slot of this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	bool GetSlotMaterialDependencyUid(const FString& SlotName, FString& OutMaterialDependency) const;

	/**
	 * Add the specified Material dependency to a specific slot name of this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	bool SetSlotMaterialDependencyUid(const FString& SlotName, const FString& MaterialDependencyUid);

	/**
	 * Remove the Material dependency associated with the given slot name from this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	bool RemoveSlotMaterialDependencyUid(const FString& SlotName);

private:
	const UE::Interchange::FAttributeKey Macro_CustomVertexCountKey = UE::Interchange::FAttributeKey(TEXT("VertexCount"));
	const UE::Interchange::FAttributeKey Macro_CustomPolygonCountKey = UE::Interchange::FAttributeKey(TEXT("PolygonCount"));
	const UE::Interchange::FAttributeKey Macro_CustomBoundingBoxKey = UE::Interchange::FAttributeKey(TEXT("BoundingBox"));
	const UE::Interchange::FAttributeKey Macro_CustomHasVertexNormalKey = UE::Interchange::FAttributeKey(TEXT("HasVertexNormal"));
	const UE::Interchange::FAttributeKey Macro_CustomHasVertexBinormalKey = UE::Interchange::FAttributeKey(TEXT("HasVertexBinormal"));
	const UE::Interchange::FAttributeKey Macro_CustomHasVertexTangentKey = UE::Interchange::FAttributeKey(TEXT("HasVertexTangent"));
	const UE::Interchange::FAttributeKey Macro_CustomHasSmoothGroupKey = UE::Interchange::FAttributeKey(TEXT("HasSmoothGroup"));
	const UE::Interchange::FAttributeKey Macro_CustomHasVertexColorKey = UE::Interchange::FAttributeKey(TEXT("HasVertexColor"));
	const UE::Interchange::FAttributeKey Macro_CustomUVCountKey = UE::Interchange::FAttributeKey(TEXT("UVCount"));

	UE::Interchange::TArrayAttributeHelper<FString> SkeletonDependencies;
	UE::Interchange::TArrayAttributeHelper<FString> MaterialDependencies;
	UE::Interchange::TArrayAttributeHelper<FString> MorphTargetDependencies;
	UE::Interchange::TArrayAttributeHelper<FString> SceneInstancesUids;
	
	UE::Interchange::TMapAttributeHelper<FString, FString> SlotMaterialDependencies;
};

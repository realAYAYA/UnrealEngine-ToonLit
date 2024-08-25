// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeMeshFactoryNode.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#if WITH_ENGINE
#include "Engine/SkeletalMesh.h"
#endif

#include "InterchangeSkeletalMeshFactoryNode.generated.h"

UENUM(BlueprintType)
enum class EInterchangeSkeletalMeshContentType : uint8
{
	All UMETA(DisplayName = "Geometry and Skin Weights", ToolTip = "Imports all skeletal mesh content: geometry and skin weights."),
	Geometry UMETA(DisplayName = "Geometry Only", ToolTip = "Imports the skeletal mesh geometry only. This creates a default skeleton, or maps the geometry to the existing one. You can import morph targets and LODs with the mesh."),
	SkinningWeights UMETA(DisplayName = "Skin Weights Only", ToolTip = "Imports the skeletal mesh skin weights only. No geometry, morph targets, or LODs are imported."),
	MAX,
};

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeSkeletalMeshFactoryNode : public UInterchangeMeshFactoryNode
{
	GENERATED_BODY()

public:
	UInterchangeSkeletalMeshFactoryNode();

	/**
	 * Initialize node data.
	 * @param: UniqueID - The unique ID for this node.
	 * @param DisplayLabel - The name of the node.
	 * @param InAssetClass - The class the SkeletalMesh factory will create for this node.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	void InitializeSkeletalMeshNode(const FString& UniqueID, const FString& DisplayLabel, const FString& InAssetClass);

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	virtual FString GetTypeName() const override;

	/** Get the class this node creates. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	virtual class UClass* GetObjectClass() const override;

public:
	/** Query the skeletal mesh factory skeleton UObject. Return false if the attribute was not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomSkeletonSoftObjectPath(FSoftObjectPath& AttributeValue) const;

	/** Set the skeletal mesh factory skeleton UObject. Return false if the attribute could not be set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomSkeletonSoftObjectPath(const FSoftObjectPath& AttributeValue);

	/** Query whether the skeletal mesh factory should create morph targets. Return false if the attribute was not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomImportMorphTarget(bool& AttributeValue) const;

	/** Set whether the skeletal mesh factory should create morph targets. Return false if the attribute could not be set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomImportMorphTarget(const bool& AttributeValue);

	/** Query whether the skeletal mesh factory should import vertex attributes. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomImportVertexAttributes(bool& AttributeValue) const;

	/** Set whether the skeletal mesh factory should import vertex attributes. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomImportVertexAttributes(const bool& AttributeValue);

	/** Query whether the skeletal mesh factory should create a physics asset. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomCreatePhysicsAsset(bool& AttributeValue) const;

	/** Set whether the skeletal mesh factory should create a physics asset. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomCreatePhysicsAsset(const bool& AttributeValue);

	/** Query a physics asset the skeletal mesh factory should use. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomPhysicAssetSoftObjectPath(FSoftObjectPath& AttributeValue) const;

	/** Set a physics asset the skeletal mesh factory should use. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomPhysicAssetSoftObjectPath(const FSoftObjectPath& AttributeValue);

	/** Query the skeletal mesh import content type. This content type determines whether the factory imports partial or full translated content. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomImportContentType(EInterchangeSkeletalMeshContentType& AttributeValue) const;

	/** Set the skeletal mesh import content type. This content type determines whether the factory imports partial or full translated content. Return false if the attribute could not be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomImportContentType(const EInterchangeSkeletalMeshContentType& AttributeValue);

	/** Query the skeletal mesh UseHighPrecisionSkinWeights setting. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomUseHighPrecisionSkinWeights(bool& AttributeValue) const;

	/** Set the skeletal mesh UseHighPrecisionSkinWeights setting. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomUseHighPrecisionSkinWeights(const bool& AttributeValue, bool bAddApplyDelegate = true);
		
	/** Query the skeletal mesh threshold value that is used to decide whether two vertex positions are equal. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomThresholdPosition(float& AttributeValue) const;

	/** Set the skeletal mesh threshold value that is used to decide whether two vertex positions are equal. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomThresholdPosition(const float& AttributeValue, bool bAddApplyDelegate = true);

	/** Query the skeletal mesh threshold value that is used to decide whether two normals, tangents, or bi-normals are equal. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomThresholdTangentNormal(float& AttributeValue) const;

	/** Set the skeletal mesh threshold value that is used to decide whether two normals, tangents, or bi-normals are equal. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomThresholdTangentNormal(const float& AttributeValue, bool bAddApplyDelegate = true);

	/** Query the skeletal mesh threshold value that is used to decide whether two UVs are equal. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomThresholdUV(float& AttributeValue) const;

	/** Set the skeletal mesh threshold value that is used to decide whether two UVs are equal. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomThresholdUV(const float& AttributeValue, bool bAddApplyDelegate = true);

	/** Query the skeletal mesh threshold value that is used to compare vertex position equality when computing morph target deltas. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomMorphThresholdPosition(float& AttributeValue) const;

	/** Set the skeletal mesh threshold value that is used to compare vertex position equality when computing morph target deltas. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomMorphThresholdPosition(const float& AttributeValue, bool bAddApplyDelegate = true);

	/**
	 * Query the maximum number of bone influences to allow each vertex in this mesh to use.
	 * If set higher than the limit determined by the project settings, it has no effect.
	 * If set to 0, the value is taken from the DefaultBoneInfluenceLimit project setting.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomBoneInfluenceLimit(int32& AttributeValue) const;

	/**
	 * Set the maximum number of bone influences to allow each vertex in this mesh to use.
	 * If set higher than the limit determined by the project settings, it has no effect.
	 * If set to 0, the value is taken from the DefaultBoneInfluenceLimit project setting.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomBoneInfluenceLimit(const int32& AttributeValue, bool bAddApplyDelegate = true);

	/**
	 * The skeletal mesh thumbnail can have an overlay if the last reimport was geometry only. This thumbnail overlay feature uses the metadata to find out if the last import was geometry only.
	 */
	virtual void AppendAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	virtual void AppendAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	virtual void CopyWithObject(const UInterchangeFactoryBaseNode* SourceNode, UObject* Object) override;
private:

	virtual void FillAssetClassFromAttribute() override;
	virtual bool SetNodeClassFromClassAttribute() override;

	const UE::Interchange::FAttributeKey ClassNameAttributeKey = UE::Interchange::FBaseNodeStaticData::ClassTypeAttributeKey();
	const UE::Interchange::FAttributeKey Macro_CustomImportMorphTargetKey = UE::Interchange::FAttributeKey(TEXT("ImportMorphTarget"));
	const UE::Interchange::FAttributeKey Macro_CustomImportVertexAttributesKey = UE::Interchange::FAttributeKey(TEXT("ImportVertexAttributes"));
	const UE::Interchange::FAttributeKey Macro_CustomSkeletonSoftObjectPathKey = UE::Interchange::FAttributeKey(TEXT("SkeletonSoftObjectPath"));
	const UE::Interchange::FAttributeKey Macro_CustomCreatePhysicsAssetKey = UE::Interchange::FAttributeKey(TEXT("CreatePhysicsAsset"));
	const UE::Interchange::FAttributeKey Macro_CustomPhysicAssetSoftObjectPathKey = UE::Interchange::FAttributeKey(TEXT("PhysicAssetSoftObjectPath"));
	const UE::Interchange::FAttributeKey Macro_CustomImportContentTypeKey = UE::Interchange::FAttributeKey(TEXT("ImportContentType"));
	const UE::Interchange::FAttributeKey Macro_CustomUseHighPrecisionSkinWeightsKey = UE::Interchange::FAttributeKey(TEXT("UseHighPrecisionSkinWeights"));
	const UE::Interchange::FAttributeKey Macro_CustomThresholdPositionKey = UE::Interchange::FAttributeKey(TEXT("ThresholdPosition"));
	const UE::Interchange::FAttributeKey Macro_CustomThresholdTangentNormalKey = UE::Interchange::FAttributeKey(TEXT("ThresholdTangentNormal"));
	const UE::Interchange::FAttributeKey Macro_CustomThresholdUVKey = UE::Interchange::FAttributeKey(TEXT("ThresholdUV"));
	const UE::Interchange::FAttributeKey Macro_CustomMorphThresholdPositionKey = UE::Interchange::FAttributeKey(TEXT("MorphThresholdPosition"));
	const UE::Interchange::FAttributeKey Macro_CustomBoneInfluenceLimitKey = UE::Interchange::FAttributeKey(TEXT("BoneInfluenceLimit"));

	bool ApplyCustomUseHighPrecisionSkinWeightsToAsset(UObject* Asset) const;
	bool FillCustomUseHighPrecisionSkinWeightsFromAsset(UObject* Asset);
	bool ApplyCustomThresholdPositionToAsset(UObject* Asset) const;
	bool FillCustomThresholdPositionFromAsset(UObject* Asset);
	bool ApplyCustomThresholdTangentNormalToAsset(UObject* Asset) const;
	bool FillCustomThresholdTangentNormalFromAsset(UObject* Asset);
	bool ApplyCustomThresholdUVToAsset(UObject* Asset) const;
	bool FillCustomThresholdUVFromAsset(UObject* Asset);
	bool ApplyCustomMorphThresholdPositionToAsset(UObject* Asset) const;
	bool FillCustomMorphThresholdPositionFromAsset(UObject* Asset);
	bool ApplyCustomBoneInfluenceLimitToAsset(UObject* Asset) const;
	bool FillCustomBoneInfluenceLimitFromAsset(UObject* Asset);
protected:
	TSubclassOf<USkeletalMesh> AssetClass = nullptr;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#include "InterchangeMeshFactoryNode.generated.h"


namespace UE::Interchange
{
	struct INTERCHANGEFACTORYNODES_API FMeshFactoryNodeStaticData : public FBaseNodeStaticData
	{
		static const FAttributeKey& GetLodDependenciesBaseKey();
		static const FAttributeKey& GetSlotMaterialDependencyBaseKey();
	};
} // namespace Interchange


UCLASS(BlueprintType, Experimental, Abstract)
class INTERCHANGEFACTORYNODES_API UInterchangeMeshFactoryNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	UInterchangeMeshFactoryNode();

	/**
	 * Override serialize to restore SlotMaterialDependencies on load.
	 */
	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		if (Ar.IsLoading() && bIsInitialized)
		{
			SlotMaterialDependencies.RebuildCache();
#if WITH_ENGINE
			// Make sure the class is properly set when we compile with engine, this will set the bIsNodeClassInitialized to true.
			SetNodeClassFromClassAttribute();
#endif
		}
	}

	virtual FString GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;

public:
	/** Return The number of LOD this static mesh has.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	int32 GetLodDataCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	void GetLodDataUniqueIds(TArray<FString>& OutLodDataUniqueIds) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool AddLodDataUniqueId(const FString& LodDataUniqueId);

	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool RemoveLodDataUniqueId(const FString& LodDataUniqueId);

	/** Query whether the static mesh factory should replace the vertex color. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool GetCustomVertexColorReplace(bool& AttributeValue) const;

	/** Set whether the static mesh factory should replace the vertex color. Return false if the attribute cannot be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool SetCustomVertexColorReplace(const bool& AttributeValue);

	/** Query whether the static mesh factory should ignore the vertex color. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool GetCustomVertexColorIgnore(bool& AttributeValue) const;

	/** Set whether the static mesh factory should ignore the vertex color. Return false if the attribute cannot be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool SetCustomVertexColorIgnore(const bool& AttributeValue);

	/** Query whether the static mesh factory should override the vertex color. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool GetCustomVertexColorOverride(FColor& AttributeValue) const;

	/** Set whether the static mesh factory should override the vertex color. Return false if the attribute cannot be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool SetCustomVertexColorOverride(const FColor& AttributeValue);

	/** Allow to retrieve the correspondence table between slot names and assigned materials for this object. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	void GetSlotMaterialDependencies(TMap<FString, FString>& OutMaterialDependencies) const;

	/** Allow to retrieve one Material dependency for a given slot of this object. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool GetSlotMaterialDependencyUid(const FString& SlotName, FString& OutMaterialDependency) const;

	/** Add one Material dependency to a specific slot name of this object. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool SetSlotMaterialDependencyUid(const FString& SlotName, const FString& MaterialDependencyUid);

	/** Remove the Material dependency associated with the given slot name from this object. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool RemoveSlotMaterialDependencyUid(const FString& SlotName);

	/** Query whether normals in the imported mesh are ignored and recomputed. When normals are recomputed the tangents are also recomputed. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool GetCustomRecomputeNormals(bool& AttributeValue) const;

	/** Set whether normals in the imported mesh are ignored and recomputed. When normals are recomputed the tangents are also recomputed. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool SetCustomRecomputeNormals(const bool& AttributeValue, bool bAddApplyDelegate = true);
	
	/** Query whether tangents in the imported mesh are ignored and recomputed. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool GetCustomRecomputeTangents(bool& AttributeValue) const;

	/** Set whether tangents in the imported mesh are ignored and recomputed. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool SetCustomRecomputeTangents(const bool& AttributeValue, bool bAddApplyDelegate = true);

	/** Query whether tangents recompute will use mikkt space if tangents are recomputed. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool GetCustomUseMikkTSpace(bool& AttributeValue) const;

	/** Set whether tangents recompute will use mikkt space if tangents are recomputed. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool SetCustomUseMikkTSpace(const bool& AttributeValue, bool bAddApplyDelegate = true);

	/** Query whether normals recompute will use the surface area and the corner angle of the triangle as a ratio when computing the normals. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool GetCustomComputeWeightedNormals(bool& AttributeValue) const;

	/** Set whether normals recompute will use the surface area and the corner angle of the triangle as a ratio when computing the normals. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool SetCustomComputeWeightedNormals(const bool& AttributeValue, bool bAddApplyDelegate = true);

	/** Query whether Tangents will be stored at 16 bit vs 8 bit precision. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool GetCustomUseHighPrecisionTangentBasis(bool& AttributeValue) const;

	/** Set whether Tangents will be stored at 16 bit vs 8 bit precision. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool SetCustomUseHighPrecisionTangentBasis(const bool& AttributeValue, bool bAddApplyDelegate = true);

	/** Query whether UVs will be stored at full floating point precision. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool GetCustomUseFullPrecisionUVs(bool& AttributeValue) const;

	/** Set whether UVs will be stored at full floating point precision. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool SetCustomUseFullPrecisionUVs(const bool& AttributeValue, bool bAddApplyDelegate = true);

	/** Query whether UVs will be stored at full floating point precision. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool GetCustomUseBackwardsCompatibleF16TruncUVs(bool& AttributeValue) const;

	/** Set whether UVs will be stored at full floating point precision. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool SetCustomUseBackwardsCompatibleF16TruncUVs(const bool& AttributeValue, bool bAddApplyDelegate = true);

	/** Query whether degenerate triangles will be removed. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool GetCustomRemoveDegenerates(bool& AttributeValue) const;

	/** Set whether degenerate triangles will be removed. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | FactoryNode | Mesh")
	bool SetCustomRemoveDegenerates(const bool& AttributeValue, bool bAddApplyDelegate = true);

private:

	const UE::Interchange::FAttributeKey Macro_CustomVertexColorReplaceKey = UE::Interchange::FAttributeKey(TEXT("VertexColorReplace"));
	const UE::Interchange::FAttributeKey Macro_CustomVertexColorIgnoreKey = UE::Interchange::FAttributeKey(TEXT("VertexColorIgnore"));
	const UE::Interchange::FAttributeKey Macro_CustomVertexColorOverrideKey = UE::Interchange::FAttributeKey(TEXT("VertexColorOverride"));
	const UE::Interchange::FAttributeKey Macro_CustomRecomputeNormalsKey = UE::Interchange::FAttributeKey(TEXT("RecomputeNormals"));
	const UE::Interchange::FAttributeKey Macro_CustomRecomputeTangentsKey = UE::Interchange::FAttributeKey(TEXT("RecomputeTangents"));
	const UE::Interchange::FAttributeKey Macro_CustomUseMikkTSpaceKey = UE::Interchange::FAttributeKey(TEXT("UseMikkTSpace"));
	const UE::Interchange::FAttributeKey Macro_CustomComputeWeightedNormalsKey = UE::Interchange::FAttributeKey(TEXT("ComputeWeightedNormals"));
	const UE::Interchange::FAttributeKey Macro_CustomUseHighPrecisionTangentBasisKey = UE::Interchange::FAttributeKey(TEXT("UseHighPrecisionTangentBasis"));
	const UE::Interchange::FAttributeKey Macro_CustomUseFullPrecisionUVsKey = UE::Interchange::FAttributeKey(TEXT("UseFullPrecisionUVs"));
	const UE::Interchange::FAttributeKey Macro_CustomUseBackwardsCompatibleF16TruncUVsKey = UE::Interchange::FAttributeKey(TEXT("UseBackwardsCompatibleF16TruncUVs"));
	const UE::Interchange::FAttributeKey Macro_CustomRemoveDegeneratesKey = UE::Interchange::FAttributeKey(TEXT("RemoveDegenerates"));

	UE::Interchange::TArrayAttributeHelper<FString> LodDependencies;
	UE::Interchange::TMapAttributeHelper<FString, FString> SlotMaterialDependencies;

protected:
	virtual void FillAssetClassFromAttribute() PURE_VIRTUAL("FillAssetClassFromAttribute");
	virtual bool SetNodeClassFromClassAttribute() PURE_VIRTUAL("SetNodeClassFromClassAttribute", return false;);

	const UE::Interchange::FAttributeKey ClassNameAttributeKey = UE::Interchange::FBaseNodeStaticData::ClassTypeAttributeKey();

	bool ApplyCustomRecomputeNormalsToAsset(UObject * Asset) const;
	bool FillCustomRecomputeNormalsFromAsset(UObject * Asset);
	bool ApplyCustomRecomputeTangentsToAsset(UObject * Asset) const;
	bool FillCustomRecomputeTangentsFromAsset(UObject * Asset);
	bool ApplyCustomUseMikkTSpaceToAsset(UObject * Asset) const;
	bool FillCustomUseMikkTSpaceFromAsset(UObject * Asset);
	bool ApplyCustomComputeWeightedNormalsToAsset(UObject * Asset) const;
	bool FillCustomComputeWeightedNormalsFromAsset(UObject * Asset);
	bool ApplyCustomUseHighPrecisionTangentBasisToAsset(UObject * Asset) const;
	bool FillCustomUseHighPrecisionTangentBasisFromAsset(UObject * Asset);
	bool ApplyCustomUseFullPrecisionUVsToAsset(UObject * Asset) const;
	bool FillCustomUseFullPrecisionUVsFromAsset(UObject * Asset);
	bool ApplyCustomUseBackwardsCompatibleF16TruncUVsToAsset(UObject * Asset) const;
	bool FillCustomUseBackwardsCompatibleF16TruncUVsFromAsset(UObject * Asset);
	bool ApplyCustomRemoveDegeneratesToAsset(UObject * Asset) const;
	bool FillCustomRemoveDegeneratesFromAsset(UObject * Asset);

	bool bIsNodeClassInitialized = false;
};

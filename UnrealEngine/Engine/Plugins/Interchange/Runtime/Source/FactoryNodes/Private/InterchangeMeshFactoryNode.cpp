// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeMeshFactoryNode.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeMeshFactoryNode)

//The static mesh source model which contain the build settings are editor only
#if WITH_EDITOR

#define IMPLEMENT_MESH_BUILD_VALUE_TO_ASSET(AttributeName, AttributeType, PropertyName)	\
AttributeType ValueData;															\
if (GetAttribute<AttributeType>(Macro_Custom##AttributeName##Key.Key, ValueData))	\
{																					\
	if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset))					\
	{																				\
		if (FSkeletalMeshLODInfo* LodInfo = SkeletalMesh->GetLODInfo(0))			\
		{																			\
			LodInfo->BuildSettings.PropertyName = ValueData;						\
			return true;															\
		}																			\
	}																				\
	else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))					\
	{																				\
		if (StaticMesh->GetNumSourceModels() > 0)									\
		{																			\
			StaticMesh->GetSourceModel(0).BuildSettings.PropertyName = ValueData;	\
			return true;															\
		}																			\
	}																				\
}																					\
return false;

#define IMPLEMENT_MESH_BUILD_ASSET_TO_VALUE(AttributeName, PropertyName)	\
if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))								\
{																					\
	if (StaticMesh->GetNumSourceModels() > 0)										\
	{																				\
		return SetAttribute(Macro_Custom##AttributeName##Key.Key, StaticMesh->GetSourceModel(0).BuildSettings.PropertyName);	\
	}																				\
}																					\
else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset))					\
{																					\
	if (FSkeletalMeshLODInfo* LodInfo = SkeletalMesh->GetLODInfo(0))				\
	{																				\
		return SetAttribute(Macro_Custom##AttributeName##Key.Key, LodInfo->BuildSettings.PropertyName);	\
	}																				\
}																					\
return false;

#else //WITH_EDITOR

#define IMPLEMENT_MESH_BUILD_VALUE_TO_ASSET(AttributeName, AttributeType, PropertyName)	\
return false;

#define IMPLEMENT_MESH_BUILD_ASSET_TO_VALUE(AttributeName, PropertyName)	\
return false;
#endif // else WITH_EDITOR



namespace UE::Interchange
{
	const FAttributeKey& FMeshFactoryNodeStaticData::GetLodDependenciesBaseKey()
	{
		static FAttributeKey LodDependencies_BaseKey = FAttributeKey(TEXT("Lod_Dependencies"));
		return LodDependencies_BaseKey;
	}

	const FAttributeKey& FMeshFactoryNodeStaticData::GetSlotMaterialDependencyBaseKey()
	{
		static FAttributeKey SlotMaterialDependency_BaseKey(TEXT("__SlotMaterialDependency__"));
		return SlotMaterialDependency_BaseKey;
	}

} // namespace UE::Interchange


UInterchangeMeshFactoryNode::UInterchangeMeshFactoryNode()
{
	LodDependencies.Initialize(Attributes, UE::Interchange::FMeshFactoryNodeStaticData::GetLodDependenciesBaseKey().ToString());
	SlotMaterialDependencies.Initialize(Attributes.ToSharedRef(), UE::Interchange::FMeshFactoryNodeStaticData::GetSlotMaterialDependencyBaseKey().ToString());
}

FString UInterchangeMeshFactoryNode::GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
{
	FString KeyDisplayName = NodeAttributeKey.ToString();
	FString NodeAttributeKeyString = NodeAttributeKey.ToString();
	if (NodeAttributeKey == UE::Interchange::FMeshFactoryNodeStaticData::GetLodDependenciesBaseKey())
	{
		KeyDisplayName = TEXT("LOD Dependencies Count");
		return KeyDisplayName;
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FMeshFactoryNodeStaticData::GetLodDependenciesBaseKey().ToString()))
	{
		KeyDisplayName = TEXT("LOD Dependencies Index ");
		const FString IndexKey = UE::Interchange::TArrayAttributeHelper<FString>::IndexKey();
		int32 IndexPosition = NodeAttributeKeyString.Find(IndexKey) + IndexKey.Len();
		if (IndexPosition < NodeAttributeKeyString.Len())
		{
			KeyDisplayName += NodeAttributeKeyString.RightChop(IndexPosition);
		}
		return KeyDisplayName;
	}
	return Super::GetKeyDisplayName(NodeAttributeKey);
}

int32 UInterchangeMeshFactoryNode::GetLodDataCount() const
{
	return LodDependencies.GetCount();
}

void UInterchangeMeshFactoryNode::GetLodDataUniqueIds(TArray<FString>& OutLodDataUniqueIds) const
{
	LodDependencies.GetItems(OutLodDataUniqueIds);
}

bool UInterchangeMeshFactoryNode::AddLodDataUniqueId(const FString& LodDataUniqueId)
{
	return LodDependencies.AddItem(LodDataUniqueId);
}

bool UInterchangeMeshFactoryNode::RemoveLodDataUniqueId(const FString& LodDataUniqueId)
{
	return LodDependencies.RemoveItem(LodDataUniqueId);
}

bool UInterchangeMeshFactoryNode::GetCustomVertexColorReplace(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(VertexColorReplace, bool)
}

bool UInterchangeMeshFactoryNode::SetCustomVertexColorReplace(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(VertexColorReplace, bool)
}

bool UInterchangeMeshFactoryNode::GetCustomVertexColorIgnore(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(VertexColorIgnore, bool)
}

bool UInterchangeMeshFactoryNode::SetCustomVertexColorIgnore(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(VertexColorIgnore, bool)
}

bool UInterchangeMeshFactoryNode::GetCustomVertexColorOverride(FColor& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(VertexColorOverride, FColor)
}

bool UInterchangeMeshFactoryNode::SetCustomVertexColorOverride(const FColor& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(VertexColorOverride, FColor)
}

void UInterchangeMeshFactoryNode::GetSlotMaterialDependencies(TMap<FString, FString>& OutMaterialDependencies) const
{
	OutMaterialDependencies = SlotMaterialDependencies.ToMap();
}

bool UInterchangeMeshFactoryNode::GetSlotMaterialDependencyUid(const FString& SlotName, FString& OutMaterialDependency) const
{
	return SlotMaterialDependencies.GetValue(SlotName, OutMaterialDependency);
}

bool UInterchangeMeshFactoryNode::SetSlotMaterialDependencyUid(const FString& SlotName, const FString& MaterialDependencyUid)
{
	return SlotMaterialDependencies.SetKeyValue(SlotName, MaterialDependencyUid);
}

bool UInterchangeMeshFactoryNode::RemoveSlotMaterialDependencyUid(const FString& SlotName)
{
	return SlotMaterialDependencies.RemoveKey(SlotName);
}

bool UInterchangeMeshFactoryNode::GetCustomRecomputeNormals(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(RecomputeNormals, bool)
}

bool UInterchangeMeshFactoryNode::SetCustomRecomputeNormals(const bool& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeMeshFactoryNode, RecomputeNormals, bool);
}

bool UInterchangeMeshFactoryNode::ApplyCustomRecomputeNormalsToAsset(UObject* Asset) const
{
	IMPLEMENT_MESH_BUILD_VALUE_TO_ASSET(RecomputeNormals, bool, bRecomputeNormals);
}

bool UInterchangeMeshFactoryNode::FillCustomRecomputeNormalsFromAsset(UObject* Asset)
{
	IMPLEMENT_MESH_BUILD_ASSET_TO_VALUE(RecomputeNormals, bRecomputeNormals);
}

bool UInterchangeMeshFactoryNode::GetCustomRecomputeTangents(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(RecomputeTangents, bool)
}

bool UInterchangeMeshFactoryNode::SetCustomRecomputeTangents(const bool& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeMeshFactoryNode, RecomputeTangents, bool);
}

bool UInterchangeMeshFactoryNode::ApplyCustomRecomputeTangentsToAsset(UObject* Asset) const
{
	IMPLEMENT_MESH_BUILD_VALUE_TO_ASSET(RecomputeTangents, bool, bRecomputeTangents);
}

bool UInterchangeMeshFactoryNode::FillCustomRecomputeTangentsFromAsset(UObject* Asset)
{
	IMPLEMENT_MESH_BUILD_ASSET_TO_VALUE(RecomputeTangents, bRecomputeTangents);
}

bool UInterchangeMeshFactoryNode::GetCustomUseMikkTSpace(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(UseMikkTSpace, bool)
}

bool UInterchangeMeshFactoryNode::SetCustomUseMikkTSpace(const bool& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeMeshFactoryNode, UseMikkTSpace, bool);
}

bool UInterchangeMeshFactoryNode::ApplyCustomUseMikkTSpaceToAsset(UObject* Asset) const
{
	IMPLEMENT_MESH_BUILD_VALUE_TO_ASSET(UseMikkTSpace, bool, bUseMikkTSpace);
}

bool UInterchangeMeshFactoryNode::FillCustomUseMikkTSpaceFromAsset(UObject* Asset)
{
	IMPLEMENT_MESH_BUILD_ASSET_TO_VALUE(UseMikkTSpace, bUseMikkTSpace);
}

bool UInterchangeMeshFactoryNode::GetCustomComputeWeightedNormals(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ComputeWeightedNormals, bool)
}

bool UInterchangeMeshFactoryNode::SetCustomComputeWeightedNormals(const bool& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeMeshFactoryNode, ComputeWeightedNormals, bool);
}

bool UInterchangeMeshFactoryNode::ApplyCustomComputeWeightedNormalsToAsset(UObject* Asset) const
{
	IMPLEMENT_MESH_BUILD_VALUE_TO_ASSET(ComputeWeightedNormals, bool, bComputeWeightedNormals);
}

bool UInterchangeMeshFactoryNode::FillCustomComputeWeightedNormalsFromAsset(UObject* Asset)
{
	IMPLEMENT_MESH_BUILD_ASSET_TO_VALUE(ComputeWeightedNormals, bComputeWeightedNormals);
}

bool UInterchangeMeshFactoryNode::GetCustomUseHighPrecisionTangentBasis(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(UseHighPrecisionTangentBasis, bool)
}

bool UInterchangeMeshFactoryNode::SetCustomUseHighPrecisionTangentBasis(const bool& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeMeshFactoryNode, UseHighPrecisionTangentBasis, bool);
}

bool UInterchangeMeshFactoryNode::ApplyCustomUseHighPrecisionTangentBasisToAsset(UObject* Asset) const
{
	IMPLEMENT_MESH_BUILD_VALUE_TO_ASSET(UseHighPrecisionTangentBasis, bool, bUseHighPrecisionTangentBasis);
}

bool UInterchangeMeshFactoryNode::FillCustomUseHighPrecisionTangentBasisFromAsset(UObject* Asset)
{
	IMPLEMENT_MESH_BUILD_ASSET_TO_VALUE(UseHighPrecisionTangentBasis, bUseHighPrecisionTangentBasis);
}

bool UInterchangeMeshFactoryNode::GetCustomUseFullPrecisionUVs(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(UseFullPrecisionUVs, bool)
}

bool UInterchangeMeshFactoryNode::SetCustomUseFullPrecisionUVs(const bool& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeMeshFactoryNode, UseFullPrecisionUVs, bool);
}

bool UInterchangeMeshFactoryNode::ApplyCustomUseFullPrecisionUVsToAsset(UObject* Asset) const
{
	IMPLEMENT_MESH_BUILD_VALUE_TO_ASSET(UseFullPrecisionUVs, bool, bUseFullPrecisionUVs);
}

bool UInterchangeMeshFactoryNode::FillCustomUseFullPrecisionUVsFromAsset(UObject* Asset)
{
	IMPLEMENT_MESH_BUILD_ASSET_TO_VALUE(UseFullPrecisionUVs, bUseFullPrecisionUVs);
}

bool UInterchangeMeshFactoryNode::GetCustomUseBackwardsCompatibleF16TruncUVs(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(UseBackwardsCompatibleF16TruncUVs, bool)
}

bool UInterchangeMeshFactoryNode::SetCustomUseBackwardsCompatibleF16TruncUVs(const bool& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeMeshFactoryNode, UseBackwardsCompatibleF16TruncUVs, bool);
}

bool UInterchangeMeshFactoryNode::ApplyCustomUseBackwardsCompatibleF16TruncUVsToAsset(UObject* Asset) const
{
	IMPLEMENT_MESH_BUILD_VALUE_TO_ASSET(UseBackwardsCompatibleF16TruncUVs, bool, bUseBackwardsCompatibleF16TruncUVs);
}
bool UInterchangeMeshFactoryNode::FillCustomUseBackwardsCompatibleF16TruncUVsFromAsset(UObject* Asset)
{
	IMPLEMENT_MESH_BUILD_ASSET_TO_VALUE(UseBackwardsCompatibleF16TruncUVs, bUseBackwardsCompatibleF16TruncUVs);
}

bool UInterchangeMeshFactoryNode::GetCustomRemoveDegenerates(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(RemoveDegenerates, bool)
}

bool UInterchangeMeshFactoryNode::SetCustomRemoveDegenerates(const bool& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeMeshFactoryNode, RemoveDegenerates, bool);
}

bool UInterchangeMeshFactoryNode::ApplyCustomRemoveDegeneratesToAsset(UObject* Asset) const
{
	IMPLEMENT_MESH_BUILD_VALUE_TO_ASSET(RemoveDegenerates, bool, bRemoveDegenerates);
}

bool UInterchangeMeshFactoryNode::FillCustomRemoveDegeneratesFromAsset(UObject* Asset)
{
	IMPLEMENT_MESH_BUILD_ASSET_TO_VALUE(RemoveDegenerates, bRemoveDegenerates);
}
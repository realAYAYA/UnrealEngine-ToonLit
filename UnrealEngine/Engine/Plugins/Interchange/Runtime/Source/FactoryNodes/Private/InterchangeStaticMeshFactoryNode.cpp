// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeStaticMeshFactoryNode.h"

#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeStaticMeshFactoryNode)

#if WITH_EDITOR

#define IMPLEMENT_STATICMESH_BUILD_VALUE_TO_ASSET(AttributeName, AttributeType, PropertyName)	\
AttributeType ValueData;															\
if (GetAttribute<AttributeType>(Macro_Custom##AttributeName##Key.Key, ValueData))	\
{																					\
	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))							\
	{																				\
		if (StaticMesh->GetNumSourceModels() > 0)									\
		{																			\
			StaticMesh->GetSourceModel(0).BuildSettings.PropertyName = ValueData;	\
			return true;															\
		}																			\
	}																				\
}																					\
return false;

#define IMPLEMENT_STATICMESH_BUILD_ASSET_TO_VALUE(AttributeName, PropertyName)	\
if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))								\
{																					\
	if (StaticMesh->GetNumSourceModels() > 0)										\
	{																				\
		return SetAttribute(Macro_Custom##AttributeName##Key.Key, StaticMesh->GetSourceModel(0).BuildSettings.PropertyName);	\
	}																				\
}																					\
return false;

#else //WITH_EDITOR

#define IMPLEMENT_STATICMESH_BUILD_VALUE_TO_ASSET(AttributeName, AttributeType, PropertyName)	\
return false;

#define IMPLEMENT_STATICMESH_BUILD_ASSET_TO_VALUE(AttributeName, PropertyName)	\
return false;

#endif // else WITH_EDITOR

namespace UE
{
	namespace Interchange
	{
		const FAttributeKey& FStaticMeshNodeStaticData::GetSocketUidsBaseKey()
		{
			static FAttributeKey SocketUids_BaseKey = FAttributeKey(TEXT("SocketUids"));
			return SocketUids_BaseKey;
		}
	} // namespace Interchange
} // namespace UE


UInterchangeStaticMeshFactoryNode::UInterchangeStaticMeshFactoryNode()
{
#if WITH_ENGINE
	AssetClass = nullptr;
#endif
	SocketUids.Initialize(Attributes, UE::Interchange::FStaticMeshNodeStaticData::GetSocketUidsBaseKey().ToString());
}

void UInterchangeStaticMeshFactoryNode::InitializeStaticMeshNode(const FString& UniqueID, const FString& DisplayLabel, const FString& InAssetClass)
{
	bIsNodeClassInitialized = false;
	InitializeNode(UniqueID, DisplayLabel, EInterchangeNodeContainerType::FactoryData);

	FString OperationName = GetTypeName() + TEXT(".SetAssetClassName");
	InterchangePrivateNodeBase::SetCustomAttribute<FString>(*Attributes, ClassNameAttributeKey, OperationName, InAssetClass);
	FillAssetClassFromAttribute();
}

FString UInterchangeStaticMeshFactoryNode::GetTypeName() const
{
	const FString TypeName = TEXT("StaticMeshNode");
	return TypeName;
}

UClass* UInterchangeStaticMeshFactoryNode::GetObjectClass() const
{
	ensure(bIsNodeClassInitialized);
#if WITH_ENGINE
	return AssetClass.Get() != nullptr ? AssetClass.Get() : UStaticMesh::StaticClass();
#else
	return nullptr;
#endif
}

bool UInterchangeStaticMeshFactoryNode::GetCustomBuildNanite(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(BuildNanite, bool)
}

bool UInterchangeStaticMeshFactoryNode::SetCustomBuildNanite(const bool& AttributeValue, bool bAddApplyDelegate /*= true*/)
{
#if WITH_EDITORONLY_DATA
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeStaticMeshFactoryNode, BuildNanite, bool);
#else
	return false;
#endif
}

int32 UInterchangeStaticMeshFactoryNode::GetSocketUidCount() const
{
	return SocketUids.GetCount();
}

void UInterchangeStaticMeshFactoryNode::GetSocketUids(TArray<FString>& OutSocketUids) const
{
	SocketUids.GetItems(OutSocketUids);
}

bool UInterchangeStaticMeshFactoryNode::AddSocketUid(const FString& SocketUid)
{
	return SocketUids.AddItem(SocketUid);
}

bool UInterchangeStaticMeshFactoryNode::AddSocketUids(const TArray<FString>& InSocketUids)
{
	for (const FString& SocketUid : InSocketUids)
	{
		if (!SocketUids.AddItem(SocketUid))
		{
			return false;
		}
	}

	return true;
}

bool UInterchangeStaticMeshFactoryNode::RemoveSocketUd(const FString& SocketUid)
{
	return SocketUids.RemoveItem(SocketUid);
}

void UInterchangeStaticMeshFactoryNode::FillAssetClassFromAttribute()
{
#if WITH_ENGINE
	FString OperationName = GetTypeName() + TEXT(".GetAssetClassName");
	FString ClassName;
	InterchangePrivateNodeBase::GetCustomAttribute<FString>(*Attributes, ClassNameAttributeKey, OperationName, ClassName);
	if (ClassName.Equals(UStaticMesh::StaticClass()->GetName()))
	{
		AssetClass = UStaticMesh::StaticClass();
		bIsNodeClassInitialized = true;
	}
#endif
}

bool UInterchangeStaticMeshFactoryNode::SetNodeClassFromClassAttribute()
{
	if (!bIsNodeClassInitialized)
	{
		FillAssetClassFromAttribute();
	}
	return bIsNodeClassInitialized;
}

bool UInterchangeStaticMeshFactoryNode::GetCustomBuildReversedIndexBuffer(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(BuildReversedIndexBuffer, bool)
}
bool UInterchangeStaticMeshFactoryNode::SetCustomBuildReversedIndexBuffer(const bool& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeStaticMeshFactoryNode, BuildReversedIndexBuffer, bool);
}
bool UInterchangeStaticMeshFactoryNode::ApplyCustomBuildReversedIndexBufferToAsset(UObject* Asset) const
{
	IMPLEMENT_STATICMESH_BUILD_VALUE_TO_ASSET(BuildReversedIndexBuffer, bool, bBuildReversedIndexBuffer);
}
bool UInterchangeStaticMeshFactoryNode::FillCustomBuildReversedIndexBufferFromAsset(UObject* Asset)
{
	IMPLEMENT_STATICMESH_BUILD_ASSET_TO_VALUE(BuildReversedIndexBuffer, bBuildReversedIndexBuffer);
}

bool UInterchangeStaticMeshFactoryNode::GetCustomGenerateLightmapUVs(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(GenerateLightmapUVs, bool)
}
bool UInterchangeStaticMeshFactoryNode::SetCustomGenerateLightmapUVs(const bool& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeStaticMeshFactoryNode, GenerateLightmapUVs, bool);
}
bool UInterchangeStaticMeshFactoryNode::ApplyCustomGenerateLightmapUVsToAsset(UObject* Asset) const
{
	IMPLEMENT_STATICMESH_BUILD_VALUE_TO_ASSET(GenerateLightmapUVs, bool, bGenerateLightmapUVs);
}
bool UInterchangeStaticMeshFactoryNode::FillCustomGenerateLightmapUVsFromAsset(UObject* Asset)
{
	IMPLEMENT_STATICMESH_BUILD_ASSET_TO_VALUE(GenerateLightmapUVs, bGenerateLightmapUVs);
}

bool UInterchangeStaticMeshFactoryNode::GetCustomGenerateDistanceFieldAsIfTwoSided(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(GenerateDistanceFieldAsIfTwoSided, bool)
}
bool UInterchangeStaticMeshFactoryNode::SetCustomGenerateDistanceFieldAsIfTwoSided(const bool& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeStaticMeshFactoryNode, GenerateDistanceFieldAsIfTwoSided, bool);
}
bool UInterchangeStaticMeshFactoryNode::ApplyCustomGenerateDistanceFieldAsIfTwoSidedToAsset(UObject* Asset) const
{
	IMPLEMENT_STATICMESH_BUILD_VALUE_TO_ASSET(GenerateDistanceFieldAsIfTwoSided, bool, bGenerateDistanceFieldAsIfTwoSided);

}
bool UInterchangeStaticMeshFactoryNode::FillCustomGenerateDistanceFieldAsIfTwoSidedFromAsset(UObject* Asset)
{
	IMPLEMENT_STATICMESH_BUILD_ASSET_TO_VALUE(GenerateDistanceFieldAsIfTwoSided, bGenerateDistanceFieldAsIfTwoSided);
}

bool UInterchangeStaticMeshFactoryNode::GetCustomSupportFaceRemap(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(SupportFaceRemap, bool)
}
bool UInterchangeStaticMeshFactoryNode::SetCustomSupportFaceRemap(const bool& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeStaticMeshFactoryNode, SupportFaceRemap, bool);
}
bool UInterchangeStaticMeshFactoryNode::ApplyCustomSupportFaceRemapToAsset(UObject* Asset) const
{
	IMPLEMENT_STATICMESH_BUILD_VALUE_TO_ASSET(SupportFaceRemap, bool, bSupportFaceRemap);
}
bool UInterchangeStaticMeshFactoryNode::FillCustomSupportFaceRemapFromAsset(UObject* Asset)
{
	IMPLEMENT_STATICMESH_BUILD_ASSET_TO_VALUE(SupportFaceRemap, bSupportFaceRemap);
}

bool UInterchangeStaticMeshFactoryNode::GetCustomMinLightmapResolution(int32& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(MinLightmapResolution, int32)
}
bool UInterchangeStaticMeshFactoryNode::SetCustomMinLightmapResolution(const int32& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeStaticMeshFactoryNode, MinLightmapResolution, int32);
}
bool UInterchangeStaticMeshFactoryNode::ApplyCustomMinLightmapResolutionToAsset(UObject* Asset) const
{
	IMPLEMENT_STATICMESH_BUILD_VALUE_TO_ASSET(MinLightmapResolution, int32, MinLightmapResolution);
}
bool UInterchangeStaticMeshFactoryNode::FillCustomMinLightmapResolutionFromAsset(UObject* Asset)
{
	IMPLEMENT_STATICMESH_BUILD_ASSET_TO_VALUE(MinLightmapResolution, MinLightmapResolution);
}

bool UInterchangeStaticMeshFactoryNode::GetCustomSrcLightmapIndex(int32& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(SrcLightmapIndex, int32)
}
bool UInterchangeStaticMeshFactoryNode::SetCustomSrcLightmapIndex(const int32& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeStaticMeshFactoryNode, SrcLightmapIndex, int32);
}
bool UInterchangeStaticMeshFactoryNode::ApplyCustomSrcLightmapIndexToAsset(UObject* Asset) const
{
	IMPLEMENT_STATICMESH_BUILD_VALUE_TO_ASSET(SrcLightmapIndex, int32, SrcLightmapIndex);
}
bool UInterchangeStaticMeshFactoryNode::FillCustomSrcLightmapIndexFromAsset(UObject* Asset)
{
	IMPLEMENT_STATICMESH_BUILD_ASSET_TO_VALUE(SrcLightmapIndex, SrcLightmapIndex);
}

bool UInterchangeStaticMeshFactoryNode::GetCustomDstLightmapIndex(int32& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(DstLightmapIndex, int32)
}
bool UInterchangeStaticMeshFactoryNode::SetCustomDstLightmapIndex(const int32& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeStaticMeshFactoryNode, DstLightmapIndex, int32);
}
bool UInterchangeStaticMeshFactoryNode::ApplyCustomDstLightmapIndexToAsset(UObject* Asset) const
{
	IMPLEMENT_STATICMESH_BUILD_VALUE_TO_ASSET(DstLightmapIndex, int32, DstLightmapIndex);
}
bool UInterchangeStaticMeshFactoryNode::FillCustomDstLightmapIndexFromAsset(UObject* Asset)
{
	IMPLEMENT_STATICMESH_BUILD_ASSET_TO_VALUE(DstLightmapIndex, DstLightmapIndex);
}

bool UInterchangeStaticMeshFactoryNode::GetCustomBuildScale3D(FVector& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(BuildScale3D, FVector)
}
bool UInterchangeStaticMeshFactoryNode::SetCustomBuildScale3D(const FVector& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeStaticMeshFactoryNode, BuildScale3D, FVector);
}
bool UInterchangeStaticMeshFactoryNode::ApplyCustomBuildScale3DToAsset(UObject* Asset) const
{
	IMPLEMENT_STATICMESH_BUILD_VALUE_TO_ASSET(BuildScale3D, FVector, BuildScale3D);
}
bool UInterchangeStaticMeshFactoryNode::FillCustomBuildScale3DFromAsset(UObject* Asset)
{
	IMPLEMENT_STATICMESH_BUILD_ASSET_TO_VALUE(BuildScale3D, BuildScale3D);
}

bool UInterchangeStaticMeshFactoryNode::GetCustomDistanceFieldResolutionScale(float& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(DistanceFieldResolutionScale, float)
}
bool UInterchangeStaticMeshFactoryNode::SetCustomDistanceFieldResolutionScale(const float& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeStaticMeshFactoryNode, DistanceFieldResolutionScale, float);
}
bool UInterchangeStaticMeshFactoryNode::ApplyCustomDistanceFieldResolutionScaleToAsset(UObject* Asset) const
{
	IMPLEMENT_STATICMESH_BUILD_VALUE_TO_ASSET(DistanceFieldResolutionScale, float, DistanceFieldResolutionScale);
}
bool UInterchangeStaticMeshFactoryNode::FillCustomDistanceFieldResolutionScaleFromAsset(UObject* Asset)
{
	IMPLEMENT_STATICMESH_BUILD_ASSET_TO_VALUE(DistanceFieldResolutionScale, DistanceFieldResolutionScale);
}

bool UInterchangeStaticMeshFactoryNode::GetCustomDistanceFieldReplacementMesh(FSoftObjectPath& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(DistanceFieldReplacementMesh, FSoftObjectPath)
}
bool UInterchangeStaticMeshFactoryNode::SetCustomDistanceFieldReplacementMesh(const FSoftObjectPath& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeStaticMeshFactoryNode, DistanceFieldReplacementMesh, FSoftObjectPath);
}
bool UInterchangeStaticMeshFactoryNode::ApplyCustomDistanceFieldReplacementMeshToAsset(UObject* Asset) const
{
#if WITH_EDITOR
	FSoftObjectPath ValueData;
	if (GetAttribute<FSoftObjectPath>(Macro_CustomDistanceFieldReplacementMeshKey.Key, ValueData))
	{
		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
		{
			if (StaticMesh->GetNumSourceModels() > 0)
			{
				StaticMesh->GetSourceModel(0).BuildSettings.DistanceFieldReplacementMesh = Cast<UStaticMesh>(ValueData.TryLoad());
				return true;
			}
		}
	}
#endif //WITH_EDITOR
	return false;
}
bool UInterchangeStaticMeshFactoryNode::FillCustomDistanceFieldReplacementMeshFromAsset(UObject* Asset)
{
#if WITH_EDITOR
	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
	{
		if (StaticMesh->GetNumSourceModels() > 0)
		{
			FSoftObjectPath SoftObjectPath(StaticMesh->GetSourceModel(0).BuildSettings.DistanceFieldReplacementMesh);
			return SetAttribute(Macro_CustomDistanceFieldReplacementMeshKey.Key, SoftObjectPath);
		}
	}
#endif //WITH_EDITOR
	return false;
}

bool UInterchangeStaticMeshFactoryNode::GetCustomMaxLumenMeshCards(int32& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(MaxLumenMeshCards, int32)
}
bool UInterchangeStaticMeshFactoryNode::SetCustomMaxLumenMeshCards(const int32& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(UInterchangeStaticMeshFactoryNode, MaxLumenMeshCards, int32);
}
bool UInterchangeStaticMeshFactoryNode::ApplyCustomMaxLumenMeshCardsToAsset(UObject* Asset) const
{
	IMPLEMENT_STATICMESH_BUILD_VALUE_TO_ASSET(MaxLumenMeshCards, int32, MaxLumenMeshCards);
}
bool UInterchangeStaticMeshFactoryNode::FillCustomMaxLumenMeshCardsFromAsset(UObject* Asset)
{
	IMPLEMENT_STATICMESH_BUILD_ASSET_TO_VALUE(MaxLumenMeshCards, MaxLumenMeshCards);
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeMeshNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeMeshNode)

//Interchange namespace
namespace UE
{
	namespace Interchange
	{

		const FAttributeKey& FMeshNodeStaticData::PayloadSourceKey()
		{
			static FAttributeKey AttributeKey(TEXT("__PayloadSourceKey__"));
			return AttributeKey;
		}

		const FAttributeKey& FMeshNodeStaticData::PayloadAnimationCurveKey()
		{
			static FAttributeKey AttributeKey(TEXT("__PayloadAnimationCurveKey__"));
			return AttributeKey;
		}

		const FAttributeKey& FMeshNodeStaticData::IsSkinnedMeshKey()
		{
			static FAttributeKey AttributeKey(TEXT("__IsSkinnedMeshKey__"));
			return AttributeKey;
		}

		const FAttributeKey& FMeshNodeStaticData::IsMorphTargetKey()
		{
			static FAttributeKey AttributeKey(TEXT("__IsMorphTargetKey__"));
			return AttributeKey;
		}

		const FAttributeKey& FMeshNodeStaticData::MorphTargetNameKey()
		{
			static FAttributeKey AttributeKey(TEXT("__MorphTargetNameKey__"));
			return AttributeKey;
		}

		const FAttributeKey& FMeshNodeStaticData::GetSkeletonDependenciesKey()
		{
			static FAttributeKey Dependencies_BaseKey(TEXT("__MeshSkeletonDependencies__"));
			return Dependencies_BaseKey;
		}

		const FAttributeKey& FMeshNodeStaticData::GetMorphTargetDependenciesKey()
		{
			static FAttributeKey Dependencies_BaseKey(TEXT("__MeshMorphTargetDependencies__"));
			return Dependencies_BaseKey;
		}

		const FAttributeKey& FMeshNodeStaticData::GetSceneInstancesUidsKey()
		{
			static FAttributeKey SceneInstanceUids_BaseKey(TEXT("__MeshSceneInstancesUids__"));
			return SceneInstanceUids_BaseKey;
		}

		const FAttributeKey& FMeshNodeStaticData::GetSlotMaterialDependenciesKey()
		{
			static FAttributeKey Dependencies_BaseKey(TEXT("__SlotMaterialDependencies__"));
			return Dependencies_BaseKey;
		}

	}//ns Interchange
}//ns UE

UInterchangeMeshNode::UInterchangeMeshNode()
{
	SkeletonDependencies.Initialize(Attributes, UE::Interchange::FMeshNodeStaticData::GetSkeletonDependenciesKey().ToString());
	MorphTargetDependencies.Initialize(Attributes, UE::Interchange::FMeshNodeStaticData::GetMorphTargetDependenciesKey().ToString());
	SceneInstancesUids.Initialize(Attributes, UE::Interchange::FMeshNodeStaticData::GetSceneInstancesUidsKey().ToString());
	SlotMaterialDependencies.Initialize(Attributes.ToSharedRef(), UE::Interchange::FMeshNodeStaticData::GetSlotMaterialDependenciesKey().ToString());
}

FString UInterchangeMeshNode::GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
{
	FString KeyDisplayName = NodeAttributeKey.ToString();
	const FString NodeAttributeKeyString = KeyDisplayName;
	if (NodeAttributeKey == UE::Interchange::FMeshNodeStaticData::PayloadSourceKey())
	{
		return KeyDisplayName = TEXT("Payload Source Key");
	}
	else if (NodeAttributeKey == UE::Interchange::FMeshNodeStaticData::PayloadAnimationCurveKey())
	{
		return KeyDisplayName = TEXT("Payload Animation Curve Key");
	}
	else if (NodeAttributeKey == UE::Interchange::FMeshNodeStaticData::IsSkinnedMeshKey())
	{
		return KeyDisplayName = TEXT("Is a Skinned Mesh");
	}
	else if (NodeAttributeKey == UE::Interchange::FMeshNodeStaticData::IsMorphTargetKey())
	{
		return KeyDisplayName = TEXT("Is a Morph Target");
	}
	else if (NodeAttributeKey == UE::Interchange::FMeshNodeStaticData::MorphTargetNameKey())
	{
		return KeyDisplayName = TEXT("Morph Target Name");
	}
	else if (NodeAttributeKey == UE::Interchange::FMeshNodeStaticData::GetSkeletonDependenciesKey())
	{
		return KeyDisplayName = TEXT("Skeleton Dependencies count");
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FMeshNodeStaticData::GetSkeletonDependenciesKey().ToString()))
	{
		KeyDisplayName = TEXT("Skeleton Dependencies Index ");
		const FString IndexKey = UE::Interchange::TArrayAttributeHelper<FString>::IndexKey();
		int32 IndexPosition = NodeAttributeKeyString.Find(IndexKey) + IndexKey.Len();
		if (IndexPosition < NodeAttributeKeyString.Len())
		{
			KeyDisplayName += NodeAttributeKeyString.RightChop(IndexPosition);
		}
		return KeyDisplayName;
	}
	else if (NodeAttributeKey == UE::Interchange::FMeshNodeStaticData::GetMorphTargetDependenciesKey())
	{
		return KeyDisplayName = TEXT("Morph Target Dependencies Count");
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FMeshNodeStaticData::GetMorphTargetDependenciesKey().ToString()))
	{
		KeyDisplayName = TEXT("Morph Target Dependencies Index ");
		const FString IndexKey = UE::Interchange::TArrayAttributeHelper<FString>::IndexKey();
		int32 IndexPosition = NodeAttributeKeyString.Find(IndexKey) + IndexKey.Len();
		if (IndexPosition < NodeAttributeKeyString.Len())
		{
			KeyDisplayName += NodeAttributeKeyString.RightChop(IndexPosition);
		}
		return KeyDisplayName;
	}
	else if (NodeAttributeKey == UE::Interchange::FMeshNodeStaticData::GetSceneInstancesUidsKey())
	{
		return KeyDisplayName = TEXT("Scene mesh instances count");
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FMeshNodeStaticData::GetSceneInstancesUidsKey().ToString()))
	{
		KeyDisplayName = TEXT("Scene mesh instances Index ");
		const FString IndexKey = UE::Interchange::TArrayAttributeHelper<FString>::IndexKey();
		int32 IndexPosition = NodeAttributeKeyString.Find(IndexKey) + IndexKey.Len();
		if (IndexPosition < NodeAttributeKeyString.Len())
		{
			KeyDisplayName += NodeAttributeKeyString.RightChop(IndexPosition);
		}
		return KeyDisplayName;
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FMeshNodeStaticData::GetSlotMaterialDependenciesKey().ToString()))
	{
		return KeyDisplayName = TEXT("Slot material dependencies");
	}
	return Super::GetKeyDisplayName(NodeAttributeKey);
}

FString UInterchangeMeshNode::GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
{
	const FString NodeAttributeKeyString = NodeAttributeKey.ToString();;
	if (NodeAttributeKeyString.StartsWith(UE::Interchange::FMeshNodeStaticData::GetSkeletonDependenciesKey().ToString()))
	{
		return FString(TEXT("SkeletonDependencies"));
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FMeshNodeStaticData::GetMorphTargetDependenciesKey().ToString()))
	{
		return FString(TEXT("MorphTargetDependencies"));
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FMeshNodeStaticData::GetSceneInstancesUidsKey().ToString()))
	{
		return FString(TEXT("SceneInstances"));
	}
	else if (NodeAttributeKey == Macro_CustomVertexCountKey
				|| NodeAttributeKey == Macro_CustomPolygonCountKey
				|| NodeAttributeKey == Macro_CustomBoundingBoxKey
				|| NodeAttributeKey == Macro_CustomHasVertexNormalKey
				|| NodeAttributeKey == Macro_CustomHasVertexBinormalKey
				|| NodeAttributeKey == Macro_CustomHasVertexTangentKey
				|| NodeAttributeKey == Macro_CustomHasSmoothGroupKey
				|| NodeAttributeKey == Macro_CustomHasVertexColorKey
				|| NodeAttributeKey == Macro_CustomUVCountKey)
	{
		return FString(TEXT("MeshInfo"));
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FMeshNodeStaticData::GetSlotMaterialDependenciesKey().ToString()))
	{
		return FString(TEXT("SlotMaterialDependencies"));
	}
	return Super::GetAttributeCategory(NodeAttributeKey);
}

FString UInterchangeMeshNode::GetTypeName() const
{
	const FString TypeName = TEXT("MeshNode");
	return TypeName;
}

FName UInterchangeMeshNode::GetIconName() const
{
	FString MeshIconName = TEXT("MeshIcon.");
	if (IsSkinnedMesh())
	{
		MeshIconName += TEXT("Skinned");
	}
	else
	{
		MeshIconName += TEXT("Static");
	}
		
	return FName(*MeshIconName);
}

bool UInterchangeMeshNode::IsSkinnedMesh() const
{
	if (!Attributes->ContainAttribute(UE::Interchange::FMeshNodeStaticData::IsSkinnedMeshKey()))
	{
		return false;
	}

	UE::Interchange::FAttributeStorage::TAttributeHandle<bool> Handle = Attributes->GetAttributeHandle<bool>(UE::Interchange::FMeshNodeStaticData::IsSkinnedMeshKey());
	if (Handle.IsValid())
	{
		bool bValue = false;
		Handle.Get(bValue);
		return bValue;
	}
	return false;
}

bool UInterchangeMeshNode::SetSkinnedMesh(const bool bIsSkinnedMesh)
{
	UE::Interchange::EAttributeStorageResult Result = Attributes->RegisterAttribute(UE::Interchange::FMeshNodeStaticData::IsSkinnedMeshKey(), bIsSkinnedMesh);
	if (IsAttributeStorageResultSuccess(Result))
	{
		UE::Interchange::FAttributeStorage::TAttributeHandle<bool> Handle = Attributes->GetAttributeHandle<bool>(UE::Interchange::FMeshNodeStaticData::IsSkinnedMeshKey());
		return Handle.IsValid();
	}
	return false;
}

bool UInterchangeMeshNode::IsMorphTarget() const
{
	if (!Attributes->ContainAttribute(UE::Interchange::FMeshNodeStaticData::IsMorphTargetKey()))
	{
		return false;
	}

	UE::Interchange::FAttributeStorage::TAttributeHandle<bool> Handle = Attributes->GetAttributeHandle<bool>(UE::Interchange::FMeshNodeStaticData::IsMorphTargetKey());
	if (Handle.IsValid())
	{
		bool bValue = false;
		Handle.Get(bValue);
		return bValue;
	}
	return false;
}

bool UInterchangeMeshNode::SetMorphTarget(const bool bIsMorphTarget)
{
	UE::Interchange::EAttributeStorageResult Result = Attributes->RegisterAttribute(UE::Interchange::FMeshNodeStaticData::IsMorphTargetKey(), bIsMorphTarget);
	if (IsAttributeStorageResultSuccess(Result))
	{
		UE::Interchange::FAttributeStorage::TAttributeHandle<bool> Handle = Attributes->GetAttributeHandle<bool>(UE::Interchange::FMeshNodeStaticData::IsMorphTargetKey());
		return Handle.IsValid();
	}
	return false;
}

bool UInterchangeMeshNode::GetMorphTargetName(FString& OutMorphTargetName) const
{
	OutMorphTargetName.Empty();
	if (!Attributes->ContainAttribute(UE::Interchange::FMeshNodeStaticData::MorphTargetNameKey()))
	{
		return false;
	}

	UE::Interchange::FAttributeStorage::TAttributeHandle<FString> Handle = Attributes->GetAttributeHandle<FString>(UE::Interchange::FMeshNodeStaticData::MorphTargetNameKey());
	if (Handle.IsValid())
	{
		Handle.Get(OutMorphTargetName);
		return true;
	}
	return false;
}

bool UInterchangeMeshNode::SetMorphTargetName(const FString& MorphTargetName)
{
	UE::Interchange::EAttributeStorageResult Result = Attributes->RegisterAttribute(UE::Interchange::FMeshNodeStaticData::MorphTargetNameKey(), MorphTargetName);
	if (IsAttributeStorageResultSuccess(Result))
	{
		UE::Interchange::FAttributeStorage::TAttributeHandle<FString> Handle = Attributes->GetAttributeHandle<FString>(UE::Interchange::FMeshNodeStaticData::MorphTargetNameKey());
		return Handle.IsValid();
	}
	return false;
}

const TOptional<FString> UInterchangeMeshNode::GetPayLoadKey() const
{
	if (!Attributes->ContainAttribute(UE::Interchange::FMeshNodeStaticData::PayloadSourceKey()))
	{
		return TOptional<FString>();
	}
	UE::Interchange::FAttributeStorage::TAttributeHandle<FString> AttributeHandle = Attributes->GetAttributeHandle<FString>(UE::Interchange::FMeshNodeStaticData::PayloadSourceKey());
	if (!AttributeHandle.IsValid())
	{
		return TOptional<FString>();
	}
	FString PayloadKey;
	UE::Interchange::EAttributeStorageResult Result = AttributeHandle.Get(PayloadKey);
	if (!IsAttributeStorageResultSuccess(Result))
	{
		LogAttributeStorageErrors(Result, TEXT("UInterchangeMeshNode.GetPayLoadKey"), UE::Interchange::FMeshNodeStaticData::PayloadSourceKey());
		return TOptional<FString>();
	}
	return TOptional<FString>(PayloadKey);
}

void UInterchangeMeshNode::SetPayLoadKey(const FString& PayloadKey)
{
	UE::Interchange::EAttributeStorageResult Result = Attributes->RegisterAttribute(UE::Interchange::FMeshNodeStaticData::PayloadSourceKey(), PayloadKey);
	if (!IsAttributeStorageResultSuccess(Result))
	{
		LogAttributeStorageErrors(Result, TEXT("UInterchangeMeshNode.SetPayLoadKey"), UE::Interchange::FMeshNodeStaticData::PayloadSourceKey());
	}
}

const TOptional<FString> UInterchangeMeshNode::GetAnimationCurvePayLoadKey() const
{
	if (!Attributes->ContainAttribute(UE::Interchange::FMeshNodeStaticData::PayloadAnimationCurveKey()))
	{
		return TOptional<FString>();
	}
	UE::Interchange::FAttributeStorage::TAttributeHandle<FString> AttributeHandle = Attributes->GetAttributeHandle<FString>(UE::Interchange::FMeshNodeStaticData::PayloadAnimationCurveKey());
	if (!AttributeHandle.IsValid())
	{
		return TOptional<FString>();
	}
	FString PayloadKey;
	UE::Interchange::EAttributeStorageResult Result = AttributeHandle.Get(PayloadKey);
	if (!IsAttributeStorageResultSuccess(Result))
	{
		LogAttributeStorageErrors(Result, TEXT("UInterchangeMeshNode.GetAnimationCurvePayLoadKey"), UE::Interchange::FMeshNodeStaticData::PayloadAnimationCurveKey());
		return TOptional<FString>();
	}
	return TOptional<FString>(PayloadKey);
}

void UInterchangeMeshNode::SetAnimationCurvePayLoadKey(const FString& PayloadKey)
{
	UE::Interchange::EAttributeStorageResult Result = Attributes->RegisterAttribute(UE::Interchange::FMeshNodeStaticData::PayloadAnimationCurveKey(), PayloadKey);
	if (!IsAttributeStorageResultSuccess(Result))
	{
		LogAttributeStorageErrors(Result, TEXT("UInterchangeMeshNode.SetAnimationCurvePayLoadKey"), UE::Interchange::FMeshNodeStaticData::PayloadAnimationCurveKey());
	}
}

bool UInterchangeMeshNode::GetCustomVertexCount(int32& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(VertexCount, int32);
}

bool UInterchangeMeshNode::SetCustomVertexCount(const int32& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(VertexCount, int32);
}

bool UInterchangeMeshNode::GetCustomPolygonCount(int32& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(PolygonCount, int32);
}

bool UInterchangeMeshNode::SetCustomPolygonCount(const int32& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(PolygonCount, int32);
}

bool UInterchangeMeshNode::GetCustomBoundingBox(FBox& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(BoundingBox, FBox);
}

bool UInterchangeMeshNode::SetCustomBoundingBox(const FBox& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(BoundingBox, FBox);
}

bool UInterchangeMeshNode::GetCustomHasVertexNormal(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(HasVertexNormal, bool);
}

bool UInterchangeMeshNode::SetCustomHasVertexNormal(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(HasVertexNormal, bool);
}

bool UInterchangeMeshNode::GetCustomHasVertexBinormal(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(HasVertexBinormal, bool);
}

bool UInterchangeMeshNode::SetCustomHasVertexBinormal(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(HasVertexBinormal, bool);
}

bool UInterchangeMeshNode::GetCustomHasVertexTangent(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(HasVertexTangent, bool);
}

bool UInterchangeMeshNode::SetCustomHasVertexTangent(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(HasVertexTangent, bool);
}

bool UInterchangeMeshNode::GetCustomHasSmoothGroup(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(HasSmoothGroup, bool);
}

bool UInterchangeMeshNode::SetCustomHasSmoothGroup(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(HasSmoothGroup, bool);
}

bool UInterchangeMeshNode::GetCustomHasVertexColor(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(HasVertexColor, bool);
}

bool UInterchangeMeshNode::SetCustomHasVertexColor(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(HasVertexColor, bool);
}

bool UInterchangeMeshNode::GetCustomUVCount(int32& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(UVCount, int32);
}

bool UInterchangeMeshNode::SetCustomUVCount(const int32& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(UVCount, int32);
}

int32 UInterchangeMeshNode::GetSkeletonDependeciesCount() const
{
	return SkeletonDependencies.GetCount();
}

void UInterchangeMeshNode::GetSkeletonDependencies(TArray<FString>& OutDependencies) const
{
	SkeletonDependencies.GetItems(OutDependencies);
}

void UInterchangeMeshNode::GetSkeletonDependency(const int32 Index, FString& OutDependency) const
{
	SkeletonDependencies.GetItem(Index, OutDependency);
}

bool UInterchangeMeshNode::SetSkeletonDependencyUid(const FString& DependencyUid)
{ 
	return SkeletonDependencies.AddItem(DependencyUid);
}

bool UInterchangeMeshNode::RemoveSkeletonDependencyUid(const FString& DependencyUid)
{
	return SkeletonDependencies.RemoveItem(DependencyUid);
}

int32 UInterchangeMeshNode::GetMorphTargetDependeciesCount() const
{
	return MorphTargetDependencies.GetCount();
}

void UInterchangeMeshNode::GetMorphTargetDependencies(TArray<FString>& OutDependencies) const
{
	MorphTargetDependencies.GetItems(OutDependencies);
}

void UInterchangeMeshNode::GetMorphTargetDependency(const int32 Index, FString& OutDependency) const
{
	MorphTargetDependencies.GetItem(Index, OutDependency);
}

bool UInterchangeMeshNode::SetMorphTargetDependencyUid(const FString& DependencyUid)
{
	return MorphTargetDependencies.AddItem(DependencyUid);
}

bool UInterchangeMeshNode::RemoveMorphTargetDependencyUid(const FString& DependencyUid)
{
	return MorphTargetDependencies.RemoveItem(DependencyUid);
}

int32 UInterchangeMeshNode::GetSceneInstanceUidsCount() const
{
	return SceneInstancesUids.GetCount();
}

void UInterchangeMeshNode::GetSceneInstanceUids(TArray<FString>& OutDependencies) const
{
	SceneInstancesUids.GetItems(OutDependencies);
}

void UInterchangeMeshNode::GetSceneInstanceUid(const int32 Index, FString& OutDependency) const
{
	SceneInstancesUids.GetItem(Index, OutDependency);
}

bool UInterchangeMeshNode::SetSceneInstanceUid(const FString& DependencyUid)
{
	return SceneInstancesUids.AddItem(DependencyUid);
}

bool UInterchangeMeshNode::RemoveSceneInstanceUid(const FString& DependencyUid)
{
	return SceneInstancesUids.RemoveItem(DependencyUid);
}

void UInterchangeMeshNode::GetSlotMaterialDependencies(TMap<FString, FString>& OutMaterialDependencies) const
{
	OutMaterialDependencies = SlotMaterialDependencies.ToMap();
}

bool UInterchangeMeshNode::GetSlotMaterialDependencyUid(const FString& SlotName, FString& OutMaterialDependency) const
{
	return SlotMaterialDependencies.GetValue(SlotName, OutMaterialDependency);
}

bool UInterchangeMeshNode::SetSlotMaterialDependencyUid(const FString& SlotName, const FString& MaterialDependencyUid)
{
	return SlotMaterialDependencies.SetKeyValue(SlotName, MaterialDependencyUid);
}

bool UInterchangeMeshNode::RemoveSlotMaterialDependencyUid(const FString& SlotName)
{
	return SlotMaterialDependencies.RemoveKey(SlotName);
}


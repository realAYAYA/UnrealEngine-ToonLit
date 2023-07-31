// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeSkeletalMeshLodDataNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeSkeletalMeshLodDataNode)

//Interchange namespace
namespace UE::Interchange
{
	const FAttributeKey& FSkeletalMeshNodeLodDataStaticData::GetMeshUidsBaseKey()
	{
		static FAttributeKey MeshUids_BaseKey(TEXT("__MeshUids__Key"));
		return MeshUids_BaseKey;
	}
}//ns UE::Interchange

UInterchangeSkeletalMeshLodDataNode::UInterchangeSkeletalMeshLodDataNode()
{
	MeshUids.Initialize(Attributes, UE::Interchange::FSkeletalMeshNodeLodDataStaticData::GetMeshUidsBaseKey().ToString());
}

/**
	* Return the node type name of the class, we use this when reporting error
	*/
FString UInterchangeSkeletalMeshLodDataNode::GetTypeName() const
{
	const FString TypeName = TEXT("SkeletalMeshLodDataNode");
	return TypeName;
}

FString UInterchangeSkeletalMeshLodDataNode::GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
{
	FString KeyDisplayName = NodeAttributeKey.ToString();
	const FString NodeAttributeKeyString = KeyDisplayName;
	if (NodeAttributeKey == UE::Interchange::FSkeletalMeshNodeLodDataStaticData::GetMeshUidsBaseKey())
	{
		KeyDisplayName = TEXT("Mesh count");
		return KeyDisplayName;
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FSkeletalMeshNodeLodDataStaticData::GetMeshUidsBaseKey().ToString()))
	{
		KeyDisplayName = TEXT("Mesh index ");
		const FString IndexKey = UE::Interchange::TArrayAttributeHelper<FString>::IndexKey();
		int32 IndexPosition = NodeAttributeKeyString.Find(IndexKey) + IndexKey.Len();
		if (IndexPosition < NodeAttributeKeyString.Len())
		{
			KeyDisplayName += NodeAttributeKeyString.RightChop(IndexPosition);
		}
		return KeyDisplayName;
	}
	else if (NodeAttributeKey == Macro_CustomSkeletonUidKey)
	{
		KeyDisplayName = TEXT("Skeleton factory node");
		return KeyDisplayName;
	}
	return Super::GetKeyDisplayName(NodeAttributeKey);
}

FString UInterchangeSkeletalMeshLodDataNode::GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
{
	if (NodeAttributeKey.ToString().StartsWith(UE::Interchange::FSkeletalMeshNodeLodDataStaticData::GetMeshUidsBaseKey().ToString()))
	{
		return FString(TEXT("Meshes"));
	}
	return Super::GetAttributeCategory(NodeAttributeKey);
}

bool UInterchangeSkeletalMeshLodDataNode::GetCustomSkeletonUid(FString& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(SkeletonUid, FString);
}

bool UInterchangeSkeletalMeshLodDataNode::SetCustomSkeletonUid(const FString& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(SkeletonUid, FString)
}

int32 UInterchangeSkeletalMeshLodDataNode::GetMeshUidsCount() const
{
	return MeshUids.GetCount();
}

void UInterchangeSkeletalMeshLodDataNode::GetMeshUids(TArray<FString>& OutMeshNames) const
{
	MeshUids.GetItems(OutMeshNames);
}

bool UInterchangeSkeletalMeshLodDataNode::AddMeshUid(const FString& MeshName)
{
	return MeshUids.AddItem(MeshName);
}

bool UInterchangeSkeletalMeshLodDataNode::RemoveMeshUid(const FString& MeshName)
{
	return MeshUids.RemoveItem(MeshName);
}

bool UInterchangeSkeletalMeshLodDataNode::RemoveAllMeshes()
{
	return MeshUids.RemoveAllItems();
}

bool UInterchangeSkeletalMeshLodDataNode::IsEditorOnlyDataDefined()
{
#if WITH_EDITORONLY_DATA
	return true;
#else
	return false;
#endif
}


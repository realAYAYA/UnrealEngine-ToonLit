// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeSceneNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeSceneNode)

//Interchange namespace
namespace UE
{
	namespace Interchange
	{

		const FAttributeKey& FSceneNodeStaticData::GetNodeSpecializeTypeBaseKey()
		{
			static FAttributeKey SceneNodeSpecializeType_BaseKey(TEXT("SceneNodeSpecializeType"));
			return SceneNodeSpecializeType_BaseKey;
		}

		const FAttributeKey& FSceneNodeStaticData::GetMaterialDependencyUidsBaseKey()
		{
			static FAttributeKey MaterialDependencyUids_BaseKey(TEXT("__MaterialDependencyUidsBaseKey__"));
			return MaterialDependencyUids_BaseKey;
		}
		
		const FString& FSceneNodeStaticData::GetTransformSpecializeTypeString()
		{
			static FString TransformSpecializeTypeString(TEXT("Transform"));
			return TransformSpecializeTypeString;
		}

		const FString& FSceneNodeStaticData::GetJointSpecializeTypeString()
		{
			static FString JointSpecializeTypeString(TEXT("Joint"));
			return JointSpecializeTypeString;
		}

		const FString& FSceneNodeStaticData::GetLodGroupSpecializeTypeString()
		{
			static FString LodGroupSpecializeTypeString(TEXT("LodGroup"));
			return LodGroupSpecializeTypeString;
		}

		const FString& FSceneNodeStaticData::GetSlotMaterialDependenciesString()
		{
			static FString SlotMaterialDependenciesString(TEXT("__SlotMaterialDependencies__"));
			return SlotMaterialDependenciesString;
		}
	}//ns Interchange
}//ns UE

UInterchangeSceneNode::UInterchangeSceneNode()
{
	NodeSpecializeTypes.Initialize(Attributes, UE::Interchange::FSceneNodeStaticData::GetNodeSpecializeTypeBaseKey().ToString());
	SlotMaterialDependencies.Initialize(Attributes.ToSharedRef(), UE::Interchange::FSceneNodeStaticData::GetSlotMaterialDependenciesString());
}

/**
	* Return the node type name of the class, we use this when reporting error
	*/
FString UInterchangeSceneNode::GetTypeName() const
{
	const FString TypeName = TEXT("SceneNode");
	return TypeName;
}

FString UInterchangeSceneNode::GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
{
	FString KeyDisplayName = NodeAttributeKey.ToString();
	const FString NodeAttributeKeyString = KeyDisplayName;
	if (NodeAttributeKey == UE::Interchange::FSceneNodeStaticData::GetNodeSpecializeTypeBaseKey())
	{
		KeyDisplayName = TEXT("Specialized type count");
		return KeyDisplayName;
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FSceneNodeStaticData::GetNodeSpecializeTypeBaseKey().ToString()))
	{
		KeyDisplayName = TEXT("Specialized type index ");
		const FString IndexKey = UE::Interchange::TArrayAttributeHelper<FString>::IndexKey();
		int32 IndexPosition = NodeAttributeKeyString.Find(IndexKey) + IndexKey.Len();
		if (IndexPosition < NodeAttributeKeyString.Len())
		{
			KeyDisplayName += NodeAttributeKeyString.RightChop(IndexPosition);
		}
		return KeyDisplayName;
	}
	else if (NodeAttributeKey == UE::Interchange::FSceneNodeStaticData::GetMaterialDependencyUidsBaseKey())
	{
		KeyDisplayName = TEXT("Material dependencies count");
		return KeyDisplayName;
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FSceneNodeStaticData::GetMaterialDependencyUidsBaseKey().ToString()))
	{
		KeyDisplayName = TEXT("Material dependency index ");
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

FString UInterchangeSceneNode::GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
{
	const FString NodeAttributeKeyString = NodeAttributeKey.ToString();

	if (NodeAttributeKey == Macro_CustomLocalTransformKey
		|| NodeAttributeKey == Macro_CustomAssetInstanceUidKey)
	{
		return FString(TEXT("Scene"));
	}
	else if (NodeAttributeKey == Macro_CustomBindPoseLocalTransformKey
		|| NodeAttributeKey == Macro_CustomTimeZeroLocalTransformKey)
	{
		return FString(TEXT("Joint"));
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FSceneNodeStaticData::GetNodeSpecializeTypeBaseKey().ToString()))
	{
		return FString(TEXT("SpecializeType"));
	}
	else if (NodeAttributeKeyString.StartsWith(UE::Interchange::FSceneNodeStaticData::GetMaterialDependencyUidsBaseKey().ToString()))
	{
		return FString(TEXT("MaterialDependencies"));
	}
	
	return Super::GetAttributeCategory(NodeAttributeKey);
}

FName UInterchangeSceneNode::GetIconName() const
{
	FString SpecializedType;
	GetSpecializedType(0, SpecializedType);
	if (SpecializedType.IsEmpty())
	{
		return NAME_None;
	}
	SpecializedType = TEXT("SceneGraphIcon.") + SpecializedType;
	return FName(*SpecializedType);
}

bool UInterchangeSceneNode::IsSpecializedTypeContains(const FString& SpecializedType) const
{
	TArray<FString> SpecializedTypes;
	GetSpecializedTypes(SpecializedTypes);
	for (const FString& SpecializedTypeRef : SpecializedTypes)
	{
		if (SpecializedTypeRef.Equals(SpecializedType))
		{
			return true;
		}
	}
	return false;
}

int32 UInterchangeSceneNode::GetSpecializedTypeCount() const
{
	return NodeSpecializeTypes.GetCount();
}

void UInterchangeSceneNode::GetSpecializedType(const int32 Index, FString& OutSpecializedType) const
{
	NodeSpecializeTypes.GetItem(Index, OutSpecializedType);
}

void UInterchangeSceneNode::GetSpecializedTypes(TArray<FString>& OutSpecializedTypes) const
{
	NodeSpecializeTypes.GetItems(OutSpecializedTypes);
}

bool UInterchangeSceneNode::AddSpecializedType(const FString& SpecializedType)
{
	return NodeSpecializeTypes.AddItem(SpecializedType);
}

bool UInterchangeSceneNode::RemoveSpecializedType(const FString& SpecializedType)
{
	return NodeSpecializeTypes.RemoveItem(SpecializedType);
}

bool UInterchangeSceneNode::GetCustomLocalTransform(FTransform& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(LocalTransform, FTransform);
}

bool UInterchangeSceneNode::SetCustomLocalTransform(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FTransform& AttributeValue, bool bResetCache /*= true*/)
{
	if(bResetCache)
	{
		ResetGlobalTransformCachesOfNodeAndAllChildren(BaseNodeContainer, this);
	}
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(LocalTransform, FTransform);
}

bool UInterchangeSceneNode::GetCustomGlobalTransform(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FTransform& GlobalOffsetTransform, FTransform& AttributeValue, bool bForceRecache /*= false*/) const
{
	return GetGlobalTransformInternal(Macro_CustomLocalTransformKey, CacheGlobalTransform, BaseNodeContainer, GlobalOffsetTransform, AttributeValue, bForceRecache);
}

bool UInterchangeSceneNode::GetCustomBindPoseLocalTransform(FTransform& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(BindPoseLocalTransform, FTransform);
}

bool UInterchangeSceneNode::SetCustomBindPoseLocalTransform(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FTransform& AttributeValue, bool bResetCache /*= true*/)
{
	if (bResetCache)
	{
		ResetGlobalTransformCachesOfNodeAndAllChildren(BaseNodeContainer, this);
	}
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(BindPoseLocalTransform, FTransform);
}

bool UInterchangeSceneNode::GetCustomBindPoseGlobalTransform(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FTransform& GlobalOffsetTransform, FTransform& AttributeValue, bool bForceRecache /*= false*/) const
{
	return GetGlobalTransformInternal(Macro_CustomBindPoseLocalTransformKey, CacheBindPoseGlobalTransform, BaseNodeContainer, GlobalOffsetTransform, AttributeValue, bForceRecache);
}

bool UInterchangeSceneNode::GetCustomTimeZeroLocalTransform(FTransform& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(TimeZeroLocalTransform, FTransform);
}

bool UInterchangeSceneNode::SetCustomTimeZeroLocalTransform(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FTransform& AttributeValue, bool bResetCache /*= true*/)
{
	if (bResetCache)
	{
		ResetGlobalTransformCachesOfNodeAndAllChildren(BaseNodeContainer, this);
	}
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(TimeZeroLocalTransform, FTransform);
}

bool UInterchangeSceneNode::GetCustomTimeZeroGlobalTransform(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FTransform& GlobalOffsetTransform, FTransform& AttributeValue, bool bForceRecache /*= false*/) const
{
	return GetGlobalTransformInternal(Macro_CustomTimeZeroLocalTransformKey, CacheTimeZeroGlobalTransform, BaseNodeContainer, GlobalOffsetTransform, AttributeValue, bForceRecache);
}

bool UInterchangeSceneNode::GetCustomGeometricTransform(FTransform& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(GeometricTransform, FTransform);
}

bool UInterchangeSceneNode::SetCustomGeometricTransform(const FTransform& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(GeometricTransform, FTransform);
}

bool UInterchangeSceneNode::GetCustomAssetInstanceUid(FString& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(AssetInstanceUid, FString);
}

bool UInterchangeSceneNode::SetCustomAssetInstanceUid(const FString& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(AssetInstanceUid, FString);
}

void UInterchangeSceneNode::ResetAllGlobalTransformCaches(const UInterchangeBaseNodeContainer* BaseNodeContainer)
{
	BaseNodeContainer->IterateNodes([](const FString& NodeUid, UInterchangeBaseNode* Node)
		{
			if (UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(Node))
			{
				SceneNode->CacheGlobalTransform.Reset();
				SceneNode->CacheBindPoseGlobalTransform.Reset();
				SceneNode->CacheTimeZeroGlobalTransform.Reset();
			}
		});
}

void UInterchangeSceneNode::ResetGlobalTransformCachesOfNodeAndAllChildren(const UInterchangeBaseNodeContainer* BaseNodeContainer, const UInterchangeBaseNode* ParentNode)
{
	check(ParentNode);
	if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(ParentNode))
	{
		SceneNode->CacheGlobalTransform.Reset();
		SceneNode->CacheBindPoseGlobalTransform.Reset();
		SceneNode->CacheTimeZeroGlobalTransform.Reset();
	}
	TArray<FString> ChildrenUids = BaseNodeContainer->GetNodeChildrenUids(ParentNode->GetUniqueID());
	for (const FString& ChildUid : ChildrenUids)
	{
		if (const UInterchangeBaseNode* ChildNode = BaseNodeContainer->GetNode(ChildUid))
		{
			ResetGlobalTransformCachesOfNodeAndAllChildren(BaseNodeContainer, ChildNode);
		}
	}
}

bool UInterchangeSceneNode::GetGlobalTransformInternal(const UE::Interchange::FAttributeKey LocalTransformKey
	, TOptional<FTransform>& CacheTransform
	, const UInterchangeBaseNodeContainer* BaseNodeContainer
	, const FTransform& GlobalOffsetTransform
	, FTransform& AttributeValue
	, bool bForceRecache) const
{
	if (!Attributes->ContainAttribute(LocalTransformKey))
	{
		return false;
	}
	if (bForceRecache)
	{
		CacheTransform.Reset();
	}
	if (!CacheTransform.IsSet())
	{
		FTransform LocalTransform;
		UE::Interchange::FAttributeStorage::TAttributeHandle<FTransform> AttributeHandle = GetAttributeHandle<FTransform>(LocalTransformKey);
		if (AttributeHandle.IsValid() && AttributeHandle.Get(LocalTransform) == UE::Interchange::EAttributeStorageResult::Operation_Success)
		{
			//Compute the Global
			if (Attributes->ContainAttribute(UE::Interchange::FBaseNodeStaticData::ParentIDKey()))
			{
				FTransform GlobalParent;
				if (const UInterchangeSceneNode* ParentSceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(GetParentUid())))
				{
					if (LocalTransformKey == Macro_CustomLocalTransformKey)
					{
						ParentSceneNode->GetCustomGlobalTransform(BaseNodeContainer, GlobalOffsetTransform, GlobalParent, bForceRecache);
					}
					else if (LocalTransformKey == Macro_CustomBindPoseLocalTransformKey)
					{
						//Its possible the root skeleton id have a parent that is not a joint, in that case we need to fall back on the normal transform
						if (!ParentSceneNode->GetCustomBindPoseGlobalTransform(BaseNodeContainer, GlobalOffsetTransform, GlobalParent, bForceRecache))
						{
							ParentSceneNode->GetCustomGlobalTransform(BaseNodeContainer, GlobalOffsetTransform, GlobalParent, bForceRecache);
						}
					}
					else if (LocalTransformKey == Macro_CustomTimeZeroLocalTransformKey)
					{
						//Its possible the root skeleton id have a parent that is not a joint, in that case we need to fall back on the normal transform
						if(!ParentSceneNode->GetCustomTimeZeroGlobalTransform(BaseNodeContainer, GlobalOffsetTransform, GlobalParent, bForceRecache))
						{
							ParentSceneNode->GetCustomGlobalTransform(BaseNodeContainer, GlobalOffsetTransform, GlobalParent, bForceRecache);
						}
					}
				}
				CacheTransform = LocalTransform * GlobalParent;
			}
			else
			{
				//Scene Node without parent will need the global offset to be apply
				CacheTransform = LocalTransform * GlobalOffsetTransform;
			}
		}
		else
		{
			CacheTransform = FTransform::Identity;
		}
	}
	//The cache is always valid here
	check(CacheTransform.IsSet());
	AttributeValue = CacheTransform.GetValue();
	return true;
}

void UInterchangeSceneNode::GetSlotMaterialDependencies(TMap<FString, FString>& OutMaterialDependencies) const
{
	OutMaterialDependencies = SlotMaterialDependencies.ToMap();
}

bool UInterchangeSceneNode::GetSlotMaterialDependencyUid(const FString& SlotName, FString& OutMaterialDependency) const
{
	return SlotMaterialDependencies.GetValue(SlotName, OutMaterialDependency);
}

bool UInterchangeSceneNode::SetSlotMaterialDependencyUid(const FString& SlotName, const FString& MaterialDependencyUid)
{
	return SlotMaterialDependencies.SetKeyValue(SlotName, MaterialDependencyUid);
}

bool UInterchangeSceneNode::RemoveSlotMaterialDependencyUid(const FString& SlotName)
{
	return SlotMaterialDependencies.RemoveKey(SlotName);
}


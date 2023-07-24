// Copyright Epic Games, Inc. All Rights Reserved.
#include "Nodes/InterchangeFactoryBaseNode.h"

#include "CoreMinimal.h"
#include "Types/AttributeStorage.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeFactoryBaseNode)


namespace UE
{
	namespace Interchange
	{
		const FString& FFactoryBaseNodeStaticData::FactoryDependenciesBaseKey()
		{
			static FString BaseNodeFactoryDependencies_BaseKey = TEXT("__BaseNodeFactoryDependencies__");
			return BaseNodeFactoryDependencies_BaseKey;
		}

		const FAttributeKey& FFactoryBaseNodeStaticData::ReimportStrategyFlagsKey()
		{
			static FAttributeKey AttributeKey(TEXT("__Reimport_Strategy_Flags_Key__"));
			return AttributeKey;
		}

	} //ns Interchange
} //ns UE


UInterchangeFactoryBaseNode::UInterchangeFactoryBaseNode()
{
	FactoryDependencies.Initialize(Attributes, UE::Interchange::FFactoryBaseNodeStaticData::FactoryDependenciesBaseKey());

	RegisterAttribute<uint8>(UE::Interchange::FFactoryBaseNodeStaticData::ReimportStrategyFlagsKey(), static_cast<uint8>(EReimportStrategyFlags::ApplyNoProperties));
}


FString UInterchangeFactoryBaseNode::GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
{
	FString KeyDisplayName = NodeAttributeKey.ToString();
	const FString OriginalKeyName = KeyDisplayName;
	if (NodeAttributeKey == Macro_CustomSubPathKey)
	{
		KeyDisplayName = TEXT("Import Sub-Path");
	}
	else if (OriginalKeyName.Equals(UE::Interchange::FFactoryBaseNodeStaticData::FactoryDependenciesBaseKey()))
	{
		KeyDisplayName = TEXT("Factory Dependencies Count");
	}
	else if (OriginalKeyName.StartsWith(UE::Interchange::FFactoryBaseNodeStaticData::FactoryDependenciesBaseKey()))
	{
		KeyDisplayName = TEXT("Factory Dependencies Index ");
		const FString IndexKey = UE::Interchange::TArrayAttributeHelper<FString>::IndexKey();
		int32 IndexPosition = OriginalKeyName.Find(IndexKey) + IndexKey.Len();
		if (IndexPosition < OriginalKeyName.Len())
		{
			KeyDisplayName += OriginalKeyName.RightChop(IndexPosition);
		}
	}
	else if (NodeAttributeKey == UE::Interchange::FFactoryBaseNodeStaticData::ReimportStrategyFlagsKey())
	{
		KeyDisplayName = TEXT("Re-Import Strategy");
	}
	else
	{
		KeyDisplayName = Super::GetKeyDisplayName(NodeAttributeKey);
	}

	return KeyDisplayName;
}


FString UInterchangeFactoryBaseNode::GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
{
	if (NodeAttributeKey.ToString().StartsWith(UE::Interchange::FFactoryBaseNodeStaticData::FactoryDependenciesBaseKey()))
	{
		return TEXT("FactoryDependencies");
	}
	else
	{
		return Super::GetAttributeCategory(NodeAttributeKey);
	}
}


UClass* UInterchangeFactoryBaseNode::GetObjectClass() const
{
	return nullptr;
}

EReimportStrategyFlags UInterchangeFactoryBaseNode::GetReimportStrategyFlags() const
{
	checkSlow(Attributes->ContainAttribute(UE::Interchange::FFactoryBaseNodeStaticData::ReimportStrategyFlagsKey()));
	uint8 ReimportStrategyFlags = static_cast<uint8>(EReimportStrategyFlags::ApplyNoProperties);
	Attributes->GetAttributeHandle<uint8>(UE::Interchange::FFactoryBaseNodeStaticData::ReimportStrategyFlagsKey()).Get(ReimportStrategyFlags);
	return static_cast<EReimportStrategyFlags>(ReimportStrategyFlags);
}

bool UInterchangeFactoryBaseNode::SetReimportStrategyFlags(const EReimportStrategyFlags& ReimportStrategyFlags)
{
	UE::Interchange::EAttributeStorageResult Result = Attributes->RegisterAttribute(UE::Interchange::FFactoryBaseNodeStaticData::ReimportStrategyFlagsKey(), static_cast<uint8>(ReimportStrategyFlags));
	if (IsAttributeStorageResultSuccess(Result))
	{
		UE::Interchange::FAttributeStorage::TAttributeHandle<uint8> Handle = Attributes->GetAttributeHandle<uint8>(UE::Interchange::FFactoryBaseNodeStaticData::ReimportStrategyFlagsKey());
		return Handle.IsValid();
	}
	return false;
}

bool UInterchangeFactoryBaseNode::GetCustomSubPath(FString& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(SubPath, FString)
}

bool UInterchangeFactoryBaseNode::SetCustomSubPath(const FString& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(SubPath, FString)
}

int32 UInterchangeFactoryBaseNode::GetFactoryDependenciesCount() const
{
	return FactoryDependencies.GetCount();
}

void UInterchangeFactoryBaseNode::GetFactoryDependency(const int32 Index, FString& OutDependency) const
{
	FactoryDependencies.GetItem(Index, OutDependency);
}

void UInterchangeFactoryBaseNode::GetFactoryDependencies(TArray<FString>& OutDependencies) const
{
	FactoryDependencies.GetItems(OutDependencies);
}

bool UInterchangeFactoryBaseNode::AddFactoryDependencyUid(const FString& DependencyUid)
{
	return FactoryDependencies.AddItem(DependencyUid);
}

bool UInterchangeFactoryBaseNode::RemoveFactoryDependencyUid(const FString& DependencyUid)
{
	return FactoryDependencies.RemoveItem(DependencyUid);
}

bool UInterchangeFactoryBaseNode::GetCustomReferenceObject(FSoftObjectPath& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ReferenceObject, FSoftObjectPath)
}

bool UInterchangeFactoryBaseNode::SetCustomReferenceObject(const FSoftObjectPath& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ReferenceObject, FSoftObjectPath)
}

FString UInterchangeFactoryBaseNode::BuildFactoryNodeUid(const FString& TranslatedNodeUid)
{
	return TEXT("Factory_") + TranslatedNodeUid;
}

void UInterchangeFactoryBaseNode::ApplyAllCustomAttributeToObject(UObject* Object) const
{
	UClass* ObjectClass = Object->GetClass();
	for (const TPair<UClass*, TArray<UE::Interchange::FApplyAttributeToAsset>>& ClassDelegatePair : ApplyCustomAttributeDelegates)
	{
		if (ObjectClass->IsChildOf(ClassDelegatePair.Key))
		{
			for (const UE::Interchange::FApplyAttributeToAsset& Delegate : ClassDelegatePair.Value)
			{
				if (Delegate.IsBound())
				{
					Delegate.Execute(Object);
				}
			}
		}
	}
}

void UInterchangeFactoryBaseNode::FillAllCustomAttributeFromObject(UObject* Object) const
{
	UClass* ObjectClass = Object->GetClass();
	for (const TPair<UClass*, TArray<UE::Interchange::FFillAttributeToAsset>>& ClassDelegatePair : FillCustomAttributeDelegates)
	{
		if (ObjectClass->IsChildOf(ClassDelegatePair.Key))
		{
			for (const UE::Interchange::FFillAttributeToAsset& Delegate : ClassDelegatePair.Value)
			{
				if (Delegate.IsBound())
				{
					Delegate.Execute(Object);
				}
			}
		}
	}
}
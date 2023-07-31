// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#if WITH_ENGINE
#include "Animation/Skeleton.h"
#endif

#include "InterchangeSkeletonFactoryNode.generated.h"

UCLASS(BlueprintType, Experimental)
class INTERCHANGEFACTORYNODES_API UInterchangeSkeletonFactoryNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	/**
	 * Initialize node data
	 * @param: UniqueID - The uniqueId for this node
	 * @param DisplayLabel - The name of the node
	 * @param InAssetClass - The class the Skeleton factory will create for this node.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Skeleton")
	void InitializeSkeletonNode(const FString& UniqueID, const FString& DisplayLabel, const FString& InAssetClass)
	{
		bIsNodeClassInitialized = false;
		InitializeNode(UniqueID, DisplayLabel, EInterchangeNodeContainerType::FactoryData);
		FString OperationName = GetTypeName() + TEXT(".SetAssetClassName");
		InterchangePrivateNodeBase::SetCustomAttribute<FString>(*Attributes, ClassNameAttributeKey, OperationName, InAssetClass);
		FillAssetClassFromAttribute();
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);
#if WITH_ENGINE
		if(Ar.IsLoading())
		{
			//Make sure the class is properly set when we compile with engine, this will set the
			//bIsNodeClassInitialized to true.
			SetNodeClassFromClassAttribute();
		}
#endif //#if WITH_ENGINE
	}

	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("SkeletonNode");
		return TypeName;
	}

	/** Get the class this node want to create */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Skeleton")
	virtual class UClass* GetObjectClass() const override
	{
		ensure(bIsNodeClassInitialized);
#if WITH_ENGINE
		return AssetClass.Get() != nullptr ? AssetClass.Get() : USkeleton::StaticClass();
#else
		return nullptr;
#endif
	}

	virtual FGuid GetHash() const override
	{
		return Attributes->GetStorageHash();
	}

public:
	/** Return false if the Attribute was not set previously.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Skeleton")
	bool GetCustomRootJointUid(FString& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(RootJointID, FString);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Skeleton")
	bool SetCustomRootJointUid(const FString& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(RootJointID, FString)
	}

	/** Query if this skeleton should replace joint transform with time zero evaluation instead of bind pose.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Skeleton")
	bool GetCustomUseTimeZeroForBindPose(bool& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(UseTimeZeroForBindPose, bool);
	}

	/** If AttributeValue is true, force this skeleton bind pose to use time zero instead of bind pose.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Skeleton")
	bool SetCustomUseTimeZeroForBindPose(const bool& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(UseTimeZeroForBindPose, bool)
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Skeleton")
	bool GetCustomSkeletalMeshFactoryNodeUid(FString& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(SkeletalMeshFactoryNodeUid, FString);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Skeleton")
	bool SetCustomSkeletalMeshFactoryNodeUid(const FString& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(SkeletalMeshFactoryNodeUid, FString)
	}

private:

	void FillAssetClassFromAttribute()
	{
#if WITH_ENGINE
		FString OperationName = GetTypeName() + TEXT(".GetAssetClassName");
		FString ClassName;
		InterchangePrivateNodeBase::GetCustomAttribute<FString>(*Attributes, ClassNameAttributeKey, OperationName, ClassName);
		if (ClassName.Equals(USkeleton::StaticClass()->GetName()))
		{
			AssetClass = USkeleton::StaticClass();
			bIsNodeClassInitialized = true;
		}
#endif
	}

	bool SetNodeClassFromClassAttribute()
	{
		if (!bIsNodeClassInitialized)
		{
			FillAssetClassFromAttribute();
		}
		return bIsNodeClassInitialized;
	}

	bool IsEditorOnlyDataDefined()
	{
#if WITH_EDITORONLY_DATA
		return true;
#else
		return false;
#endif
	}

	const UE::Interchange::FAttributeKey ClassNameAttributeKey = UE::Interchange::FBaseNodeStaticData::ClassTypeAttributeKey();

	//Skeleton
	const UE::Interchange::FAttributeKey Macro_CustomRootJointIDKey = UE::Interchange::FAttributeKey(TEXT("RootJointID"));
	const UE::Interchange::FAttributeKey Macro_CustomUseTimeZeroForBindPoseKey = UE::Interchange::FAttributeKey(TEXT("UseTimeZeroForBindPose"));
	const UE::Interchange::FAttributeKey Macro_CustomSkeletalMeshFactoryNodeUidKey = UE::Interchange::FAttributeKey(TEXT("SkeletalMeshFactoryNodeUid"));
	
protected:
#if WITH_ENGINE
	TSubclassOf<USkeleton> AssetClass = nullptr;
#endif
	bool bIsNodeClassInitialized = false;
};

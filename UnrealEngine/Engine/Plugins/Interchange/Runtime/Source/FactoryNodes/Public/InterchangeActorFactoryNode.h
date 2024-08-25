// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Nodes/InterchangeFactoryBaseNode.h"

#include "InterchangeActorFactoryNode.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeActorFactoryNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	virtual UClass* GetObjectClass() const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | ActorFactory")
	bool GetCustomGlobalTransform(FTransform& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | ActorFactory")
	bool SetCustomGlobalTransform(const FTransform& AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | ActorFactory")
	bool GetCustomLocalTransform(FTransform& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | ActorFactory")
	bool SetCustomLocalTransform(const FTransform& AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | ActorFactory")
	bool GetCustomActorClassName(FString& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(ActorClassName, FString);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | ActorFactory")
	bool SetCustomActorClassName(const FString& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ActorClassName, FString);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | ActorFactory")
	bool GetCustomMobility(uint8& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(Mobility, uint8);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | ActorFactory")
	bool SetCustomMobility(const uint8& AttributeValue, bool bAddApplyDelegate = true)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(Mobility, uint8);
	}

	virtual void CopyWithObject(const UInterchangeFactoryBaseNode* SourceNode, UObject* Object) override;

private:

	bool ApplyCustomGlobalTransformToAsset(UObject* Asset) const;
	bool FillCustomGlobalTransformFromAsset(UObject* Asset);

	const UE::Interchange::FAttributeKey Macro_CustomGlobalTransformKey = UE::Interchange::FAttributeKey(TEXT("GlobalTransform"));
	const UE::Interchange::FAttributeKey Macro_CustomLocalTransformKey = UE::Interchange::FAttributeKey(TEXT("LocalTransform"));
	const UE::Interchange::FAttributeKey Macro_CustomActorClassNameKey = UE::Interchange::FAttributeKey(TEXT("ActorClassName"));
	const UE::Interchange::FAttributeKey Macro_CustomMobilityKey = UE::Interchange::FAttributeKey(TEXT("Mobility"));
};
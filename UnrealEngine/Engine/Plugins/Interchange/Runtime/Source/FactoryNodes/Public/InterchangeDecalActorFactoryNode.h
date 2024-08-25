// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangeActorFactoryNode.h"

#include "InterchangeDecalActorFactoryNode.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeDecalActorFactoryNode : public UInterchangeActorFactoryNode
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Decal")
	UClass* GetObjectClass()const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Decal")
	bool GetCustomSortOrder(int32& AttributeValue) const;
	
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Decal")
	bool SetCustomSortOrder(const int32& AttributeValue, bool bAddApplyDelegate = true);
	
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Decal")
	bool GetCustomDecalSize(FVector& AttributeValue) const;
	
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Decal")
	bool SetCustomDecalSize(const FVector& AttributeValue, bool bAddApplyDelegate = true);
	
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Decal")
	bool GetCustomDecalMaterialPathName(FString& AttributeValue) const;
	

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Decal")
	bool SetCustomDecalMaterialPathName(const FString& AttributeValue);
	
private:
	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(SortOrder, int32, UDecalComponent, TEXT("SortOrder"));
	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(DecalSize, FVector, UDecalComponent, TEXT("DecalSize"));

private:
	const UE::Interchange::FAttributeKey Macro_CustomSortOrderKey = UE::Interchange::FAttributeKey(TEXT("SortOrder"));
	const UE::Interchange::FAttributeKey Macro_CustomDecalSizeKey = UE::Interchange::FAttributeKey(TEXT("DecalSize"));
	const UE::Interchange::FAttributeKey Macro_CustomDecalMaterialPathNameKey = UE::Interchange::FAttributeKey(TEXT("DecalMaterialPathName"));
};
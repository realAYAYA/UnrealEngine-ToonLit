// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeMaterialFactoryNode.h"

#include "InterchangeDecalMaterialFactoryNode.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeDecalMaterialFactoryNode : public UInterchangeBaseMaterialFactoryNode
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Decal")
	UClass* GetObjectClass()const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Decal")
	bool GetCustomDiffuseTexturePath(FString& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Decal")
	bool SetCustomDiffuseTexturePath(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Decal")
	bool GetCustomNormalTexturePath(FString& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Decal")
	bool SetCustomNormalTexturePath(const FString& AttributeValue);

private:
	const UE::Interchange::FAttributeKey Macro_CustomDiffuseTexturePathKey = UE::Interchange::FAttributeKey(TEXT("DiffuseTexturePath"));
	const UE::Interchange::FAttributeKey Macro_CustomNormalTexturePathKey = UE::Interchange::FAttributeKey(TEXT("NormalTexturePath"));
};
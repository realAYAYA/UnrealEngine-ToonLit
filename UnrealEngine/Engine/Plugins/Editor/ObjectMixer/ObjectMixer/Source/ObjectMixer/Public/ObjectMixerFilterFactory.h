// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "ObjectMixerFilterFactory.generated.h"

UCLASS(hidecategories = Object)
class OBJECTMIXEREDITOR_API UObjectMixerBlueprintFilterFactory : public UFactory
{
	GENERATED_BODY()
public:
	UObjectMixerBlueprintFilterFactory();
	
	// The parent class of the created blueprint
    UPROPERTY(VisibleAnywhere, Category="ObjectMixerBlueprintFilterFactory", meta=(AllowAbstract = "", BlueprintBaseOnly = ""))
    TSubclassOf<UObject> ParentClass;
	
	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FString GetDefaultNewAssetName() const override;
	virtual uint32 GetMenuCategories() const override;
	//~ Begin UFactory Interface
	
};

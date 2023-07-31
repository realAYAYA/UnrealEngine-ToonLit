// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/BlueprintFactory.h"
#include "UObject/ObjectMacros.h"

#include "LiveLinkBlueprintVirtualSubjectFactory.generated.h"

class ULiveLinkRole;

UCLASS(hidecategories = Object)
class LIVELINKEDITOR_API ULiveLinkBlueprintVirtualSubjectFactory : public UBlueprintFactory
{
	GENERATED_BODY()
	
public:
	ULiveLinkBlueprintVirtualSubjectFactory();

	UPROPERTY(BlueprintReadWrite, Category = "Live Link Blueprint Virtual Subject Factory")
	TSubclassOf<ULiveLinkRole> Role;

	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual bool ShouldShowInNewMenu() const override;
	virtual uint32 GetMenuCategories() const override;
	//~ Begin UFactory Interface
};

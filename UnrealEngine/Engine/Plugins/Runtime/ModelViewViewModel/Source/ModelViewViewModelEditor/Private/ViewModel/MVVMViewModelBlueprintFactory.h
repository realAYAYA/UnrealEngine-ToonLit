// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Factories/Factory.h"
#include "Engine/Blueprint.h"
#include "MVVMViewModelBase.h"

#include "MVVMViewModelBlueprintFactory.generated.h"

UCLASS(HideCategories=Object, MinimalAPI)
class UMVVMViewModelBlueprintFactory : public UFactory
{
	GENERATED_BODY()

public:
	UMVVMViewModelBlueprintFactory();

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual bool ShouldShowInNewMenu() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ End UFactory Interface	

private:
	// The parent class of the created blueprint
	UPROPERTY(EditAnywhere, Category="MVVM", meta=(AllowAbstract="true"))
	TSubclassOf<class UMVVMViewModelBase> ParentClass;
};

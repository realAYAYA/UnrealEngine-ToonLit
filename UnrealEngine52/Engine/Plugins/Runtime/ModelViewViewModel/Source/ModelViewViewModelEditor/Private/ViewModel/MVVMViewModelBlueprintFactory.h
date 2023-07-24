// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "MVVMViewModelBlueprintFactory.generated.h"

UCLASS(HideCategories=Object, MinimalAPI)
class UMVVMViewModelBlueprintFactory : public UFactory
{
	GENERATED_BODY()

public:
	UMVVMViewModelBlueprintFactory();

#if UE_MVVM_WITH_VIEWMODEL_EDITOR
	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual bool ShouldShowInNewMenu() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ End UFactory Interface
#endif

private:
	// The parent class of the created blueprint
	UPROPERTY(EditAnywhere, Category = "Viewmodel", meta=(AllowAbstract="true"))
	TSubclassOf<class UMVVMViewModelBase> ParentClass;
};

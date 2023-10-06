// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "INotifyFieldValueChanged.h"

#include "MVVMViewModelContextResolver.generated.h"

class UMVVMView;
class UMVVMViewClass;
class UUserWidget;

/**
 * Shared data to find or create a ViewModel at runtime.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DisplayName = "Viewmodel Resolver")
class MODELVIEWVIEWMODEL_API UMVVMViewModelContextResolver : public UObject
{
	GENERATED_BODY()

public:
	virtual UObject* CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const
	{
		return K2_CreateInstance(ExpectedType, UserWidget).GetObject();
	}

	UFUNCTION(BlueprintImplementableEvent, Category="Viewmodel", DisplayName="Create Instance")
	TScriptInterface<INotifyFieldValueChanged> K2_CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget) const;

	virtual void DestroyInstance(const UObject* ViewModel, const UMVVMView* View) const {}
};

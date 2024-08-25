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

	UFUNCTION(BlueprintImplementableEvent, Category = "Viewmodel", meta = (DisplayName = "Create Instance"))
	TScriptInterface<INotifyFieldValueChanged> K2_CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget) const;

	virtual void DestroyInstance(const UObject* ViewModel, const UMVVMView* View) const
	{
		K2_DestroyInstance(ViewModel, View);
	}

	UFUNCTION(BlueprintImplementableEvent, Category = "Viewmodel", meta = (DisplayName = "Destroy Instance"))
	void K2_DestroyInstance(const UObject* ViewModel, const UMVVMView* View) const;

public:
#if WITH_EDITOR
	virtual bool DoesSupportViewModelClass(const UClass* Class) const;
#endif

private:
#if WITH_EDITORONLY_DATA
	/** Viewmodel class that the resolver supports.*/
	UPROPERTY(EditDefaultsOnly, Category = "Viewmodel", meta=(MustImplement="/Script/FieldNotification.NotifyFieldValueChanged", DisallowedClasses="/Script/UMG.Widget"))
	TArray<FSoftClassPath> AllowedViewModelClasses;

	/** Viewmodel class that the resolver explicitly does not support. */
	UPROPERTY(EditDefaultsOnly, Category = "Viewmodel", meta=(MustImplement="/Script/FieldNotification.NotifyFieldValueChanged", DisallowedClasses="/Script/UMG.Widget"))
	TArray<FSoftClassPath> DeniedViewModelClasses;
#endif
};

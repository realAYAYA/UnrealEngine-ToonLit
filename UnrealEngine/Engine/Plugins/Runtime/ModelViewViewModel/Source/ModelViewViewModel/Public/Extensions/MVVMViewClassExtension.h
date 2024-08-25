// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MVVMViewClassExtension.generated.h"

class UMVVMView;
class UMVVMBlueprintViewExtension;
class UUserWidget;

/**
 * A runtime extension class to define MVVM-related properties and behaviour. This information comes from the
 * corresponding UMVVMBlueprintViewExtension class. This class provides a hook into the MVVM runtime initializations.
 */
UCLASS(MinimalAPI)
class UMVVMViewClassExtension : public UObject
{
	GENERATED_BODY()

public:
	//~ Functions to be overriden in a user-defined UMVVMViewMyWidgetExtension class
	virtual void OnSourcesInitialized(UUserWidget* UserWidget, UMVVMView* View) {};
	virtual void OnBindingsInitialized(UUserWidget* UserWidget, UMVVMView* View) {};
	virtual void OnEventsInitialized(UUserWidget* UserWidget, UMVVMView* View) {};
	virtual void OnEventsUninitialized(UUserWidget* UserWidget, UMVVMView* View) {};
	virtual void OnBindingsUninitialized(UUserWidget* UserWidget, UMVVMView* View) {};
	virtual void OnSourcesUninitialized(UUserWidget* UserWidget, UMVVMView* View) {};
};
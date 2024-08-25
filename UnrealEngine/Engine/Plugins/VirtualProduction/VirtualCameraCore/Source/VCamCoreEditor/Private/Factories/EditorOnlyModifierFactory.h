// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VCamObjectWithInputFactory.h"
#include "EditorOnlyModifierFactory.generated.h"

/**
 * 
 */
UCLASS()
class UEditorOnlyModifierFactory : public UVCamObjectWithInputFactory
{
	GENERATED_BODY()
public:

	UEditorOnlyModifierFactory();
	
	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTip() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	//~ Begin UFactory Interface
};

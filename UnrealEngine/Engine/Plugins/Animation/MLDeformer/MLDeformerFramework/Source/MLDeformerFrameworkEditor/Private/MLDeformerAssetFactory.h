// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "MLDeformerAssetFactory.generated.h"

/**
 * The factory for the ML Deformer asset type.
 * This basically integrates the new asset type into the editor, so you can right click and create a new ML Deformer asset.
 */
UCLASS(hidecategories=Object)
class UMLDeformerFactory
	: public UFactory
{
	GENERATED_BODY()

public:
	UMLDeformerFactory();

	// UFactory overrides.
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
	virtual FText GetToolTip() const override;
	virtual FString GetDefaultNewAssetName() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ConfigureProperties() override;
	virtual bool ShouldShowInNewMenu() const override;
	// ~END UFactory overrides.
};

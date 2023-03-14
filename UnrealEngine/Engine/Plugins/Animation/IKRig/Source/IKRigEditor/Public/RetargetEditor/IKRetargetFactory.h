// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "IKRetargetFactory.generated.h"

class SWindow;

UCLASS(hidecategories=Object, MinimalAPI)
class UIKRetargetFactory : public UFactory
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<class UIKRigDefinition>	SourceIKRig;

public:

	UIKRetargetFactory();

	// UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
	virtual FText GetToolTip() const override;
	virtual FString GetDefaultNewAssetName() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ConfigureProperties() override;
	virtual bool ShouldShowInNewMenu() const override;

private:
	
	void OnTargetIKRigSelected(const FAssetData& SelectedAsset);

	TSharedPtr<SWindow> PickerWindow;
};

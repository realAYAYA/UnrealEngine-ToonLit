// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "IKRigDefinitionFactory.generated.h"

class SWindow;

UCLASS(BlueprintType, hidecategories=Object)
class UIKRigDefinitionFactory : public UFactory
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<class USkeletalMesh>	SkeletalMesh;

public:

	UIKRigDefinitionFactory();

	// UFactory interface
	virtual UObject* FactoryCreateNew(
        UClass* Class,
        UObject* InParent,
        FName Name,
        EObjectFlags Flags,
        UObject* Context,
        FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
	virtual FText GetToolTip() const override;
	virtual FString GetDefaultNewAssetName() const override;
	virtual bool ConfigureProperties() override;
	virtual bool ShouldShowInNewMenu() const override;
	// END UFactory interface

private:
	
	void OnSkeletalMeshSelected(const FAssetData& SelectedAsset);

	TSharedPtr<SWindow> PickerWindow;
};

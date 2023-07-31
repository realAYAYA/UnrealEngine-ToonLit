// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions/AssetTypeActions_Blueprint.h"
#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UClass;
class UObject;


class FAssetTypeActions_EditorUtilityBlueprint : public FAssetTypeActions_Blueprint
{
public:
	// IAssetTypeActions interface
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual uint32 GetCategories() override;
	virtual bool CanLocalize() const override { return false; }
	// End of IAssetTypeActions interface

protected:
	typedef TArray< TWeakObjectPtr<class UEditorUtilityBlueprint> > FWeakBlueprintPointerArray;

	void ExecuteNewDerivedBlueprint(TWeakObjectPtr<class UEditorUtilityBlueprint> InObject);
	void ExecuteRun(FWeakBlueprintPointerArray Objects);
};

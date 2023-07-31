// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "NiagaraEditorModule.h"

class FAssetTypeActions_NiagaraScript : public FAssetTypeActions_Base
{
public:
	// Begin IAssetTypeActions Interface
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NiagaraScript", "Niagara Script"); }
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override { return FNiagaraEditorModule::GetAssetCategory(); }
	virtual FText GetDisplayNameFromAssetData(const FAssetData& AssetData) const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	// End IAssetTypeActions Interface

	/** Get the action type name. */
	virtual FName GetTypeName() const = 0;
};

class FAssetTypeActions_NiagaraScriptFunctions: public FAssetTypeActions_NiagaraScript
{
public:
	// Begin IAssetTypeActions Interface
	virtual FText GetName() const override { return GetFormattedName(); }
	virtual FName GetTypeName() const override { return NiagaraFunctionScriptName; }
	// End IAssetTypeActions Interface

	static FText GetFormattedName() { return FText::Format(NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NiagaraScriptFunctions", "{0}"), FText::FromName(NiagaraFunctionScriptName)); }
	static const FName NiagaraFunctionScriptName;
};

class FAssetTypeActions_NiagaraScriptModules: public FAssetTypeActions_NiagaraScript
{
public:
	// Begin IAssetTypeActions Interface
	virtual FText GetName() const override { return GetFormattedName(); }
	virtual FName GetTypeName() const override { return NiagaraModuleScriptName; }
	// End IAssetTypeActions Interface

	static FText GetFormattedName() { return FText::Format(NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NiagaraScriptModules", "{0}"), FText::FromName(NiagaraModuleScriptName)); }
	static const FName NiagaraModuleScriptName;
};

class FAssetTypeActions_NiagaraScriptDynamicInputs: public FAssetTypeActions_NiagaraScript
{
public:
	// Begin IAssetTypeActions Interface
	virtual FText GetName() const override { return GetFormattedName(); }
	virtual FName GetTypeName() const override { return NiagaraDynamicInputScriptName; }
	// End IAssetTypeActions Interface

	static FText GetFormattedName() { return FText::Format(NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NiagaraScriptDynamicInputs", "{0}"), FText::FromName(NiagaraDynamicInputScriptName)); }
	static const FName NiagaraDynamicInputScriptName;
};

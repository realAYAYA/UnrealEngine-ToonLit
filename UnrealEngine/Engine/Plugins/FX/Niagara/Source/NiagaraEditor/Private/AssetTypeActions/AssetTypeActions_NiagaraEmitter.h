// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "NiagaraEditorModule.h"

/** Asset type actions for FNiagaraEmitter. */
class FAssetTypeActions_NiagaraEmitter: public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_NiagaraEmitter();
	virtual ~FAssetTypeActions_NiagaraEmitter();

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NiagaraEmitter", "Niagara Emitter"); }
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override { return FNiagaraEditorModule::GetAssetCategory(); }

private:
	void ExecuteNewNiagaraSystem(TArray<TWeakObjectPtr<UNiagaraEmitter>> Emitters);
	void ExecuteCreateDuplicateParent(TArray<TWeakObjectPtr<UNiagaraEmitter>> Emitters);
};
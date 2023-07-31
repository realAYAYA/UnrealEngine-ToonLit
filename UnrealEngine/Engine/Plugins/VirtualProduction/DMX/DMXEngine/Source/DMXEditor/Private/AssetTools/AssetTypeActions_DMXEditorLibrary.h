// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "DMXEditorModule.h"

class FAssetTypeActions_DMXEditorLibrary: public FAssetTypeActions_Base
{
public:
	//~ Begin IAssetTypeActions implementation
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override { return FColor(62, 140, 35); }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override { return FDMXEditorModule::GetAssetCategory(); }
	//~ End IAssetTypeActions implementation
};

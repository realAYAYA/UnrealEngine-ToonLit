// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_CSVAssetBase.h"

class UCommonGenericInputActionDataTable;

/** Asset type actions for FortHomebaseNodeGameplayEffectDataTable classes */
class FCommonAssetTypeActions_GenericInputActionDataTable : public FAssetTypeActions_CSVAssetBase
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	// End IAssetTypeActions

private:

	/** Called via context menu to open data within the data table editor */
	void OpenInDataTableEditor(TArray< TWeakObjectPtr<UCommonGenericInputActionDataTable> > Objects);
};

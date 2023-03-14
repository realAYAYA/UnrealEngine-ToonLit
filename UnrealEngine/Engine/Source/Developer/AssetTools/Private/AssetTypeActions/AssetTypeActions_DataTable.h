// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_CSVAssetBase.h"
#include "Engine/DataTable.h"


class FAssetTypeActions_DataTable : public FAssetTypeActions_CSVAssetBase
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DataTable", "Data Table"); }
	virtual UClass* GetSupportedClass() const override { return UDataTable::StaticClass(); }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
	virtual void PerformAssetDiff(UObject* Asset1, UObject* Asset2, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const override;
	virtual FText GetDisplayNameFromAssetData(const FAssetData& AssetData) const override;
	// End IAssetTypeActions

protected:
	/** Handler for when CSV is selected */
	void ExecuteExportAsCSV(TArray< TWeakObjectPtr<UObject> > Objects);

	/** Handler for when JSON is selected */
	void ExecuteExportAsJSON(TArray< TWeakObjectPtr<UObject> > Objects);
};


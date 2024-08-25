// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Modules/ModuleInterface.h"

class FExtender;
class FMenuBuilder;
struct FAssetData;

class RIGLOGICEDITOR_API FRigLogicEditor : public IModuleInterface 
{
public:
	void StartupModule() override;
	void ShutdownModule() override;

private:
	TArray<TSharedRef<class IAssetTypeActions>> AssetTypeActions;

	static TSharedRef<FExtender> OnExtendSkelMeshWithDNASelectionMenu(const TArray<FAssetData>& SelectedAssets);
	static void CreateDnaActionsSubMenu(FMenuBuilder& MenuBuilder, const TArray<FAssetData> SelectedAssets);
	static void GetDNAMenu(FMenuBuilder& MenuBuilder, const TArray<FAssetData> SelectedAssets);

	static void ExecuteDNAImport(UObject* Mesh);
	static void ExecuteDNAReimport(UObject* Mesh);

	static void GetAssetRegistryTagsForDNA(FAssetRegistryTagsContext Context);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralNetworkAssetTypeActions.h"
#include "NeuralNetwork.h"
#include "NeuralNetworkInferenceEditorModule.h"
#include "EditorFramework/AssetImportData.h"


FText FNeuralNetworkAssetTypeActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NeuralNetwork", "Neural Network");
}

FColor FNeuralNetworkAssetTypeActions::GetTypeColor() const
{
	return FColor::Red;
}

UClass* FNeuralNetworkAssetTypeActions::GetSupportedClass() const
{
	return UNeuralNetwork::StaticClass();
}

uint32 FNeuralNetworkAssetTypeActions::GetCategories()
{
	const INeuralNetworkInferenceEditorModule& NeuralNetworkInferenceEditorModule = FModuleManager::GetModuleChecked<INeuralNetworkInferenceEditorModule>("NeuralNetworkInferenceEditor");
	return NeuralNetworkInferenceEditorModule.GetMLAssetCategoryBit(); // Or EAssetTypeCategories::Misc
}

bool FNeuralNetworkAssetTypeActions::IsImportedAsset() const
{
	// Returns whether the asset was imported from an external source
	return true;
}

void FNeuralNetworkAssetTypeActions::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	// Collects the resolved source paths for the imported assets
	for (UObject* Asset : TypeAssets)
	{
		const UNeuralNetwork* const Network = CastChecked<UNeuralNetwork>(Asset);
		if (Network && Network->GetAssetImportData())
		{
			Network->GetAssetImportData()->ExtractFilenames(OutSourceFilePaths);
		}
		else
		{
			OutSourceFilePaths.Add(TEXT(""));
		}
	}
}

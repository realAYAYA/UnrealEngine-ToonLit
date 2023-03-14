// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralNetworkInferenceQAAssetFactory.h"
#include "NeuralNetworkInferenceQAAsset.h"
#include "Misc/Paths.h"



/* UNeuralNetworkInferenceQAAssetFactory structors
 *****************************************************************************/

UNeuralNetworkInferenceQAAssetFactory::UNeuralNetworkInferenceQAAssetFactory()
{
	SupportedClass = UNeuralNetworkInferenceQAAsset::StaticClass();

	bCreateNew = true;
	bEditorImport = true;
	bEditAfterNew = true;
	bText = false;
}



/* UNeuralNetworkInferenceQAAssetFactory public functions
 *****************************************************************************/

UObject* UNeuralNetworkInferenceQAAssetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn)
{
	// If created with right-click on Content Browser --> Neural Network
	return NewObject<UNeuralNetworkInferenceQAAsset>(InParent, InName, InFlags);
}

UObject* UNeuralNetworkInferenceQAAssetFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags,
	const FString& InFilename, const TCHAR* Params, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	// If created by dragging a new file into the UE Editor Content Browser
	if (InFilename.Len() > 0)
	{
		ensureMsgf(false, TEXT("No files should trigger this."));
	}

	// If created with right-click on Content Browser --> NeuralNetworkInferenceQAAsset
	return NewObject<UNeuralNetworkInferenceQAAsset>(InParent, InName, InFlags);
}

bool UNeuralNetworkInferenceQAAssetFactory::CanCreateNew() const
{
	// If true --> It will always call FactoryCreateNew(), not allowing me to use FactoryCreateFile().
	// If false --> It will ignore the FactoryCreateFile (thus the txt file) when creating a new UNeuralNetworkInferenceQAAsset.

	// Keep false while in production, set to true only when trying to create a new one
	return false; // CurrentFilename.IsEmpty();
}

bool UNeuralNetworkInferenceQAAssetFactory::DoesSupportClass(UClass * Class)
{
	return (Class == UNeuralNetworkInferenceQAAsset::StaticClass());
}

UClass* UNeuralNetworkInferenceQAAssetFactory::ResolveSupportedClass()
{
	return UNeuralNetworkInferenceQAAsset::StaticClass();
}

bool UNeuralNetworkInferenceQAAssetFactory::FactoryCanImport(const FString& InFilename)
{
	return IsValidFile(InFilename);
}

bool UNeuralNetworkInferenceQAAssetFactory::CanImportBeCanceled() const
{
	return false;
}



/* UNeuralNetworkInferenceQAAssetFactory protected functions
 *****************************************************************************/

bool UNeuralNetworkInferenceQAAssetFactory::IsValidFile(const FString& InFilename) const
{
	return false;
}

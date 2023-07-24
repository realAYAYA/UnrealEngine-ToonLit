// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralNetworkFactory.h"
#include "NeuralNetwork.h"
#include "NeuralNetworkInferenceEditorUtils.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/Paths.h"



/* UNeuralNetworkFactory structors
 *****************************************************************************/

UNeuralNetworkFactory::UNeuralNetworkFactory()
{
	SupportedClass = UNeuralNetwork::StaticClass();
	Formats.Add(TEXT("onnx;ONNX file"));
	Formats.Add(TEXT("ort;ONNX Runtime (ORT) file"));

	bCreateNew = true;
	bEditorImport = true;
	bEditAfterNew = true;
	bText = false;
}



/* UNeuralNetworkFactory public functions
 *****************************************************************************/

UObject* UNeuralNetworkFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn)
{
	// If created with right-click on Content Browser --> Neural Network
	return NewObject<UNeuralNetwork>(InParent, InName, InFlags);
}

UObject* UNeuralNetworkFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags,
	const FString& InFilename, const TCHAR* Params, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	// If created by dragging a new file into the UE Editor Content Browser
	if (InFilename.Len() > 0)
	{
		UNeuralNetwork* Network = NewObject<UNeuralNetwork>(InParent, InClass, InName, InFlags);
		// Try to load neural network from file.
		UE_LOG(LogNeuralNetworkInferenceEditor, Display, TEXT("Importing \"%s\"."), *InFilename);
		if (Network && Network->Load(InFilename))
		{
			Network->GetAssetImportData()->Update(InFilename);
			return Network;
		}
		else
		{
			UE_LOG(LogNeuralNetworkInferenceEditor, Warning, TEXT("UNeuralNetworkFactory::FactoryCreateFile(): Import failed."));
			// Invalid file or parameters.
			return nullptr;
		}
	}
	else
	{
		UE_LOG(LogNeuralNetworkInferenceEditor, Warning, TEXT("UNeuralNetworkFactory::FactoryCreateFile(): No filename provided, creating default UNeuralNetwork."));
		// If created with right-click on Content Browser --> NeuralNetwork
		return NewObject<UNeuralNetwork>(InParent, InName, InFlags);
	}
	return nullptr;
}

bool UNeuralNetworkFactory::CanCreateNew() const
{
	// If true --> It will always call FactoryCreateNew(), not allowing me to use FactoryCreateFile().
	// If false --> It will ignore the FactoryCreateFile (thus the txt file) when creating a new UNeuralNetwork.
	return CurrentFilename.IsEmpty();
}

bool UNeuralNetworkFactory::DoesSupportClass(UClass * Class)
{
	return (Class == UNeuralNetwork::StaticClass());
}

UClass* UNeuralNetworkFactory::ResolveSupportedClass()
{
	return UNeuralNetwork::StaticClass();
}

bool UNeuralNetworkFactory::FactoryCanImport(const FString& InFilename)
{
	return IsValidFile(InFilename);
}

bool UNeuralNetworkFactory::CanImportBeCanceled() const
{
	return false;
}



/* UNeuralNetworkFactory protected functions
 *****************************************************************************/

bool UNeuralNetworkFactory::IsValidFile(const FString& InFilename) const
{
	const FString FileExtension = FPaths::GetExtension(InFilename, /*bIncludeDot*/ false);
	return FileExtension.Equals(TEXT("onnx"), ESearchCase::IgnoreCase) || FileExtension.Equals(TEXT("ort"), ESearchCase::IgnoreCase);
}

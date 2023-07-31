// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeCategories.h"
#include "Modules/ModuleManager.h"

/**
 * For a general overview of NeuralNetworkInference (NNI), including documentation and code samples, @see UNeuralNetwork, the main class of NNI.
 *
 * General NNI users do not need to use this class nor any other class from the NeuralNetworkInferenceEditor (NNIEditor) module, except for
 * potentially INeuralNetworkInferenceEditorModule::GetMLAssetCategoryBit().
 *
 * INeuralNetworkInferenceEditorModule is the IModuleInterface of NeuralNetworkInferenceEditor (NNIEditor), and additionally provides the public
 * function INeuralNetworkInferenceEditorModule::GetMLAssetCategoryBit().
 */
class NEURALNETWORKINFERENCEEDITOR_API INeuralNetworkInferenceEditorModule : public IModuleInterface
{
public:
	/**
	 * It returns the EAssetTypeCategories::Type for the "Machine Learning" category. This allows for non-NNI classes to appear in the
	 * "Machine Learning" category. To see all classes on this category, right-click on the Editor's "Content Browser", then "Create Advanced Asset",
	 * and "Machine Learning". "Neural Network" and any other class registered with GetMLAssetCategoryBit() should appear in there.
	 */
	virtual EAssetTypeCategories::Type GetMLAssetCategoryBit() const = 0;
};

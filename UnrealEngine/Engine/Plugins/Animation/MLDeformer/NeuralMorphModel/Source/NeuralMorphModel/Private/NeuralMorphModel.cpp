// Copyright Epic Games, Inc. All Rights Reserved.
#include "NeuralMorphModel.h"
#include "NeuralMorphModelVizSettings.h"

#define LOCTEXT_NAMESPACE "NeuralMorphModel"

// Implement our module.
namespace UE::NeuralMorphModel
{
	class NEURALMORPHMODEL_API FNeuralMorphModelModule
		: public IModuleInterface
	{
	};
}
IMPLEMENT_MODULE(UE::NeuralMorphModel::FNeuralMorphModelModule, NeuralMorphModel)

// Our log category for this model.
NEURALMORPHMODEL_API DEFINE_LOG_CATEGORY(LogNeuralMorphModel)

//////////////////////////////////////////////////////////////////////////////

UNeuralMorphModel::UNeuralMorphModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Create the visualization settings for this model.
	// Never directly create one of the frameworks base classes such as the FMLDeformerMorphModelVizSettings as
	// that can cause issues with detail customizations.
#if WITH_EDITORONLY_DATA
	SetVizSettings(ObjectInitializer.CreateEditorOnlyDefaultSubobject<UNeuralMorphModelVizSettings>(this, TEXT("VizSettings")));
#endif
}

#undef LOCTEXT_NAMESPACE

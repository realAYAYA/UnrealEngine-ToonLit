// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralMorphEditorModel.h"
#include "NeuralMorphModel.h"
#include "MLDeformerMorphModelVizSettings.h"
#include "NeuralMorphTrainingModel.h"
#include "MLDeformerEditorToolkit.h"

#define LOCTEXT_NAMESPACE "NeuralMorphEditorModel"

namespace UE::NeuralMorphModel
{
	using namespace UE::MLDeformer;

	FMLDeformerEditorModel* FNeuralMorphEditorModel::MakeInstance()
	{
		return new FNeuralMorphEditorModel();
	}

	void FNeuralMorphEditorModel::OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent)
	{
		const FProperty* Property = PropertyChangedEvent.Property;
		if (Property == nullptr)
		{
			return;
		}

		// Process the base class property changes.
		FMLDeformerMorphModelEditorModel::OnPropertyChanged(PropertyChangedEvent);

		// Handle property changes of this model.
		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNeuralMorphModel, Mode))
		{
			if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
			{
				SetResamplingInputOutputsNeeded(true);	// Needed as local mode doesn't support curves, while the global mode does.
				GetEditor()->GetModelDetailsView()->ForceRefresh();
			}
		}
	}

	ETrainingResult FNeuralMorphEditorModel::Train()
	{
		return TrainModel<UNeuralMorphTrainingModel>(this);
	}
}	// namespace UE::NeuralMorphModel

#undef LOCTEXT_NAMESPACE

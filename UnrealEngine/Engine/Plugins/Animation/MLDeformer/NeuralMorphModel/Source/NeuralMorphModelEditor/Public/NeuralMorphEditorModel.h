// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerMorphModelEditorModel.h"

class UNeuralMorphModel;

namespace UE::NeuralMorphModel
{
	using namespace UE::MLDeformer;

	/**
	 * The editor model related to the neural morph model's runtime class (UNeuralMorphModel).
	 */
	class NEURALMORPHMODELEDITOR_API FNeuralMorphEditorModel
		: public UE::MLDeformer::FMLDeformerMorphModelEditorModel
	{
	public:
		// We need to implement this static MakeInstance method.
		static FMLDeformerEditorModel* MakeInstance();

		// FGCObject overrides.
		virtual FString GetReferencerName() const override		{ return TEXT("FNeuralMorphEditorModel"); }
		// ~END FGCObject overrides.
	
		// FMLDeformerEditorModel overrides.
		virtual ETrainingResult Train() override;
		virtual void OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent) override;
		// ~END FMLDeformerEditorModel overrides.

		/** Get a pointer to the neural morph runtime model. */
		UNeuralMorphModel* GetNeuralMorphModel() const			{ return Cast<UNeuralMorphModel>(Model); }
	};
}	// namespace UE::NeuralMorphModel

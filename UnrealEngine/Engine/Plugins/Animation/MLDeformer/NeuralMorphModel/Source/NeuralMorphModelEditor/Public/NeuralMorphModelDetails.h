// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerMorphModelDetails.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

namespace UE::NeuralMorphModel
{
	/**
	 * The detail customization for the neural morph model.
	 */
	class NEURALMORPHMODELEDITOR_API FNeuralMorphModelDetails
		: public UE::MLDeformer::FMLDeformerMorphModelDetails
	{
	public:
		/** Makes a new instance of this detail layout class for a specific detail view requesting it. */
		static TSharedRef<IDetailCustomization> MakeInstance();

		// ILayoutDetails overrides.
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
		// ~END ILayoutDetails overrides.
	};
}	// namespace UE::NeuralMorphModel

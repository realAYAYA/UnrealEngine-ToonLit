// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerMorphModelVizSettingsDetails.h"

namespace UE::NeuralMorphModel
{
	/**
	 * The neural morph model visualization settings detail customization.
	 * This is inherited from the Morph Model detail customization.
	 * We do not have to do anything else. We still need this class in order to do our
	 * detail customization correctly. Just using the Morph Model directly would cause issues.
	 */
	class NEURALMORPHMODELEDITOR_API FNeuralMorphModelVizSettingsDetails
		: public UE::MLDeformer::FMLDeformerMorphModelVizSettingsDetails
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShareable(new FNeuralMorphModelVizSettingsDetails());
		}
	};
}	// namespace UE::NeuralMorphModel

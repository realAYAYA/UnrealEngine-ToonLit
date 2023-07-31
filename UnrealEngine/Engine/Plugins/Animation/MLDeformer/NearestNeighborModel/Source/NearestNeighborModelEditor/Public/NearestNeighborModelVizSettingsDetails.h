// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerMorphModelVizSettingsDetails.h"

namespace UE::NearestNeighborModel
{
	class NEARESTNEIGHBORMODELEDITOR_API FNearestNeighborModelVizSettingsDetails
		: public UE::MLDeformer::FMLDeformerMorphModelVizSettingsDetails
	{
	public:
		/** Makes a new instance of this detail layout class for a specific detail view requesting it. */
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShareable(new FNearestNeighborModelVizSettingsDetails());
		}
	};
}	// namespace UE::NearestNeighborModel

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MLDeformerMorphModelVizSettingsDetails.h"

namespace UE::NearestNeighborModel
{
	class FNearestNeighborEditorModel;
	class NEARESTNEIGHBORMODELEDITOR_API FNearestNeighborModelVizSettingsDetails
		: public UE::MLDeformer::FMLDeformerMorphModelVizSettingsDetails
	{
	public:
		/** Makes a new instance of this detail layout class for a specific detail view requesting it. */
		static TSharedRef<IDetailCustomization> MakeInstance();
		virtual void AddAdditionalSettings() override;

	private:
		void AddTrainingMeshAdditionalSettings();
		void AddLiveAdditionalSettings();
		FNearestNeighborEditorModel* GetCastEditorModel() const;
	};
}	// namespace UE::NearestNeighborModel

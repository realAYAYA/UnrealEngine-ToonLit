// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerGeomCacheVizSettingsDetails.h"

class UMLDeformerMorphModel;
class UMLDeformerMorphModelVizSettings;

namespace UE::MLDeformer
{
	/**
	 * The visualization settings for models inherited from the UMLDeformerMorphModel class.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerMorphModelVizSettingsDetails
		: public FMLDeformerGeomCacheVizSettingsDetails
	{
	public:
		// FMLDeformerVizSettingsDetails overrides.
		virtual bool UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects) override;
		virtual void AddAdditionalSettings() override;
		// ~END FMLDeformerVizSettingsDetails overrides.

	protected:
		/** Is the morph target visualization option/checkbox enabled? */
		bool IsMorphTargetsEnabled() const;

		/** A pointer to the runtime morph model. This is updated when UpdateMemberPointers is called. */
		TObjectPtr<UMLDeformerMorphModel> MorphModel = nullptr;

		/** A pointer to the morph model visualization settings. This is updated when UpdateMemberPointers is called. */
		TObjectPtr<UMLDeformerMorphModelVizSettings> MorphModelVizSettings = nullptr;
	};
}	// namespace UE::MLDeformer

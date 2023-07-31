// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerGeomCacheModelDetails.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class UMLDeformerMorphModel;

namespace UE::MLDeformer
{
	class FMLDeformerMorphModelEditorModel;

	/**
	 * The detail customization for models inherited from the UMLDeformerMorphModel class.
	 * You can inherit the detail customization for your own model from this class if your model inherited from the UMLDeformerMorphModel class.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerMorphModelDetails
		: public FMLDeformerGeomCacheModelDetails
	{
	public:
		// ILayoutDetails overrides.
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
		// ~END ILayoutDetails overrides.

		// FMLDeformerModelDetails overrides.
		virtual void CreateCategories() override;
		virtual bool UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects) override;
		// ~END FMLDeformerModelDetails overrides.

	protected:
		/** A pointer to the morph model. This is updated when UpdateMemberPointers is called. */
		TObjectPtr<UMLDeformerMorphModel> MorphModel = nullptr;

		/** A pointer to the editor model for the morph model. This is updated when UpdateMemberPointers is called. */
		FMLDeformerMorphModelEditorModel* MorphModelEditorModel = nullptr;

		/** The morph settings category. */
		IDetailCategoryBuilder* MorphTargetCategoryBuilder = nullptr;
	};
}	// namespace UE::MLDeformer

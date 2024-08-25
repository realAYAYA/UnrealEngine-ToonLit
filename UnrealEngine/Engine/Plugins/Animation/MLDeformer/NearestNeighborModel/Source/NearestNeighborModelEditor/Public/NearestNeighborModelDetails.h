// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MLDeformerMorphModelDetails.h"

class IDetailChildrenBuilder;
class IPropertyHandle;
class UNearestNeighborModel;

namespace UE::NearestNeighborModel
{
	class FNearestNeighborEditorModel;

	class NEARESTNEIGHBORMODELEDITOR_API FNearestNeighborModelDetails
		: public ::UE::MLDeformer::FMLDeformerMorphModelDetails
	{
	public:
		/** Makes a new instance of this detail layout class for a specific detail view requesting it. */
		static TSharedRef<IDetailCustomization> MakeInstance();

		// ILayoutDetails overrides.
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
		virtual void CreateCategories() override;
		// ~END ILayoutDetails overrides.

		// FMLDeformerModelDetails overrides.
		virtual bool UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects) override;
		// ~END FMLDeformerModelDetails overrides.

	private:
		UNearestNeighborModel* GetCastModel() const;
		FNearestNeighborEditorModel* GetCastEditorModel() const;

		void CustomizeTrainingSettingsCategory() const;
		void CustomizeNearestNeighborSettingsCategory() const;
		void CustomizeSectionsCategory(IDetailLayoutBuilder& DetailBuilder);
		void CustomizeMorphTargetCategory(IDetailLayoutBuilder& DetailBuilder) const;
		void CustomizeStatusCategory() const;
		void CustomizeFileCacheCategory(IDetailLayoutBuilder& DetailBuilder) const;

		void GenerateSectionElementWidget(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder);

		IDetailCategoryBuilder* NearestNeighborCategoryBuilder = nullptr;
		IDetailCategoryBuilder* StatusCategoryBuilder = nullptr;
		IDetailCategoryBuilder* SectionsCategoryBuilder = nullptr;
	};
}	// namespace UE::NearestNeighborModel

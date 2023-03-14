// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerMorphModelDetails.h"

class UNearestNeighborModel;

namespace UE::NearestNeighborModel
{
	class FNearestNeighborEditorModel;

	class NEARESTNEIGHBORMODELEDITOR_API FNearestNeighborModelDetails
		: public UE::MLDeformer::FMLDeformerMorphModelDetails
	{
	public:
		/** Makes a new instance of this detail layout class for a specific detail view requesting it. */
		static TSharedRef<IDetailCustomization> MakeInstance();

		// ILayoutDetails overrides.
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
		// ~END ILayoutDetails overrides.

		virtual void CreateCategories() override;

		virtual bool UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects) override;

	protected:
		UNearestNeighborModel* NearestNeighborModel = nullptr;
		FNearestNeighborEditorModel* NearestNeighborEditorModel = nullptr;

		IDetailCategoryBuilder* FileCacheCategoryBuilder = nullptr;
		IDetailCategoryBuilder* ClothPartCategoryBuilder = nullptr;
		IDetailCategoryBuilder* NearestNeighborCategoryBuilder = nullptr;
		IDetailCategoryBuilder* KMeansCategoryBuilder = nullptr;
	};
}	// namespace UE::NearestNeighborModel

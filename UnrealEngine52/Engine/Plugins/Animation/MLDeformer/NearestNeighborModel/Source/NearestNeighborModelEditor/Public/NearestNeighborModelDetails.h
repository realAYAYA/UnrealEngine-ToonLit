// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerMorphModelDetails.h"

class UNearestNeighborModel;
class IPropertyHandle;
class IDetailChildrenBuilder;
class FDetailWidgetRow;

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
		virtual void CreateCategories() override;
		// ~END ILayoutDetails overrides.

		// FMLDeformerModelDetails overrides.
		virtual bool UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects) override;
		// ~END FMLDeformerModelDetails overrides.

	private:
		UNearestNeighborModel* NearestNeighborModel = nullptr;
		FNearestNeighborEditorModel* NearestNeighborEditorModel = nullptr;

		IDetailCategoryBuilder* FileCacheCategoryBuilder = nullptr;
		IDetailCategoryBuilder* ClothPartCategoryBuilder = nullptr;
		IDetailCategoryBuilder* NearestNeighborCategoryBuilder = nullptr;
		IDetailCategoryBuilder* KMeansCategoryBuilder = nullptr;

		TArray<TSharedPtr<FString> > SubMeshNames;
		TMap<TSharedPtr<FString>, int32> SubMeshNameMap;

		void AddActionResultText(IDetailCategoryBuilder* CategoryBuilder, uint8 Result, const FString& ActionName);
		void GenerateClothPartElementWidget(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder);
		void BuildSubMeshNames();
		void SubMeshComboSelectionChanged(TSharedPtr<FString> InSelectedItem, ESelectInfo::Type SelectInfo, int32 ArrayIndex);
	};
}	// namespace UE::NearestNeighborModel

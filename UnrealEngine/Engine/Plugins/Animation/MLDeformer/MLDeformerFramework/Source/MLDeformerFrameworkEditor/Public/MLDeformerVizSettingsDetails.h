// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "AssetRegistry/AssetData.h"
#include "PropertyHandle.h"

class IDetailCategoryBuilder;
class IDetailLayoutBuilder;
class USkeleton;
class UMLDeformerModel;
class UMLDeformerVizSettings;

namespace UE::MLDeformer
{
	class FMLDeformerEditorModel;

	/**
	 * The detail customization base class for any UMLDeformerVizSettings inherited object.
	 * This automatically adds the settings of the UMLDeformerVizSettings class to the UI and creates some groups, inserts error messages when needed, etc.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerVizSettingsDetails
		: public IDetailCustomization
	{
	public:
		// ILayoutDetails overrides.
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
		// ~END ILayoutDetails overrides.

		/** Get a pointer to the detail layout builder. */
		IDetailLayoutBuilder* GetDetailLayoutBuilder() const { return DetailLayoutBuilder; }

	protected:
		/** The filter that only shows anim sequences that are compatible with the given skeleton. */
		bool FilterAnimSequences(const FAssetData& AssetData, USkeleton* Skeleton);

		/** When we click the reset to default button next to the deformer graph property. */
		void OnResetToDefaultDeformerGraph(TSharedPtr<IPropertyHandle> PropertyHandle);

		/** Check if the reset to default button next to the deformer graph property should be visible or not. */
		bool IsResetToDefaultDeformerGraphVisible(TSharedPtr<IPropertyHandle> PropertyHandle);

		/**
		 * Update the class member pointers, which includes the pointer to the model, its editor model, and the viz settings.
		 * @param Objects The array of objects that the detail customization is showing.
		 */
		virtual bool UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects);

		/** Create the categories to which we can add properties. Update the member category pointers to those. */
		virtual void CreateCategories();

		/** Add some error message related to the test animation sequence. */
		virtual void AddTestSequenceErrors() {}

		/** Add some error related to the deformer graph. */
		virtual void AddDeformerGraphErrors() {}

		/** Add the ground truth test sequence property. */
		virtual void AddGroundTruth() {}

		/** Add any additional settings. */
		virtual void AddAdditionalSettings() {}

	protected:
		/** Associated detail layout builder. */
		IDetailLayoutBuilder* DetailLayoutBuilder = nullptr;

		/** The ML deformer model that these visualization settings belong to. */
		TObjectPtr<UMLDeformerModel> Model = nullptr;

		/** The visualization settings we are viewing. */
		TObjectPtr<UMLDeformerVizSettings> VizSettings = nullptr;

		/** The editor model of the runtime model. */
		FMLDeformerEditorModel* EditorModel = nullptr;

		/** A pointer to the shared settings category. */
		IDetailCategoryBuilder* SharedCategoryBuilder = nullptr;

		/** A pointer to the test assets category. */
		IDetailCategoryBuilder* TestAssetsCategory = nullptr;

		/** A pointer to the live settings category. */
		IDetailCategoryBuilder* LiveSettingsCategory = nullptr;

		/** A pointer to the training mesh category. This is only visible in training mode. */
		IDetailCategoryBuilder* TrainingMeshesCategoryBuilder = nullptr;
	};
}	// namespace UE::MLDeformer

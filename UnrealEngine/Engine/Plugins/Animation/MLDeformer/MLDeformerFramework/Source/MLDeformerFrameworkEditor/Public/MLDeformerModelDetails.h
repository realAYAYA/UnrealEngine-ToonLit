// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "IDetailCustomization.h"
#include "AssetRegistry/AssetData.h"
#include "Input/Reply.h"

class IDetailLayoutBuilder;
class USkeleton;
class IDetailCategoryBuilder;
class UMLDeformerModel;
class UGeometryCache;
class USkeletalMesh;

namespace UE::MLDeformer
{
	class FMLDeformerEditorModel;

	/**
	 * The ML Deformer model detail customization base class.
	 * This adds the shared properties, inserts error messages, creates some groups, etc.
	 * Model detail customizations should all inherit from this class. If you use a geometry cache based model however, you will 
	 * likely inherit from other classes such as FMLDeformerGeomCacheModelDetails or FMLDeformerMorphModelDetails. 
	 * Those classes are also inherited from this one though.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerModelDetails
		: public IDetailCustomization
	{
	public:
		// ILayoutDetails overrides.
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
		// ~END ILayoutDetails overrides.

		/**
		 * Update the class member pointers, which includes the pointer to the model, and its editor model.
		 * @param Objects The array of objects that the detail customization is showing.
		 */
		virtual bool UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects);

		/**
		 * Create the categories that we can add properties to. This will update the category member pointers. */
		virtual void CreateCategories();

		/**
		 * Add the target mesh property. For example the FMLDeformerGeomCacheModelDetails class will add the geometry cache property here. */
		UE_DEPRECATED(5.4, "This method will be removed. Please add your errors in your MLDeformerTrainingInputAnim type customizations instead.")
		virtual void AddTargetMesh() {}

		/** Add additional errors related to the base mesh. */
		virtual void AddBaseMeshErrors() {}

		/** Add additional errors related to the training animation sequence. */
		UE_DEPRECATED(5.4, "This method will be removed. Please add your errors in your MLDeformerTrainingInputAnim type customizations instead.")
		virtual void AddAnimSequenceErrors() {}

		/** Show errors about skeletons being incompatible. */
		UE_DEPRECATED(5.4, "This method will be removed. Please add your errors in your MLDeformerTrainingInputAnim type customizations instead.")
		virtual void AddIncompatibleSkeletonErrors() {}

		/** Add additional errors related to bone inputs. */
		virtual void AddBoneInputErrors() {}

		/** Add additional errors related to curve inputs. */
		virtual void AddCurveInputErrors() {}

		/** Add the property that shows the input animations. */
		virtual void AddTrainingInputAnims();

		/** Add training input flags, which basically are things like the check boxes that specify whether bones or curves (or both) should be included. */
		virtual void AddTrainingInputFlags() {}

		/** Add additional training input filters. Filters are things like a list of bones, or curves. */
		UE_DEPRECATED(5.4, "This method will be removed.")
		virtual void AddTrainingInputFilters() {}

		/** Add additional training input errors. */
		virtual void AddTrainingInputErrors() {}

		/** Add training settings errors. */
		virtual void AddTrainingSettingsErrors() {}

		/** Is the "include bones" checkbox visible? */
		UE_DEPRECATED(5.4, "This method will be removed.")
		virtual bool IsBonesFlagVisible() const { return true; }

		/** Is the "include curves" checkbox visible? */
		UE_DEPRECATED(5.4, "This method will be removed.")
		virtual bool IsCurvesFlagVisible() const { return true; }

	protected:
		/** The filter that only shows anim sequences that are compatible with the given skeleton. */
		bool FilterAnimSequences(const FAssetData& AssetData, USkeleton* Skeleton);

		/** Executed when we press the Animated Bones Only button. */
		UE_DEPRECATED(5.4, "This method will be removed.")
		FReply OnFilterAnimatedBonesOnly() const { return FReply::Handled(); }

		/** Executed when we press the Animated Curves Only button. */
		UE_DEPRECATED(5.4, "This method will be removed.")
		FReply OnFilterAnimatedCurvesOnly() const { return FReply::Handled(); }

	protected:
		/** Associated detail layout builder. */
		IDetailLayoutBuilder* DetailLayoutBuilder = nullptr;

		/** A pointer to the model we are customizing details for. */
		TObjectPtr<UMLDeformerModel> Model = nullptr;

		/** A pointer to the editor model of the runtime model. */
		FMLDeformerEditorModel* EditorModel = nullptr;

		/** The category related to the base mesh. */
		IDetailCategoryBuilder* BaseMeshCategoryBuilder = nullptr;

		/** The category related to the target mesh. */
		IDetailCategoryBuilder* TargetMeshCategoryBuilder_DEPRECATED = nullptr;

		/** The category related to the inputs and outputs. */
		IDetailCategoryBuilder* InputOutputCategoryBuilder = nullptr;

		/** The training settings category. You most likely add your model properties to this. */
		IDetailCategoryBuilder* TrainingSettingsCategoryBuilder = nullptr;

		/** The LOD generation category. This category can be hidden, based on whether the model supports LOD or not. */
		IDetailCategoryBuilder* LODSettingsCategoryBuilder = nullptr;
	};
}	// namespace UE::MLDeformer

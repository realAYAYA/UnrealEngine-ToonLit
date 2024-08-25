// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerMorphModelDetails.h"
#include "MLDeformerMorphModelEditorModel.h"
#include "MLDeformerMorphModel.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "SWarningOrErrorBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "MLDeformerMorphModelDetails"

namespace UE::MLDeformer
{
	bool FMLDeformerMorphModelDetails::UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects)
	{
		if (!FMLDeformerGeomCacheModelDetails::UpdateMemberPointers(Objects))
		{
			return false;
		}

		MorphModel = Cast<UMLDeformerMorphModel>(Model);
		check(MorphModel);
		MorphModelEditorModel = static_cast<FMLDeformerMorphModelEditorModel*>(EditorModel);

		return (MorphModel != nullptr && MorphModelEditorModel != nullptr);
	}

	void FMLDeformerMorphModelDetails::CreateCategories()
	{
		FMLDeformerGeomCacheModelDetails::CreateCategories();
		MorphTargetCategoryBuilder = &DetailLayoutBuilder->EditCategory("Morph Targets", FText::GetEmpty(), ECategoryPriority::Important);
	}

	void FMLDeformerMorphModelDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
	{
		// Create all the detail categories and add the properties of the base class.
		FMLDeformerGeomCacheModelDetails::CustomizeDetails(DetailBuilder);

		MorphTargetCategoryBuilder->AddProperty(DetailLayoutBuilder->GetProperty(UMLDeformerMorphModel::GetClampMorphTargetWeightsPropertyName(), UMLDeformerMorphModel::StaticClass()))
			.Visibility(MorphModelEditorModel->IsMorphWeightClampingSupported() ? EVisibility::Visible : EVisibility::Collapsed);

		MorphTargetCategoryBuilder->AddProperty(DetailLayoutBuilder->GetProperty(UMLDeformerMorphModel::GetIncludeMorphTargetNormalsPropertyName(), UMLDeformerMorphModel::StaticClass()));

		IDetailGroup& CompressionGroup = MorphTargetCategoryBuilder->AddGroup("Compression", LOCTEXT("MorphCompressionGroupLabel", "Compression"), false, true);
		CompressionGroup.AddPropertyRow(DetailLayoutBuilder->GetProperty(UMLDeformerMorphModel::GetMorphDeltaZeroThresholdPropertyName(), UMLDeformerMorphModel::StaticClass()));
		CompressionGroup.AddPropertyRow(DetailLayoutBuilder->GetProperty(UMLDeformerMorphModel::GetMorphCompressionLevelPropertyName(), UMLDeformerMorphModel::StaticClass()));

		IDetailGroup& MaskGroup = MorphTargetCategoryBuilder->AddGroup("Mask", LOCTEXT("MorphMaskGroupLabel", "Masking"), false, false);
		MaskGroup.AddPropertyRow(DetailLayoutBuilder->GetProperty(UMLDeformerMorphModel::GetMaskChannelPropertyName(), UMLDeformerMorphModel::StaticClass()));
		MaskGroup.AddPropertyRow(DetailLayoutBuilder->GetProperty(UMLDeformerMorphModel::GetInvertMaskChannelPropertyName(), UMLDeformerMorphModel::StaticClass()));

		if (MorphModel && MorphModel->HasRawMorph() && !MorphModel->CanDynamicallyUpdateMorphTargets())
		{
			const FText DeltaCountMismatchErrorText = LOCTEXT("MorphDeltaCountMismatch", "Dynamic morph target updates disabled until retrained. This is because the vertex count changed after the model was trained.");
			FDetailWidgetRow& DeltaMismatchErrorRow = MorphTargetCategoryBuilder->AddCustomRow(FText::FromString("MorphDeltaCountMismatchError"))
				.WholeRowContent()
				[
					SNew(SBox)
					.Padding(FMargin(0.0f, 4.0f))
					[
						SNew(SWarningOrErrorBox)
						.MessageStyle(EMessageStyle::Warning)
						.Message(DeltaCountMismatchErrorText)
					]
				];
		}

		MorphTargetCategoryBuilder->AddCustomRow(LOCTEXT("FinalizeMorphTargetsButton", "Finalize Morph Targets"))
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(2, 2)
				.MaxDesiredWidth(200.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("FinalizeMorphTargetsButtonText", "Finalize Morph Targets"))
					.ToolTipText(LOCTEXT("FinalizeMorphTargetsButtonTooltip", "Delete raw data and fianlize the morph targets. This will reduce disk size but will make morph data ineditable. Re-training is required to edit the morph targets again."))
					.HAlign(HAlign_Center)
					.OnClicked_Lambda([this]() -> FReply
					{
						if (MorphModel)
						{
							MorphModel->FinalizeMorphTargets();
						}
						return FReply::Handled();
					})
				]
			];
	}
}	// namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE

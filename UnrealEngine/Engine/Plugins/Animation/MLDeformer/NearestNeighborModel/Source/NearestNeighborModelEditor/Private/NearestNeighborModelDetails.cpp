// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborModelDetails.h"
#include "NearestNeighborEditorModel.h"
#include "NearestNeighborModel.h"
#include "MLDeformerEditorToolkit.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "SWarningOrErrorBox.h"
#include "IDetailGroup.h"
#include "IDetailsView.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "NearestNeighborModelDetails"

namespace UE::NearestNeighborModel
{
	using namespace UE::MLDeformer;

	TSharedRef<IDetailCustomization> FNearestNeighborModelDetails::MakeInstance()
	{
		return MakeShareable(new FNearestNeighborModelDetails());
	}

	bool FNearestNeighborModelDetails::UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects)
	{
		if (!FMLDeformerGeomCacheModelDetails::UpdateMemberPointers(Objects))
		{
			return false;
		}

		NearestNeighborModel = static_cast<UNearestNeighborModel*>(Model);
		check(NearestNeighborModel);
		NearestNeighborEditorModel = static_cast<FNearestNeighborEditorModel*>(EditorModel);

		return (NearestNeighborModel != nullptr && NearestNeighborEditorModel != nullptr);
	}

	void FNearestNeighborModelDetails::CreateCategories()
	{
		FMLDeformerMorphModelDetails::CreateCategories();
		FileCacheCategoryBuilder = &DetailLayoutBuilder->EditCategory("File Cache", FText::GetEmpty(), ECategoryPriority::Important);
		ClothPartCategoryBuilder = &DetailLayoutBuilder->EditCategory("Cloth Parts", FText::GetEmpty(), ECategoryPriority::Important);
		NearestNeighborCategoryBuilder = &DetailLayoutBuilder->EditCategory("Nearest Neighbors", FText::GetEmpty(), ECategoryPriority::Important);
		KMeansCategoryBuilder = &DetailLayoutBuilder->EditCategory("KMeans Pose Generator", FText::GetEmpty(), ECategoryPriority::Important);

		// Add warning in CreateCategories so that the warning appears at the top of the details panel.
		FDetailWidgetRow& NearestNeighborWarningRow = BaseMeshCategoryBuilder->AddCustomRow(FText::FromString("NearestNeighborWarning"))
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Warning)
					.Message(LOCTEXT("NearestNeighborWarning", "Nearest neighbor model is still experimental and the details here are subject to change."))
				]
			];
	}

	void FNearestNeighborModelDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
	{
		// Create all the detail categories and add the properties of the base class.
		FMLDeformerMorphModelDetails::CustomizeDetails(DetailBuilder);

		// Training settings.
		TrainingSettingsCategoryBuilder->AddProperty(UNearestNeighborModel::GetInputDimPropertyName());
		TrainingSettingsCategoryBuilder->AddProperty(UNearestNeighborModel::GetHiddenLayerDimsPropertyName());
		TrainingSettingsCategoryBuilder->AddProperty(UNearestNeighborModel::GetOutputDimPropertyName());
		TrainingSettingsCategoryBuilder->AddProperty(UNearestNeighborModel::GetNumEpochsPropertyName());
		TrainingSettingsCategoryBuilder->AddProperty(UNearestNeighborModel::GetBatchSizePropertyName());
		TrainingSettingsCategoryBuilder->AddProperty(UNearestNeighborModel::GetLearningRatePropertyName());
		TrainingSettingsCategoryBuilder->AddProperty(UNearestNeighborModel::GetSavedNetworkSizePropertyName());

		// File cache settings
		IDetailGroup *Group = &FileCacheCategoryBuilder->AddGroup(TEXT("File Cache"), LOCTEXT("File Cache", "File Cache"), false, true);
		Group->HeaderProperty(DetailBuilder.GetProperty(UNearestNeighborModel::GetUseFileCachePropertyName()));
		Group->AddPropertyRow(DetailBuilder.GetProperty(UNearestNeighborModel::GetFileCacheDirectoryPropertyName()));
		Group->AddPropertyRow(DetailBuilder.GetProperty(UNearestNeighborModel::GetRecomputeDeltasPropertyName()));
		Group->AddPropertyRow(DetailBuilder.GetProperty(UNearestNeighborModel::GetRecomputePCAPropertyName()));

		// Cloth part settings
		ClothPartCategoryBuilder->AddProperty(UNearestNeighborModel::GetClothPartEditorDataPropertyName());

		FString VertexCountStr = "VertexCounts:";
		if (NearestNeighborModel)
		{
			const int32 NumParts = NearestNeighborModel->GetNumParts();
			VertexCountStr += "[";
			for (int32 PartId = 0; PartId < NumParts; PartId++)
			{
				VertexCountStr += FString::Printf(TEXT("%d"), NearestNeighborModel->GetPartNumVerts(PartId));
				if (PartId != NumParts - 1)
				{
					VertexCountStr += ",";
				}
			}
			VertexCountStr += "]";
		}
		FText VertexCountText = FText::FromString(VertexCountStr);
		ClothPartCategoryBuilder->AddCustomRow(FText::FromString(""))
			.WholeRowContent()
			[
				SNew(STextBlock)
				.Text(VertexCountText)
			];


		FText ButtonText = (NearestNeighborModel && NearestNeighborModel->IsClothPartDataValid()) ? LOCTEXT("Update", "Update") : LOCTEXT("Update *", "Update *");
		ClothPartCategoryBuilder->AddCustomRow(FText::FromString(""))
			.WholeRowContent()
			[
				SNew(SButton)
				.Text(ButtonText)
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]
				{
					if (NearestNeighborModel != nullptr)
					{
						NearestNeighborModel->UpdateClothPartData();
						NearestNeighborModel->InitPreviousWeights();
						if (NearestNeighborEditorModel != nullptr)
						{
							NearestNeighborEditorModel->UpdateNearestNeighborActors();
						}
						EditorModel->GetEditor()->GetModelDetailsView()->ForceRefresh();
					}
					return FReply::Handled();
				})
			];

		// Nearest Neighbor settings
		NearestNeighborCategoryBuilder->AddProperty(GET_MEMBER_NAME_STRING_CHECKED(UNearestNeighborModel, DecayFactor));
		NearestNeighborCategoryBuilder->AddProperty(GET_MEMBER_NAME_STRING_CHECKED(UNearestNeighborModel, NearestNeighborOffsetWeight));
		NearestNeighborCategoryBuilder->AddProperty(UNearestNeighborModel::GetUsePartOnlyMeshPropertyName());
		NearestNeighborCategoryBuilder->AddProperty(UNearestNeighborModel::GetNearestNeighborDataPropertyName());
		ButtonText = NearestNeighborModel->IsNearestNeighborDataValid() ? LOCTEXT("Update", "Update") : LOCTEXT("Update *", "Update *");

		NearestNeighborCategoryBuilder->AddCustomRow(FText::FromString(""))
			.WholeRowContent()
			[
				SNew(SButton)
				.Text(ButtonText)
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]
				{
					if (NearestNeighborEditorModel != nullptr)
					{
						NearestNeighborEditorModel->UpdateNearestNeighborData();
						NearestNeighborModel->InitPreviousWeights();
						EditorModel->GetEditor()->GetModelDetailsView()->ForceRefresh();
					}
					return FReply::Handled();
				})
			];

		KMeansCategoryBuilder->AddProperty(GET_MEMBER_NAME_STRING_CHECKED(UNearestNeighborModel, SourceSkeletons));
		KMeansCategoryBuilder->AddProperty(GET_MEMBER_NAME_STRING_CHECKED(UNearestNeighborModel, NumClusters));
		KMeansCategoryBuilder->AddCustomRow(FText::FromString(""))
			.WholeRowContent()
			[
				SNew(SButton)
				.Text(FText::FromString("Cluster"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]
				{
					NearestNeighborEditorModel->KMeansClusterPoses();
					return FReply::Handled();
				})
			];

		MorphTargetCategoryBuilder->AddProperty(UNearestNeighborModel::GetMorphDataSizePropertyName());
		MorphTargetCategoryBuilder->AddCustomRow(FText::FromString(""))
			.WholeRowContent()
			[
				SNew(SButton)
				.Text(FText::FromString("Update"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]
				{
					if (NearestNeighborEditorModel != nullptr)
					{
						NearestNeighborEditorModel->InitMorphTargets();
						NearestNeighborEditorModel->RefreshMorphTargets();
						NearestNeighborModel->UpdateNetworkSize();
						NearestNeighborModel->UpdateMorphTargetSize();
						EditorModel->GetEditor()->GetModelDetailsView()->ForceRefresh();
					}
					return FReply::Handled();
				})
			];
	}
}	// namespace UE::NearestNeighborModel

#undef LOCTEXT_NAMESPACE

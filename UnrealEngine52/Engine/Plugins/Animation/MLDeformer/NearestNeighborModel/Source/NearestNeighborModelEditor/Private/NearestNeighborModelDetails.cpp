// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborModelDetails.h"
#include "NearestNeighborEditorModel.h"
#include "NearestNeighborModel.h"
#include "MLDeformerEditorToolkit.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/SkeletalMesh.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "IDetailsView.h"
#include "PropertyCustomizationHelpers.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "SWarningOrErrorBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

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
		FMLDeformerGeomCacheModelDetails::CreateCategories();

		FileCacheCategoryBuilder = &DetailLayoutBuilder->EditCategory("File Cache", FText::GetEmpty(), ECategoryPriority::Important);
		ClothPartCategoryBuilder = &DetailLayoutBuilder->EditCategory("Cloth Parts", FText::GetEmpty(), ECategoryPriority::Important);
		NearestNeighborCategoryBuilder = &DetailLayoutBuilder->EditCategory("Nearest Neighbors", FText::GetEmpty(), ECategoryPriority::Important);
		MorphTargetCategoryBuilder = &DetailLayoutBuilder->EditCategory("Morph Targets", FText::GetEmpty(), ECategoryPriority::Important);
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


	void FNearestNeighborModelDetails::GenerateClothPartElementWidget(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
	{
		TSharedPtr<IPropertyHandle> PCACoeffNumPropertyHandle = PropertyHandle->GetChildHandle(TEXT("PCACoeffNum"));
		TSharedPtr<IPropertyHandle> VertexMapPathHandle = PropertyHandle->GetChildHandle(TEXT("VertexMapPath"));

		typename STextComboBox::FOnTextSelectionChanged SubMeshComboOnSelectionChangedDelegate;
		SubMeshComboOnSelectionChangedDelegate.BindRaw(this, &FNearestNeighborModelDetails::SubMeshComboSelectionChanged, ArrayIndex);
		int32 InitMeshIndex = NearestNeighborModel ? NearestNeighborModel->GetPartMeshIndex(ArrayIndex) : 0;
		InitMeshIndex = InitMeshIndex >= SubMeshNames.Num() ? 0 : InitMeshIndex;

		ChildrenBuilder.AddCustomRow(FText::GetEmpty())
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.Padding(5, 2)
				.FillWidth(0.3f)
				[
					PropertyHandle->CreatePropertyNameWidget()
				]

				+SHorizontalBox::Slot()
				.Padding(5, 2)
				.FillWidth(0.7f)
				[
					PropertyHandle->CreatePropertyValueWidget()
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.Padding(5, 2)
				.FillWidth(0.3f)
				[
					PCACoeffNumPropertyHandle->CreatePropertyNameWidget()
				]

				+SHorizontalBox::Slot()
				.Padding(5, 2)
				.FillWidth(0.7f)
				[
					PCACoeffNumPropertyHandle->CreatePropertyValueWidget()
				]

			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.Padding(5, 2)
				.FillWidth(0.3f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Submesh")))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+SHorizontalBox::Slot()
				.Padding(5, 2)
				.FillWidth(0.7f)
				[
					SNew(STextComboBox)
					.OptionsSource(&SubMeshNames)
					.OnSelectionChanged(SubMeshComboOnSelectionChangedDelegate)
					.InitiallySelectedItem(SubMeshNames[InitMeshIndex])
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.Padding(5, 2)
				.FillWidth(0.3f)		
				[
					VertexMapPathHandle->CreatePropertyNameWidget()
				]

				+SHorizontalBox::Slot()
				.Padding(5, 2)
				.FillWidth(0.7f)
				[
					VertexMapPathHandle->CreatePropertyValueWidget()
				]
			]
		];
	}

	void FNearestNeighborModelDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
	{
		// Create all the detail categories and add the properties of the base class.
		FMLDeformerGeomCacheModelDetails::CustomizeDetails(DetailBuilder);

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

		if (NearestNeighborModel == nullptr)
		{
			return;
		}

		BuildSubMeshNames();
		const int32 MaxPartMeshIndex = NearestNeighborModel->GetMaxPartMeshIndex();
		if (MaxPartMeshIndex >= SubMeshNames.Num())
		{
			UE_LOG(LogNearestNeighborModel, Error, TEXT("Nearest neighbor model was previously created with %d submeshes, but the current skeletal mesh has %d submeshes. Please use the original skeletal mesh or click update to overwrite existing data."), MaxPartMeshIndex + 1, SubMeshNames.Num());

			NearestNeighborModel->InvalidateClothPartData();
			AddActionResultText(ClothPartCategoryBuilder, EUpdateResult::ERROR, TEXT("Loading"));
		}
		TSharedRef<IPropertyHandle> ClothPartDataPropertyHandle = DetailBuilder.GetProperty(UNearestNeighborModel::GetClothPartEditorDataPropertyName());
		if (ClothPartDataPropertyHandle->AsArray().IsValid() && !SubMeshNames.IsEmpty())
		{
			TSharedRef<FDetailArrayBuilder> PropertyBuilder = MakeShared<FDetailArrayBuilder>(ClothPartDataPropertyHandle, true, false, true);
			PropertyBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FNearestNeighborModelDetails::GenerateClothPartElementWidget));
			ClothPartCategoryBuilder->AddCustomBuilder(PropertyBuilder);
		}

		// Nearest Neighbor settings
		NearestNeighborCategoryBuilder->AddProperty(GET_MEMBER_NAME_STRING_CHECKED(UNearestNeighborModel, DecayFactor));
		NearestNeighborCategoryBuilder->AddProperty(GET_MEMBER_NAME_STRING_CHECKED(UNearestNeighborModel, NearestNeighborOffsetWeight));
		NearestNeighborCategoryBuilder->AddProperty(UNearestNeighborModel::GetUsePartOnlyMeshPropertyName());
		NearestNeighborCategoryBuilder->AddProperty(UNearestNeighborModel::GetNearestNeighborDataPropertyName());

		KMeansCategoryBuilder->AddProperty(GET_MEMBER_NAME_STRING_CHECKED(UNearestNeighborModel, SourceAnims));
		KMeansCategoryBuilder->AddProperty(GET_MEMBER_NAME_STRING_CHECKED(UNearestNeighborModel, NumClusters));
		KMeansCategoryBuilder->AddProperty(GET_MEMBER_NAME_STRING_CHECKED(UNearestNeighborModel, KMeansPartId));
		KMeansCategoryBuilder->AddCustomRow(FText::FromString(""))
			.WholeRowContent()
			[
				SNew(SButton)
				.Text(FText::FromString("Cluster"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]
				{
					NearestNeighborEditorModel->KMeansClusterPoses();
					EditorModel->GetEditor()->GetModelDetailsView()->ForceRefresh();
					if (NearestNeighborEditorModel->GetKMeansClusterResult() == EUpdateResult::SUCCESS)
					{
						UE_LOG(LogNearestNeighborModel, Display, TEXT("Cluster succeeded."));
					}
					return FReply::Handled();
				})
			];
				
		MorphTargetCategoryBuilder->AddProperty(UMLDeformerMorphModel::GetMorphDeltaZeroThresholdPropertyName(), UMLDeformerMorphModel::StaticClass());
		MorphTargetCategoryBuilder->AddProperty(UMLDeformerMorphModel::GetMorphCompressionLevelPropertyName(), UMLDeformerMorphModel::StaticClass());
		FText ButtonText = NearestNeighborModel->IsMorphTargetDataValid() ? LOCTEXT("Update", "Update") : LOCTEXT("Update *", "Update *");
		MorphTargetCategoryBuilder->AddProperty(UNearestNeighborModel::GetMorphDataSizePropertyName());
		MorphTargetCategoryBuilder->AddCustomRow(FText::FromString(""))
			.WholeRowContent()
			[
				SNew(SButton)
				.Text(ButtonText)
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([this]
				{
					NearestNeighborEditorModel->OnMorphTargetUpdate();
					EditorModel->GetEditor()->GetModelDetailsView()->ForceRefresh();
					if (NearestNeighborEditorModel->GetMorphTargetUpdateResult() == EUpdateResult::SUCCESS)
					{
						UE_LOG(LogNearestNeighborModel, Display, TEXT("Update succeeded."));
					}
					return FReply::Handled();
				})
			];
		AddActionResultText(MorphTargetCategoryBuilder, NearestNeighborEditorModel->GetMorphTargetUpdateResult(), TEXT("Update"));
		AddActionResultText(KMeansCategoryBuilder, NearestNeighborEditorModel->GetKMeansClusterResult(), TEXT("KMeans"));
	}

	void FNearestNeighborModelDetails::AddActionResultText(IDetailCategoryBuilder* CategoryBuilder, uint8 Result, const FString& ActionName)
	{
		CategoryBuilder->AddCustomRow(FText::FromString("UpdateResultError"))
			.Visibility((Result & EUpdateResult::ERROR) != 0 ? EVisibility::Visible : EVisibility::Collapsed)
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Error)
					.Message(FText::FromString(ActionName + TEXT(" failed with errors. Please check Output Log (LogNearestNeighborModel, LogPython) for details.")))
				]
			];
		CategoryBuilder->AddCustomRow(FText::FromString("UpdateResultWarning"))
			.Visibility((Result & EUpdateResult::WARNING) != 0 ? EVisibility::Visible : EVisibility::Collapsed)
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Warning)
					.Message(FText::FromString(ActionName + TEXT(" finished with warnings. Please check Output Log (LogNearestNeighborModel, LogPython) for details.")))
				]
			];
	}

	void FNearestNeighborModelDetails::BuildSubMeshNames()
	{
		SubMeshNames.Reset();
		SubMeshNameMap.Reset();
		if (NearestNeighborModel && NearestNeighborModel->GetSkeletalMesh() && NearestNeighborModel->GetSkeletalMesh()->GetImportedModel())
		{
			const FSkeletalMeshLODModel& LODModel =  NearestNeighborModel->GetSkeletalMesh()->GetImportedModel()->LODModels[0];
			const TArray<FSkelMeshImportedMeshInfo>& SkelMeshInfos = LODModel.ImportedMeshInfos;
			for (int32 i = 0; i < SkelMeshInfos.Num(); i++)
			{
				TSharedPtr<FString> StrPtr = MakeShared<FString>(SkelMeshInfos[i].Name.ToString());
				SubMeshNames.Add(StrPtr);
				SubMeshNameMap.Add(StrPtr, i);
			}
		}
	}

	void FNearestNeighborModelDetails::SubMeshComboSelectionChanged(TSharedPtr<FString> InSelectedItem, ESelectInfo::Type SelectInfo, int32 ArrayIndex)
	{
		if (NearestNeighborModel)
		{
			NearestNeighborModel->SetPartMeshIndex(ArrayIndex, SubMeshNameMap[InSelectedItem]);
		}
	}
}	// namespace UE::NearestNeighborModel

#undef LOCTEXT_NAMESPACE

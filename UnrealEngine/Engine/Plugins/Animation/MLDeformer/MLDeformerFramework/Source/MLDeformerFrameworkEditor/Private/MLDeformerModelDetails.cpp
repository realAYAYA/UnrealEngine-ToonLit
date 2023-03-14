// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerModelDetails.h"
#include "MLDeformerModule.h"
#include "MLDeformerAsset.h"
#include "MLDeformerModel.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerEditorModel.h"
#include "MLDeformerEditorModule.h"
#include "MLDeformerGeomCacheHelpers.h"
#include "NeuralNetwork.h"
#include "GeometryCache.h"
#include "GeometryCacheTrack.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "PropertyCustomizationHelpers.h"
#include "AssetRegistry/AssetData.h"
#include "Modules/ModuleManager.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "IDetailGroup.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "DetailLayoutBuilder.h"
#include "SWarningOrErrorBox.h"

#define LOCTEXT_NAMESPACE "MLDeformerModelDetails"

namespace UE::MLDeformer
{
	bool FMLDeformerModelDetails::UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects)
	{
		Model = nullptr;
		EditorModel = nullptr;
		if (Objects.Num() == 1 && Objects[0] != nullptr)
		{
			Model = static_cast<UMLDeformerModel*>(Objects[0].Get());

			// Get the editor model for this runtime model.
			FMLDeformerEditorModule& EditorModule = FModuleManager::LoadModuleChecked<FMLDeformerEditorModule>("MLDeformerFrameworkEditor");
			if (Model)
			{
				EditorModel = EditorModule.GetModelRegistry().GetEditorModel(Model);
			}
		}

		return (Model != nullptr && EditorModel != nullptr);
	}

	void FMLDeformerModelDetails::CreateCategories()
	{
		BaseMeshCategoryBuilder = &DetailLayoutBuilder->EditCategory("Base Mesh", FText::GetEmpty(), ECategoryPriority::Important);
		TargetMeshCategoryBuilder = &DetailLayoutBuilder->EditCategory("Target Mesh", FText::GetEmpty(), ECategoryPriority::Important);
		InputOutputCategoryBuilder = &DetailLayoutBuilder->EditCategory("Inputs and Output", FText::GetEmpty(), ECategoryPriority::Important);
		TrainingSettingsCategoryBuilder = &DetailLayoutBuilder->EditCategory("Training Settings", FText::GetEmpty(), ECategoryPriority::Important);
	}

	void FMLDeformerModelDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
	{
		DetailLayoutBuilder = &DetailBuilder;

		// Update the pointers and check if they are valid.
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailBuilder.GetObjectsBeingCustomized(Objects);
		if (!UpdateMemberPointers(Objects))
		{
			return;
		}

		CreateCategories();

		// Base mesh details.
		BaseMeshCategoryBuilder->AddProperty(UMLDeformerModel::GetSkeletalMeshPropertyName(), UMLDeformerModel::StaticClass());

		AddBaseMeshErrors();

		// Check if the vertex counts of our asset have changed.
		const FText ChangedErrorText = EditorModel->GetBaseAssetChangedErrorText();
		FDetailWidgetRow& ChangedErrorRow = BaseMeshCategoryBuilder->AddCustomRow(FText::FromString("BaseMeshChangedError"))
			.Visibility(!ChangedErrorText.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed)
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Warning)
					.Message(ChangedErrorText)
				]
			];

		// Check if our skeletal mesh's imported model contains a list of mesh infos. If not, we need to reimport it as it is an older asset.
		const FText NeedsReimportErrorText = EditorModel->GetSkeletalMeshNeedsReimportErrorText();
		FDetailWidgetRow& NeedsReimportErrorRow = BaseMeshCategoryBuilder->AddCustomRow(FText::FromString("BaseMeshNeedsReimportError"))
			.Visibility(!NeedsReimportErrorText.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed)
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Error)
					.Message(NeedsReimportErrorText)
				]
			];

		const FText VertexMapMisMatchErrorText = EditorModel->GetVertexMapChangedErrorText();
		FDetailWidgetRow& VertexMapErrorRow = BaseMeshCategoryBuilder->AddCustomRow(FText::FromString("VertexMapError"))
			.Visibility(!VertexMapMisMatchErrorText.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed)
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Error)
					.Message(VertexMapMisMatchErrorText)
				]
			];

		// Animation sequence.
		IDetailPropertyRow& AnimRow = BaseMeshCategoryBuilder->AddProperty(UMLDeformerModel::GetAnimSequencePropertyName(), UMLDeformerModel::StaticClass());
		AnimRow.CustomWidget()
		.NameContent()
		[
			AnimRow.GetPropertyHandle()->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SObjectPropertyEntryBox)
			.PropertyHandle(AnimRow.GetPropertyHandle())
			.AllowedClass(UAnimSequence::StaticClass())
			.ObjectPath(Model ? Model->GetAnimSequence()->GetPathName() : FString())
			.ThumbnailPool(DetailBuilder.GetThumbnailPool())
			.OnShouldFilterAsset(
				this, 
				&FMLDeformerModelDetails::FilterAnimSequences, 
				Model->GetSkeletalMesh() ? Model->GetSkeletalMesh()->GetSkeleton() : nullptr
			)
		];

		AddAnimSequenceErrors();

		const FText AnimErrorText = EditorModel->GetIncompatibleSkeletonErrorText(Model->GetSkeletalMesh(), Model->GetAnimSequence());
		FDetailWidgetRow& AnimErrorRow = BaseMeshCategoryBuilder->AddCustomRow(FText::FromString("AnimSkeletonMisMatchError"))
			.Visibility(!AnimErrorText.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed)
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Warning)
					.Message(AnimErrorText)
				]
			];


		AddTargetMesh();

		TargetMeshCategoryBuilder->AddProperty(UMLDeformerModel::GetAlignmentTransformPropertyName(), UMLDeformerModel::StaticClass());

		InputOutputCategoryBuilder->AddProperty(UMLDeformerModel::GetShouldIncludeBonesPropertyName(), UMLDeformerModel::StaticClass())
			.Visibility(IsBonesFlagVisible() ? EVisibility::Visible : EVisibility::Collapsed);

		InputOutputCategoryBuilder->AddProperty(UMLDeformerModel::GetShouldIncludeCurvesPropertyName(), UMLDeformerModel::StaticClass())
			.Visibility(IsCurvesFlagVisible() ? EVisibility::Visible : EVisibility::Collapsed);

		AddTrainingInputFlags();
		AddTrainingInputErrors();

		const FText ErrorText = EditorModel->GetInputsErrorText();
		FDetailWidgetRow& ErrorRow = InputOutputCategoryBuilder->AddCustomRow(FText::FromString("InputsError"))
			.Visibility(!ErrorText.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed)
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Error)
					.Message(ErrorText)
				]
			];

		InputOutputCategoryBuilder->AddProperty(UMLDeformerModel::GetMaxTrainingFramesPropertyName(), UMLDeformerModel::StaticClass());
		InputOutputCategoryBuilder->AddProperty(UMLDeformerModel::GetDeltaCutoffLengthPropertyName(), UMLDeformerModel::StaticClass());

		// Bone include list group.
		if (Model->DoesSupportBones())
		{
			const int NumBones = EditorModel->GetEditorInputInfo() ? EditorModel->GetEditorInputInfo()->GetNumBones() : 0;
			const FText BonesText = FText::Format(FTextFormat(LOCTEXT("BonesGroupName", "Bones ({0})")), NumBones);;
			FText AllBonesText;
			if (Model->GetSkeletalMesh() && (Model->GetSkeletalMesh()->GetRefSkeleton().GetNum() == NumBones || NumBones == 0))
			{
				AllBonesText = LOCTEXT("BoneGroupValue", "All Bones Included");
			}
			IDetailGroup& BoneIncludeGroup = InputOutputCategoryBuilder->AddGroup("BoneIncludeGroup", FText(), false, false);
			BoneIncludeGroup.HeaderRow()
			.NameContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text(BonesText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.Text(AllBonesText)
					.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				]
			];
			BoneIncludeGroup.AddWidgetRow()
				.ValueContent()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Text(LOCTEXT("AnimatedBonesButton", "Animated Bones Only"))
					.ToolTipText(LOCTEXT("AnimatedBonesButtonTooltip", "Initialize the bone include list to bones that are animated."))
					.OnClicked(FOnClicked::CreateSP(this, &FMLDeformerModelDetails::OnFilterAnimatedBonesOnly))
					.IsEnabled_Lambda([this](){ return Model->ShouldIncludeBonesInTraining(); })
				];
			BoneIncludeGroup.AddPropertyRow(DetailBuilder.GetProperty(UMLDeformerModel::GetBoneIncludeListPropertyName(), UMLDeformerModel::StaticClass()));

			AddBoneInputErrors();
		}
		else
		{
			InputOutputCategoryBuilder->AddProperty(UMLDeformerModel::GetBoneIncludeListPropertyName(), UMLDeformerModel::StaticClass())
				.Visibility(EVisibility::Collapsed);
		}

		// Curve include list group.
		if (Model->DoesSupportCurves())
		{
			const int NumCurves = EditorModel->GetEditorInputInfo() ? EditorModel->GetEditorInputInfo()->GetNumCurves() : 0;
			const FText CurvesText = FText::Format(FTextFormat(LOCTEXT("CurvesGroupName", "Curves ({0})")), NumCurves);
			FText AllCurvesText;
			const int32 NumCurvesOnSkelMesh = EditorModel->GetNumCurvesOnSkeletalMesh(Model->GetSkeletalMesh());
			if (NumCurvesOnSkelMesh == 0)
			{
				AllCurvesText = LOCTEXT("CurvesGroupValueNoCurves", "No Curves Found");
			}
			else if (NumCurves == NumCurvesOnSkelMesh)
			{
				AllCurvesText = LOCTEXT("CurvesGroupValue", "All Curves Included");
			}
			IDetailGroup& CurvesIncludeGroup = InputOutputCategoryBuilder->AddGroup("CurveIncludeGroup", LOCTEXT("CurveIncludeGroup", "Curves"), false, false);
			CurvesIncludeGroup.HeaderRow()
			.NameContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text(CurvesText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.Text(AllCurvesText)
					.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				]
			];

			CurvesIncludeGroup.AddWidgetRow()
				.ValueContent()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Text(LOCTEXT("AnimatedCurvesButton", "Animated Curves Only"))
					.ToolTipText(LOCTEXT("AnimatedCurvesButtonTooltip", "Initialize the curve include list to curves that are animated."))
					.OnClicked(FOnClicked::CreateSP(this, &FMLDeformerModelDetails::OnFilterAnimatedCurvesOnly))
					.IsEnabled_Lambda([this](){ return Model->ShouldIncludeCurvesInTraining() && EditorModel->GetNumCurvesOnSkeletalMesh(Model->GetSkeletalMesh()) > 0; })
				];
			CurvesIncludeGroup.AddPropertyRow(DetailBuilder.GetProperty(UMLDeformerModel::GetCurveIncludeListPropertyName(), UMLDeformerModel::StaticClass()));

			AddCurveInputErrors();
		}
		else
		{
			InputOutputCategoryBuilder->AddProperty(UMLDeformerModel::GetCurveIncludeListPropertyName(), UMLDeformerModel::StaticClass())
				.Visibility(EVisibility::Collapsed);
		}

		AddTrainingInputFilters();

		// Show a warning when no neural network has been set.
		{		
			UNeuralNetwork* NeuralNetwork = Model->GetNeuralNetwork();
			FDetailWidgetRow& NeuralNetErrorRow = TrainingSettingsCategoryBuilder->AddCustomRow(FText::FromString("NeuralNetError"))
				.Visibility((NeuralNetwork == nullptr) ? EVisibility::Visible : EVisibility::Collapsed)
				.WholeRowContent()
				[
					SNew(SBox)
					.Padding(FMargin(0.0f, 4.0f))
					[
						SNew(SWarningOrErrorBox)
						.MessageStyle(EMessageStyle::Warning)
						.Message(FText::FromString("Model still needs to be trained."))
					]
				];

			// Check if our network is compatible with the skeletal mesh.
			if (Model->GetSkeletalMesh() && NeuralNetwork)
			{
				FDetailWidgetRow& NeuralNetIncompatibleErrorRow = TrainingSettingsCategoryBuilder->AddCustomRow(FText::FromString("NeuralNetIncompatibleError"))
					.Visibility(!Model->GetInputInfo()->IsCompatible(Model->GetSkeletalMesh()) ? EVisibility::Visible : EVisibility::Collapsed)
					.WholeRowContent()
					[
						SNew(SBox)
						.Padding(FMargin(0.0f, 4.0f))
						[
							SNew(SWarningOrErrorBox)
							.MessageStyle(EMessageStyle::Error)
							.Message(FText::FromString("Trained neural network is incompatible with selected SkeletalMesh."))
						]
					];
			}
		}
	}

	bool FMLDeformerModelDetails::FilterAnimSequences(const FAssetData& AssetData, USkeleton* Skeleton)
	{
		if (Skeleton && Skeleton->IsCompatibleSkeletonByAssetData(AssetData))
		{
			return false;
		}

		return true;
	}

	FReply FMLDeformerModelDetails::OnFilterAnimatedBonesOnly() const
	{
		EditorModel->InitBoneIncludeListToAnimatedBonesOnly();
		DetailLayoutBuilder->ForceRefreshDetails();
		return FReply::Handled();
	}

	FReply FMLDeformerModelDetails::OnFilterAnimatedCurvesOnly() const
	{
		EditorModel->InitCurveIncludeListToAnimatedCurvesOnly();
		DetailLayoutBuilder->ForceRefreshDetails();
		return FReply::Handled();
	}

	bool FMLDeformerModelDetails::IsBonesFlagVisible() const
	{
		return (Model->DoesSupportBones() && Model->DoesSupportCurves());
	}

	bool FMLDeformerModelDetails::IsCurvesFlagVisible() const
	{
		return (Model->DoesSupportBones() && Model->DoesSupportCurves() && EditorModel->GetNumCurvesOnSkeletalMesh(Model->GetSkeletalMesh()) > 0);
	}
}	// namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE

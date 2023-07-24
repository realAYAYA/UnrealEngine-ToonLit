// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralMorphEditorModel.h"
#include "IDetailsView.h"
#include "NeuralMorphModel.h"
#include "NeuralMorphNetwork.h"
#include "NeuralMorphInputInfo.h"
#include "NeuralMorphTrainingModel.h"
#include "MLDeformerMorphModelVizSettings.h"
#include "MLDeformerEditorToolkit.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "BoneContainer.h"

#define LOCTEXT_NAMESPACE "NeuralMorphEditorModel"

namespace UE::NeuralMorphModel
{
	using namespace UE::MLDeformer;

	FMLDeformerEditorModel* FNeuralMorphEditorModel::MakeInstance()
	{
		return new FNeuralMorphEditorModel();
	}

	void FNeuralMorphEditorModel::OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent)
	{
		const FProperty* Property = PropertyChangedEvent.Property;
		if (Property == nullptr)
		{
			return;
		}

		// Process the base class property changes.
		FMLDeformerMorphModelEditorModel::OnPropertyChanged(PropertyChangedEvent);

		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNeuralMorphModel, BoneGroups) ||
			Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNeuralMorphModel, CurveGroups) ||
			Property->GetFName() == TEXT("BoneNames") ||	// FNeuralMorphBoneGroup::BoneNames has items added or removed (it is an array).
			Property->GetFName() == TEXT("CurveNames") ||	// FNeuralMorphCurveGroup::CurveNames has items added or removed (it is an array).
			Property->GetFName() == TEXT("BoneName") ||		// The bone name inside one of the items in the BoneNames list changed.
			Property->GetFName() == TEXT("CurveName"))		// The curve name inside one of the items in the CurveNames list changed.
		{
			UpdateIsReadyForTrainingState();
		}
		else if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNeuralMorphModel, Mode))
		{
			if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
			{
				UpdateIsReadyForTrainingState();
				GetEditor()->GetModelDetailsView()->ForceRefresh();
			}
		}
	}

	bool FNeuralMorphEditorModel::IsTrained() const
	{
		UNeuralMorphNetwork* MorphNetwork = GetNeuralMorphModel()->GetNeuralMorphNetwork();
		if (MorphNetwork != nullptr && MorphNetwork->GetMainMLP() != nullptr)
		{
			return true;
		}

		return false;
	}

	ETrainingResult FNeuralMorphEditorModel::Train()
	{
		return TrainModel<UNeuralMorphTrainingModel>(this);
	}

	bool FNeuralMorphEditorModel::LoadTrainedNetwork() const
	{
		// Load the specialized neural morph model network.
		// Base the filename on the onnx filename, and replace the file extension.
		FString NetworkFilename = GetTrainedNetworkOnnxFile();
		NetworkFilename.RemoveFromEnd(TEXT("onnx"));
		NetworkFilename += TEXT("nmn");

		// Load the actual network.
		UNeuralMorphNetwork* NeuralNet = NewObject<UNeuralMorphNetwork>(Model);
		if (!NeuralNet->Load(NetworkFilename))
		{
			UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to load neural morph network from file '%s'!"), *NetworkFilename);
			NeuralNet->ConditionalBeginDestroy();

			// Restore the deltas to the ones before training started.
			GetMorphModel()->SetMorphTargetDeltas(MorphTargetDeltasBackup);
			return false;
		}

		if (NeuralNet->IsEmpty())
		{
			NeuralNet->ConditionalBeginDestroy();
			NeuralNet = nullptr;
		}

		GetNeuralMorphModel()->SetNeuralMorphNetwork(NeuralNet);	// Use our custom inference.
		return true;
	}

	void FNeuralMorphEditorModel::InitInputInfo(UMLDeformerInputInfo* InputInfo)
	{
		FMLDeformerEditorModel::InitInputInfo(InputInfo);

		UNeuralMorphInputInfo* NeuralInputInfo = Cast<UNeuralMorphInputInfo>(InputInfo);
		if (NeuralInputInfo == nullptr)
		{
			return;
		}

		UNeuralMorphModel* NeuralMorphModel = Cast<UNeuralMorphModel>(Model);
		if (NeuralMorphModel->GetModelMode() != ENeuralMorphMode::Local)
		{
			return;
		}

		const bool bIncludeBones = Model->DoesSupportBones() && Model->ShouldIncludeBonesInTraining();
		const bool bIncludeCurves = Model->DoesSupportCurves() && Model->ShouldIncludeCurvesInTraining();
		const USkeleton* Skeleton = Model->GetSkeletalMesh() ? Model->GetSkeletalMesh()->GetSkeleton() : nullptr;
		const USkeletalMesh* SkeletalMesh = Model->GetSkeletalMesh();

		// Handle bones.
		if (bIncludeBones && SkeletalMesh)
		{
			const TArray<FNeuralMorphBoneGroup>& ModelBoneGroups = NeuralMorphModel->GetBoneGroups();
			const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
			for (int32 BoneGroupIndex = 0; BoneGroupIndex < ModelBoneGroups.Num(); ++BoneGroupIndex)
			{
				const FNeuralMorphBoneGroup& BoneGroup = ModelBoneGroups[BoneGroupIndex];
				if (BoneGroup.BoneNames.IsEmpty())
				{
					continue;
				}

				NeuralInputInfo->GetBoneGroups().AddDefaulted();
				FNeuralMorphBoneGroup& NewGroup = NeuralInputInfo->GetBoneGroups().Last();

				const int32 NumBonesInGroup = BoneGroup.BoneNames.Num();
				NewGroup.BoneNames.AddDefaulted(NumBonesInGroup);
				for (int32 Index = 0; Index < NumBonesInGroup; ++Index)
				{
					const FBoneReference& BoneRef = BoneGroup.BoneNames[Index];
					if (!BoneRef.BoneName.IsValid() || BoneRef.BoneName.IsNone())
					{
						UE_LOG(LogNeuralMorphModel, Warning, TEXT("Invalid or 'None' bone detected inside curve group %d, please fix this."), BoneGroupIndex);
						continue;
					}

					if (RefSkeleton.FindBoneIndex(BoneRef.BoneName) == INDEX_NONE)
					{
						UE_LOG(LogNeuralMorphModel, Warning, TEXT("Bone '%s' inside bone group %d doesn't exist, please fix this."), *BoneRef.BoneName.ToString(), BoneGroupIndex);
						continue;
					}

					if (!InputInfo->GetBoneNames().Contains(BoneRef.BoneName))
					{
						UE_LOG(LogNeuralMorphModel, Warning, TEXT("Bone '%s' inside bone group %d isn't included in the bone list that are input to the model."), *BoneRef.BoneName.ToString(), BoneGroupIndex);
						continue;
					}

					NewGroup.BoneNames[Index] = BoneRef.BoneName;
				}
			}
		}

		// Handle curves.
		if (bIncludeCurves && SkeletalMesh)
		{
			const FSmartNameMapping* SmartNameMapping = Skeleton ? Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName) : nullptr;
			if (SmartNameMapping) // When there are curves.
			{
				const TArray<FNeuralMorphCurveGroup>& ModelCurveGroups = NeuralMorphModel->GetCurveGroups();
				const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
				for (int32 CurveGroupIndex = 0; CurveGroupIndex < ModelCurveGroups.Num(); ++CurveGroupIndex)
				{				
					const FNeuralMorphCurveGroup& CurveGroup = ModelCurveGroups[CurveGroupIndex];
					if (CurveGroup.CurveNames.IsEmpty())
					{
						continue;
					}

					NeuralInputInfo->GetCurveGroups().AddDefaulted();
					FNeuralMorphCurveGroup& NewGroup = NeuralInputInfo->GetCurveGroups().Last();

					const int32 NumCurvesInGroup = CurveGroup.CurveNames.Num();
					NewGroup.CurveNames.AddDefaulted(NumCurvesInGroup);
					for (int32 Index = 0; Index < NumCurvesInGroup; ++Index)
					{
						const FMLDeformerCurveReference& CurveRef = CurveGroup.CurveNames[Index];
						if (!CurveRef.CurveName.IsValid() || CurveRef.CurveName.IsNone())
						{
							UE_LOG(LogNeuralMorphModel, Warning, TEXT("Invalid or 'None' curve detected inside curve group %d, please fix this."), CurveGroupIndex);
							continue;
						}

						if (!SmartNameMapping->Exists(CurveRef.CurveName))
						{
							UE_LOG(LogNeuralMorphModel, Warning, TEXT("Curve '%s' inside curve group %d doesn't exist, please fix this."), *CurveRef.CurveName.ToString(), CurveGroupIndex);
							continue;
						}

						if (!InputInfo->GetCurveNames().Contains(CurveRef.CurveName))
						{
							UE_LOG(LogNeuralMorphModel, Warning, TEXT("Curve '%s' inside curve group %d isn't included in the curve list that are input to the model."), *CurveRef.CurveName.ToString(), CurveGroupIndex);
							continue;
						}

						NewGroup.CurveNames[Index] = CurveRef.CurveName;
					}
				}
			} // if SmartNameMapping
		}
	}

	void FNeuralMorphEditorModel::UpdateIsReadyForTrainingState()
	{
		FMLDeformerMorphModelEditorModel::UpdateIsReadyForTrainingState();
		UNeuralMorphInputInfo* Info = Cast<UNeuralMorphInputInfo>(GetEditorInputInfo());
		if (Info)
		{
			if (GetNeuralMorphModel()->GetModelMode() == ENeuralMorphMode::Local)
			{
				bIsReadyForTraining &= !Info->HasInvalidGroups();
			}
		}
	}

	FText FNeuralMorphEditorModel::GetOverlayText() const
	{
		FText Text = FMLDeformerMorphModelEditorModel::GetOverlayText();
		UNeuralMorphInputInfo* Info = Cast<UNeuralMorphInputInfo>(GetEditorInputInfo());
		if (GetNeuralMorphModel()->GetModelMode() == ENeuralMorphMode::Local && Info->HasInvalidGroups())
		{
			Text = FText::Format(
				LOCTEXT("GroupErrorFormat", "{0}\n{1}"), 
				Text,
				LOCTEXT("GroupErrorText", "Bone and curve groups require at least two valid items.\nCheck the log warnings for more information."));
		}
		return Text;
	}
}	// namespace UE::NeuralMorphModel

#undef LOCTEXT_NAMESPACE

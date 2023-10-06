// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralMorphTrainingModel.h"
#include "NeuralMorphEditorModel.h"
#include "NeuralMorphModel.h"
#include "NeuralMorphInputInfo.h"

int32 UNeuralMorphTrainingModel::GetNumBoneGroups() const
{
	UNeuralMorphInputInfo* InputInfo = Cast<UNeuralMorphInputInfo>(EditorModel->GetEditorInputInfo());
	return InputInfo ? InputInfo->GetBoneGroups().Num() : 0;
}

int32 UNeuralMorphTrainingModel::GetNumCurveGroups() const
{
	UNeuralMorphInputInfo* InputInfo = Cast<UNeuralMorphInputInfo>(EditorModel->GetEditorInputInfo());	
	return InputInfo ? InputInfo->GetCurveGroups().Num() : 0;
}

TArray<int32> UNeuralMorphTrainingModel::GenerateBoneGroupIndices() const
{
	UNeuralMorphInputInfo* InputInfo = Cast<UNeuralMorphInputInfo>(EditorModel->GetEditorInputInfo());
	if (InputInfo == nullptr)
	{
		return TArray<int32>();
	}

	TArray<int32> OutBoneGroupIndices;
	InputInfo->GenerateBoneGroupIndices(OutBoneGroupIndices);
	return MoveTemp(OutBoneGroupIndices);
}

TArray<int32> UNeuralMorphTrainingModel::GenerateCurveGroupIndices() const
{
	UNeuralMorphInputInfo* InputInfo = Cast<UNeuralMorphInputInfo>(EditorModel->GetEditorInputInfo());
	if (InputInfo == nullptr)
	{
		return TArray<int32>();
	}

	TArray<int32> OutCurveGroupIndices;
	InputInfo->GenerateCurveGroupIndices(OutCurveGroupIndices);
	return MoveTemp(OutCurveGroupIndices);
}

TArray<float> UNeuralMorphTrainingModel::GetMorphTargetMasks() const
{
	using namespace UE::NeuralMorphModel;
	FNeuralMorphEditorModel* NeuralMorphEditorModel = static_cast<FNeuralMorphEditorModel*>(EditorModel);
	UNeuralMorphModel* NeuralMorphModel = NeuralMorphEditorModel->GetNeuralMorphModel();
	UNeuralMorphInputInfo* NeuralInputInfo = Cast<UNeuralMorphInputInfo>(NeuralMorphEditorModel->GetEditorInputInfo());
	if (NeuralMorphModel->GetModelMode() != ENeuralMorphMode::Local || !NeuralMorphModel->IsBoneMaskingEnabled())
	{
		NeuralInputInfo->GetInputItemMaskBuffer().Empty();
	}
	else	
	{
		NeuralMorphEditorModel->RebuildEditorMaskInfo();
	}

	return NeuralInputInfo->GetInputItemMaskBuffer();
}

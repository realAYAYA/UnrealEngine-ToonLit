// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerTrainingModel.h"
#include "MLDeformerEditorModel.h"
#include "MLDeformerModel.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerSampler.h"

UMLDeformerModel* UMLDeformerTrainingModel::GetModel() const
{
	return EditorModel->GetModel();
}

void UMLDeformerTrainingModel::Init(UE::MLDeformer::FMLDeformerEditorModel* InEditorModel)
{
	EditorModel = InEditorModel;
	ResetSampling();
}

void UMLDeformerTrainingModel::SetEditorModel(UE::MLDeformer::FMLDeformerEditorModel* InModel)
{ 
	EditorModel = InModel;
}

UE::MLDeformer::FMLDeformerEditorModel* UMLDeformerTrainingModel::GetEditorModel() const
{
	return EditorModel;
}

int32 UMLDeformerTrainingModel::GetNumberSampleTransforms() const
{
	return EditorModel->GetEditorInputInfo()->GetNumBones();
}

int32 UMLDeformerTrainingModel::GetNumberSampleCurves() const
{
	return EditorModel->GetEditorInputInfo()->GetNumCurves();
}

int32 UMLDeformerTrainingModel::NumSamples() const
{
	return EditorModel->GetNumFramesForTraining();
}

void UMLDeformerTrainingModel::ResetSampling()
{
	NumTimesSampled.Reset();
	NumTimesSampled.AddZeroed(EditorModel->GetNumTrainingInputAnims());
	SampleAnimIndex = 0;
	bFinishedSampling = !FindNextAnimToSample(SampleAnimIndex);
}

int32 UMLDeformerTrainingModel::GetNumberSampleDeltas() const
{
	return EditorModel->GetEditorInputInfo()->GetNumBaseMeshVertices();
}

void UMLDeformerTrainingModel::SetNumFloatsPerCurve(int32 NumFloatsPerCurve)
{
	const int32 NumAnims = EditorModel->GetNumTrainingInputAnims();
	for (int32 AnimIndex = 0; AnimIndex < NumAnims; ++AnimIndex)
	{
		UE::MLDeformer::FMLDeformerSampler* Sampler = EditorModel->GetSamplerForTrainingAnim(AnimIndex);
		if (Sampler)
		{
			Sampler->SetNumFloatsPerCurve(NumFloatsPerCurve);
		}
	}
}

bool UMLDeformerTrainingModel::SetCurrentSampleIndex(int32 Index)
{
	return NextSample();
}

bool UMLDeformerTrainingModel::GetNeedsResampling() const
{
	return EditorModel->GetResamplingInputOutputsNeeded();
}

void UMLDeformerTrainingModel::SetNeedsResampling(bool bNeedsResampling)
{
	EditorModel->SetResamplingInputOutputsNeeded(bNeedsResampling);
}

bool UMLDeformerTrainingModel::NextSample()
{
	return SampleNextFrame();
}

bool UMLDeformerTrainingModel::SampleNextFrame()
{
	UE_LOG(LogMLDeformer, Warning, TEXT("Please override the SampleNextFrame method in your UMLDeformerTrainingModel inherited class."));
	return false;
}

bool UMLDeformerTrainingModel::SampleFrame(int32 Index)
{
	UE_LOG(LogMLDeformer, Warning, TEXT("Please use UMLDeformerTrainingModel::NextSample() instead."));
	return false;
}

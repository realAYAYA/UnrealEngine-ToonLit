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
	return GetModel()->GetInputInfo()->GetNumBones();
}

int32 UMLDeformerTrainingModel::GetNumberSampleCurves() const
{
	return GetModel()->GetInputInfo()->GetNumCurves();
}

int32 UMLDeformerTrainingModel::NumSamples() const
{
	return EditorModel->GetNumFramesForTraining();
}

int32 UMLDeformerTrainingModel::GetNumberSampleDeltas() const
{
	return GetModel()->GetNumBaseMeshVerts();
}

bool UMLDeformerTrainingModel::SetCurrentSampleIndex(int32 Index)
{
	return SampleFrame(Index);
}

bool UMLDeformerTrainingModel::GetNeedsResampling() const
{
	return EditorModel->GetResamplingInputOutputsNeeded();
}

void UMLDeformerTrainingModel::SetNeedsResampling(bool bNeedsResampling)
{
	EditorModel->SetResamplingInputOutputsNeeded(bNeedsResampling);
}

bool UMLDeformerTrainingModel::SampleFrame(int32 Index)
{
	using namespace UE::MLDeformer;

	// Make sure we have a valid frame number.
	if (Index < 0 || Index >= EditorModel->GetNumTrainingFrames())
	{
		UE_LOG(LogMLDeformer, Warning, TEXT("Sample index must range from %d to %d, but a value of %d was provided."), 0, EditorModel->GetNumTrainingFrames()-1, Index);
		return false;
	}

	// Sample the frame.
	FMLDeformerSampler* Sampler = EditorModel->GetSampler();
	Sampler->SetVertexDeltaSpace(EVertexDeltaSpace::PreSkinning);
	Sampler->Sample(Index);

	// Copy sampled values.
	SampleDeltas = Sampler->GetVertexDeltas();
	SampleBoneRotations = Sampler->GetBoneRotations();
	SampleCurveValues = Sampler->GetCurveValues();

	return true;
}

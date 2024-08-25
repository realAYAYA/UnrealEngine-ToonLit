// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerGeomCacheTrainingModel.h"
#include "MLDeformerGeomCacheTrainingInputAnim.h"
#include "MLDeformerGeomCacheEditorModel.h"
#include "MLDeformerGeomCacheModel.h"

void UMLDeformerGeomCacheTrainingModel::Init(UE::MLDeformer::FMLDeformerEditorModel* InEditorModel)
{
	Super::Init(InEditorModel);

	// Find the first valid input anim index to sample from.
	// This modifies the SampleAnimIndex value.
	bFinishedSampling = !FindNextAnimToSample(SampleAnimIndex);
}

bool UMLDeformerGeomCacheTrainingModel::FindNextAnimToSample(int32& OutNextAnimIndex) const
{
	using namespace UE::MLDeformer;

	UMLDeformerGeomCacheModel* GeomCacheModel = Cast<UMLDeformerGeomCacheModel>(EditorModel->GetModel());
	int32 NumTries = 0;
	int32 AnimIndex = SampleAnimIndex;

	const int32 NumInputAnims = EditorModel->GetNumTrainingInputAnims();
	while (NumTries < NumInputAnims)	// Try all input animations at worst case.
	{
		const FMLDeformerTrainingInputAnim* InputAnim = EditorModel->GetTrainingInputAnim(AnimIndex);
		if (InputAnim && InputAnim->IsEnabled())
		{
			if (NumTimesSampled[AnimIndex] < InputAnim->GetNumFramesToSample())
			{
				OutNextAnimIndex = AnimIndex;
				return true;
			}
		}

		// Get the next animation index.
		AnimIndex++;
		AnimIndex %= EditorModel->GetNumTrainingInputAnims();

		NumTries++;
	}

	OutNextAnimIndex = INDEX_NONE;
    return false;
}

bool UMLDeformerGeomCacheTrainingModel::SampleNextFrame()
{
	using namespace UE::MLDeformer;

	// Make sure that there is more left to sample.
	if (bFinishedSampling)
	{
		return false;
	}

	FMLDeformerGeomCacheEditorModel* GeomEditorModel = static_cast<FMLDeformerGeomCacheEditorModel*>(EditorModel);
	UMLDeformerGeomCacheModel* GeomCacheModel = Cast<UMLDeformerGeomCacheModel>(EditorModel->GetModel());

	// Get the animation to sample from and validate some things.
	const FMLDeformerGeomCacheTrainingInputAnim& InputAnim = GeomCacheModel->GetTrainingInputAnims()[SampleAnimIndex];
	check(InputAnim.IsEnabled());
	check(InputAnim.GetAnimSequence());
	check(InputAnim.GetGeometryCache());

	const int32 StartFrame = InputAnim.GetUseCustomRange() ? FMath::Min<int32>(InputAnim.GetStartFrame(), InputAnim.GetEndFrame()) : 0;
	const int32 CurFrameToSample = StartFrame + NumTimesSampled[SampleAnimIndex];
	check(CurFrameToSample < StartFrame + InputAnim.GetNumFramesToSample());	// We should never sample more frames than the animation has.
	NumTimesSampled[SampleAnimIndex]++;

	// Perform the actual sampling.
	FMLDeformerSampler* Sampler = EditorModel->GetSamplerForTrainingAnim(SampleAnimIndex);
	Sampler->SetVertexDeltaSpace(EVertexDeltaSpace::PreSkinning);
	Sampler->Sample(CurFrameToSample);
	UE_LOG(LogMLDeformer, Display, TEXT("Sampling frame %d of anim %d"), CurFrameToSample, SampleAnimIndex);

	// Copy sampled values.
	SampleDeltas = Sampler->GetVertexDeltas();
	SampleBoneRotations = Sampler->GetBoneRotations();
	SampleCurveValues = Sampler->GetCurveValues();

	// Now find the next sample we should take.
	// This will return false when we finished sampling everything.
	SampleAnimIndex++;
	SampleAnimIndex %= GeomCacheModel->GetTrainingInputAnims().Num();
	if (!FindNextAnimToSample(SampleAnimIndex))
	{
		bFinishedSampling = true;
		return false;
	}

	// Let the caller know we can still sample more.
	return true;
}

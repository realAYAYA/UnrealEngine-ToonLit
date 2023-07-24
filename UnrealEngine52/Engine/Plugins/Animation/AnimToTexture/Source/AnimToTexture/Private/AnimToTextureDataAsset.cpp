// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimToTextureDataAsset.h"

int32 UAnimToTextureDataAsset::GetIndexFromAnimSequence(const UAnimSequence* Sequence)
{
	int32 OutIndex = 0;

	int32 NumSequences = AnimSequences.Num();
	
	// We can store a sequence to index map for a faster search
	for (int32 CurrentIndex = 0; CurrentIndex < NumSequences; ++CurrentIndex)
	{
		const FAnimSequenceInfo& SequenceInfo = AnimSequences[CurrentIndex];
		if (SequenceInfo.AnimSequence == Sequence)
		{
			OutIndex = CurrentIndex;
			break;
		}
	}

	return OutIndex;
}

void UAnimToTextureDataAsset::ResetInfo()
{
	// Common Info.
	NumFrames = 0;
	Animations.Reset();

	// Vertex Info
	VertexRowsPerFrame = 1;
	VertexMinBBox = FVector::ZeroVector;
	VertexSizeBBox = FVector::ZeroVector;

	// Bone Info
	BoneRowsPerFrame = 1;
	BoneWeightRowsPerFrame = 1;
	BoneMinBBox = FVector::ZeroVector;
	BoneSizeBBox = FVector::ZeroVector;
};
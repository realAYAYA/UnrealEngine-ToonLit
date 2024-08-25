// Copyright Epic Games, Inc. All Rights Reserved.

#include "TransformArrayOperations.h"
#include "TransformArray.h"

namespace UE::AnimNext
{
	void SetIdentity(const FTransformArrayAoSView& Dest, bool bIsAdditive)
	{
		const int32 NumTransforms = Dest.Num();
		const FTransform Identity = bIsAdditive ? TransformAdditiveIdentity : FTransform::Identity;

		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			Dest[TransformIndex] = Identity;
		}
	}

	void CopyTransforms(const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source, int32 StartIndex, int32 NumToCopy)
	{
		const int32 NumTransforms = Dest.Num();
		const int32 EndIndex = NumToCopy >= 0 ? (StartIndex + NumToCopy) : NumTransforms;

		check(Source.Num() >= NumTransforms);
		check(StartIndex >= 0 && StartIndex < NumTransforms);
		check(EndIndex <= NumTransforms);

		for (int32 TransformIndex = StartIndex; TransformIndex < EndIndex; ++TransformIndex)
		{
			Dest[TransformIndex] = Source[TransformIndex];
		}
	}

	void NormalizeRotations(const FTransformArrayAoSView& Input)
	{
		const int32 NumTransforms = Input.Num();

		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			Input[TransformIndex].NormalizeRotation();
		}
	}

	void BlendWithIdentityAndAccumulate(const FTransformArrayAoSView& Base, const FTransformArrayAoSConstView& Additive, const float BlendWeight)
	{
		const ScalarRegister VBlendWeight(BlendWeight);
		const int32 NumTransforms = Base.Num();

		check(Additive.Num() >= NumTransforms);

		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			FTransform::BlendFromIdentityAndAccumulate(Base[TransformIndex], Additive[TransformIndex], VBlendWeight);
		}
	}

	void BlendOverwriteWithScale(const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source, const float ScaleWeight)
	{
		const ScalarRegister VScaleWeight(ScaleWeight);
		const int32 NumTransforms = Source.Num();

		check(Dest.Num() >= NumTransforms);

		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			Dest[TransformIndex] = Source[TransformIndex] * VScaleWeight;
		}
	}

	void BlendAddWithScale(const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source, const float ScaleWeight)
	{
		const ScalarRegister VScaleWeight(ScaleWeight);
		const int32 NumTransforms = Source.Num();

		check(Dest.Num() >= NumTransforms);

		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			Dest[TransformIndex].AccumulateWithShortestRotation(Source[TransformIndex], VScaleWeight);
		}
	}

	void BlendOverwritePerBoneWithScale(
		const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source,
		const TArrayView<const int32>& LODBoneIndexToWeightIndexMap, const TArrayView<const float>& BoneWeights, const float DefaultScaleWeight)
	{
		const ScalarRegister VDefaultScaleWeight(DefaultScaleWeight);
		const int32 NumTransforms = Source.Num();

		check(Dest.Num() >= NumTransforms);

		for (int32 LODBoneIndex = 0; LODBoneIndex < NumTransforms; ++LODBoneIndex)
		{
			const int32 PerBoneIndex = LODBoneIndexToWeightIndexMap[LODBoneIndex];
			const ScalarRegister VScaleWeight = BoneWeights.IsValidIndex(PerBoneIndex) ? ScalarRegister(BoneWeights[PerBoneIndex]) : VDefaultScaleWeight;
			Dest[LODBoneIndex] = Source[LODBoneIndex] * VScaleWeight;
		}
	}

	void BlendAddPerBoneWithScale(
		const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source,
		const TArrayView<const int32>& LODBoneIndexToWeightIndexMap, const TArrayView<const float>& BoneWeights, const float DefaultScaleWeight)
	{
		const ScalarRegister VDefaultScaleWeight(DefaultScaleWeight);
		const int32 NumTransforms = Source.Num();

		check(Dest.Num() >= NumTransforms);

		for (int32 LODBoneIndex = 0; LODBoneIndex < NumTransforms; ++LODBoneIndex)
		{
			const int32 PerBoneIndex = LODBoneIndexToWeightIndexMap[LODBoneIndex];
			const ScalarRegister VScaleWeight = BoneWeights.IsValidIndex(PerBoneIndex) ? ScalarRegister(BoneWeights[PerBoneIndex]) : VDefaultScaleWeight;
			Dest[LODBoneIndex].AccumulateWithShortestRotation(Source[LODBoneIndex], VScaleWeight);
		}
	}
}

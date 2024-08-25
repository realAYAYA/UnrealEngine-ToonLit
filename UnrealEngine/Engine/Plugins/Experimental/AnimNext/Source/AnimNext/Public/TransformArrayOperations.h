// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TransformArrayView.h"

namespace UE::AnimNext
{
	// Sets the identity (regular or additive) over the whole destination array
	ANIMNEXT_API void SetIdentity(const FTransformArrayAoSView& Dest, bool bIsAdditive);
	ANIMNEXT_API void SetIdentity(const FTransformArraySoAView& Dest, bool bIsAdditive);

	// Copies the specified number of transforms from a source into a destination starting at the specified start index
	// If NumToCopy is -1, we copy until the end
	ANIMNEXT_API void CopyTransforms(const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source, int32 StartIndex = 0, int32 NumToCopy = -1);
	ANIMNEXT_API void CopyTransforms(const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source, int32 StartIndex = 0, int32 NumToCopy = -1);

	// Normalizes rotations in input transform view
	ANIMNEXT_API void NormalizeRotations(const FTransformArrayAoSView& Input);
	ANIMNEXT_API void NormalizeRotations(const FTransformArraySoAView& Input);

	// The additive transform view is blended with the additive identity using the provided blend weight
	// We then accumulate the resulting transforms on top of the base transforms
	// Delta = Blend(Identity, Additive, BlendWeight)
	// Base.Accumulate(Delta);
	ANIMNEXT_API void BlendWithIdentityAndAccumulate(const FTransformArrayAoSView& Base, const FTransformArrayAoSConstView& Additive, const float BlendWeight);
	ANIMNEXT_API void BlendWithIdentityAndAccumulate(const FTransformArraySoAView& Base, const FTransformArraySoAConstView& Additive, const float BlendWeight);

	// The source transforms are scaled by the provided weight and the result is written in the destination
	// Dest = Source * ScaleWeight
	ANIMNEXT_API void BlendOverwriteWithScale(const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source, const float ScaleWeight);
	ANIMNEXT_API void BlendOverwriteWithScale(const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source, const float ScaleWeight);

	// The source transforms are scaled by the provided weight and the result is added to the destination
	// Dest = Dest + (Source * ScaleWeight)
	ANIMNEXT_API void BlendAddWithScale(const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source, const float ScaleWeight);
	ANIMNEXT_API void BlendAddWithScale(const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source, const float ScaleWeight);

	// The source transforms are scaled by the provided per bone weight and the result is written in the destination
	// Dest = Source * (WeightIndex != INDEX_NONE ? Weights[WeightIndex] : DefaultScaleWeight)
	ANIMNEXT_API void BlendOverwritePerBoneWithScale(
		const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source,
		const TArrayView<const int32>& LODBoneIndexToWeightIndexMap, const TArrayView<const float>& BoneWeights, const float DefaultScaleWeight);
	ANIMNEXT_API void BlendOverwritePerBoneWithScale(
		const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source,
		const TArrayView<const int32>& LODBoneIndexToWeightIndexMap, const TArrayView<const float>& BoneWeights, const float DefaultScaleWeight);

	// The source transforms are scaled by the provided per bone weight and the result is added to the destination
	// Dest = Dest + (Source * (WeightIndex != INDEX_NONE ? Weights[WeightIndex] : DefaultScaleWeight))
	ANIMNEXT_API void BlendAddPerBoneWithScale(
		const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source,
		const TArrayView<const int32>& LODBoneIndexToWeightIndexMap, const TArrayView<const float>& BoneWeights, const float DefaultScaleWeight);
	ANIMNEXT_API void BlendAddPerBoneWithScale(
		const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source,
		const TArrayView<const int32>& LODBoneIndexToWeightIndexMap, const TArrayView<const float>& BoneWeights, const float DefaultScaleWeight);
}

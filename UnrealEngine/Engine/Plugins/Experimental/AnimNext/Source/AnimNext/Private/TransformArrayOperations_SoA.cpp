// Copyright Epic Games, Inc. All Rights Reserved.

#include "TransformArrayOperations.h"
#include "TransformArray.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_CopyTransforms_SoA);
DEFINE_STAT(STAT_AnimNext_NormalizeRotations_SoA);
DEFINE_STAT(STAT_AnimNext_BlendOverwrite_SoA);
DEFINE_STAT(STAT_AnimNext_BlendAccumulate_SoA);

namespace UE::AnimNext
{
	// A couple of performance notes:
	//    * Types all use doubles
	//    * Because we split transforms into 3 arrays, we pay 3x the range checks
	//    * We could use pointers directly to avoid range checks, similar to what ISPC would do
	//    * FTransform uses 3x register wide types (32 bytes each)
	//    * We could do the same and use register wide types for aligned stores at the expense of
	//      using more memory
	//    * Unaligned loads aren't a big deal, perf wise, as long as we are 16 byte aligned
	//    * FVector loads require zeroing W which we could do without if we ensure we have padding
	//      on the last array element to avoid an out-of-bounds read
	//    * FVector stores end up as 2 stores which isn't too bad
	//    * Array values are very likely to be in the CPU L1 but to help hide the latency we should
	//      move loads next to one another to bundle them up up front
	//    * The biggest cost to normalizing is the square root and division which we perform on 4 elements
	//      even though there is a single value: the length. We could reduce this cost by processing
	//      4 rotations at a time and computing the square root and division for all 4 at the same time.
	//      To achieve this, we would have to swizzle the inputs/outputs in true SOA format: XXXX, YYYY, ZZZZ, WWWW.
	//      Shuffling with 4x4 floats requires 8 shuffles (after the 4 loads) and with doubles we should be
	//      able to do it with 8 loads and 8 shuffles (perhaps less with AVX).
	//      Another option might be to split loops into stages:
	//          * do dot, write it out for 8 entries
	//          * load, do sqrt, write
	//          * load, do div, write
	//          * load, cmp, mul, mask, final store
	//      Splitting into stages would help ensure that the CPU is saturated with independent instructions
	//      it can execute out-of-order. The intermediary load/stores would be from the stack if the values don't fit
	//      in registers or if we wish to re-order things cheaply (shuffle for sqrt from above).
	//      The idea being to keep loops smaller to maximize overlap of instructions while waiting for long
	//      latency instructions (cache miss in first batch, sqrt, div, etc).

	void SetIdentity(const FTransformArraySoAView& Dest, bool bIsAdditive)
	{
		const int32 NumTransforms = Dest.Num();

		const FTransform& Identity = bIsAdditive ? TransformAdditiveIdentity : FTransform::Identity;

		const FVector IdentityTranslation = Identity.GetTranslation();
		const FQuat IdentityRotation = Identity.GetRotation();
		const FVector IdentityScale3D = Identity.GetScale3D();

		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			Dest.Translations[TransformIndex] = IdentityTranslation;
			Dest.Rotations[TransformIndex] = IdentityRotation;
			Dest.Scales3D[TransformIndex] = IdentityScale3D;
		}
	}

	void CopyTransforms(const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source, int32 StartIndex, int32 NumToCopy)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimNext_CopyTransforms_SoA);
		
		const int32 NumTransforms = Dest.Num();
		const int32 EndIndex = NumToCopy >= 0 ? (StartIndex + NumToCopy) : NumTransforms;

		check(Source.Num() >= NumTransforms);
		check(StartIndex >= 0 && StartIndex <= NumTransforms);
		check(EndIndex <= NumTransforms);

		for (int32 TransformIndex = StartIndex; TransformIndex < EndIndex; ++TransformIndex)
		{
			Dest.Translations[TransformIndex] = Source.Translations[TransformIndex];
			Dest.Rotations[TransformIndex] = Source.Rotations[TransformIndex];
			Dest.Scales3D[TransformIndex] = Source.Scales3D[TransformIndex];
		}
	}

	void NormalizeRotations(const FTransformArraySoAView& Input)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimNext_NormalizeRotations_SoA);

		const int32 NumTransforms = Input.Num();

		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			Input.Rotations[TransformIndex].Normalize();
		}
	}

	void BlendWithIdentityAndAccumulate(const FTransformArraySoAView& Base, const FTransformArraySoAConstView& Additive, const float BlendWeight)
	{
		const int32 NumTransforms = Base.Num();

		check(Additive.Num() >= NumTransforms);

		using TransformVectorRegister = FTransform::TransformVectorRegister;

		const TransformVectorRegister VBlendWeight(ScalarRegister(BlendWeight).Value);
		const TransformVectorRegister Zero = VectorZero();
		const TransformVectorRegister Const0001 = GlobalVectorConstants::Float0001;
		const TransformVectorRegister ConstNegative0001 = VectorSubtract(Zero, Const0001);
		const TransformVectorRegister VOneMinusAlpha = VectorSubtract(VectorOne(), VBlendWeight);
		const TransformVectorRegister DefaultScale = GlobalVectorConstants::Float1110;

		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			// Blend rotation
			//     To ensure the 'shortest route', we make sure the dot product between the both rotations is positive.
			//     const float Bias = (|A.B| >= 0 ? 1 : -1)
			//     BlendedAtom.Rotation = (B * Alpha) + (A * (Bias * (1.f - Alpha)));
			//     BlendedAtom.Rotation.QuaternionNormalize();
			//  Note: A = (0,0,0,1), which simplifies things a lot; only care about sign of B.W now, instead of doing a dot product
			const TransformVectorRegister RotationB = VectorLoadAligned(&Additive.Rotations[TransformIndex]);

			const TransformVectorRegister QuatRotationDirMask = VectorCompareGE(RotationB, Zero);
			const TransformVectorRegister BiasTimesA = VectorSelect(QuatRotationDirMask, Const0001, ConstNegative0001);
			const TransformVectorRegister RotateBTimesWeight = VectorMultiply(RotationB, VBlendWeight);
			const TransformVectorRegister UnnormalizedRotation = VectorMultiplyAdd(BiasTimesA, VOneMinusAlpha, RotateBTimesWeight);

			// Normalize blended rotation ( result = (Q.Q >= 1e-8) ? (Q / |Q|) : (0,0,0,1) )
			const TransformVectorRegister BlendedRotation = VectorNormalizeSafe(UnnormalizedRotation, Const0001);

			// FinalAtom.Rotation = BlendedAtom.Rotation * FinalAtom.Rotation;
			Base.Rotations[TransformIndex] = FQuat::MakeFromVectorRegister(VectorQuaternionMultiply2(BlendedRotation, VectorLoadAligned(&Base.Rotations[TransformIndex])));

			// Blend translation and scale
			//    BlendedAtom.Translation = Lerp(Zero, Additive.Translation, Alpha);
			//    BlendedAtom.Scale = Lerp(0, Additive.Scale, Alpha);
			const TransformVectorRegister BlendedTranslation = FMath::Lerp<TransformVectorRegister>(Zero, VectorLoadFloat3_W0(&Additive.Translations[TransformIndex]), VBlendWeight);
			const TransformVectorRegister BlendedScale3D = FMath::Lerp<TransformVectorRegister>(Zero, VectorLoadFloat3_W0(&Additive.Scales3D[TransformIndex]), VBlendWeight);

			// Apply translation and scale to final atom
			//     FinalAtom.Translation += BlendedAtom.Translation
			//     FinalAtom.Scale *= BlendedAtom.Scale
			VectorStoreFloat3(VectorAdd(VectorLoadFloat3_W0(&Base.Translations[TransformIndex]), BlendedTranslation), &Base.Translations[TransformIndex]);
			VectorStoreFloat3(VectorMultiply(VectorLoadFloat3_W0(&Base.Scales3D[TransformIndex]), VectorAdd(DefaultScale, BlendedScale3D)), &Base.Scales3D[TransformIndex]);
		}
	}

	void BlendOverwriteWithScale(const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source, const float ScaleWeight)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimNext_BlendOverwrite_SoA);

		const int32 NumTransforms = Source.Num();

		check(Dest.Num() >= NumTransforms);

		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			Dest.Translations[TransformIndex] = Source.Translations[TransformIndex] * ScaleWeight;
			Dest.Rotations[TransformIndex] = Source.Rotations[TransformIndex] * ScaleWeight;
			Dest.Scales3D[TransformIndex] = Source.Scales3D[TransformIndex] * ScaleWeight;
		}
	}

	void BlendAddWithScale(const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source, const float ScaleWeight)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimNext_BlendAccumulate_SoA);

		const int32 NumTransforms = Source.Num();

		check(Dest.Num() >= NumTransforms);

		using QuatVectorRegister = FQuat::QuatVectorRegister;
		const QuatVectorRegister Zero = VectorZero();
		const ScalarRegister VScaleWeight(ScaleWeight);

		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			const QuatVectorRegister SourceRotation = VectorLoadAligned(&Source.Rotations[TransformIndex]);
			const QuatVectorRegister DestRotation = VectorLoadAligned(&Dest.Rotations[TransformIndex]);

			const QuatVectorRegister BlendedRotation = VectorMultiply(SourceRotation, VScaleWeight);

			// Blend rotation
			//     To ensure the 'shortest route', we make sure the dot product between the both rotations is positive.
			//     const float Bias = (|A.B| >= 0 ? 1 : -1)
			//     return A + B * Bias;

			const QuatVectorRegister RotationDot = VectorDot4(DestRotation, BlendedRotation);
			const QuatVectorRegister QuatRotationDirMask = VectorCompareGE(RotationDot, Zero);
			const QuatVectorRegister NegativeB = VectorSubtract(Zero, BlendedRotation);
			const QuatVectorRegister BiasTimesB = VectorSelect(QuatRotationDirMask, BlendedRotation, NegativeB);

			Dest.Rotations[TransformIndex] = FQuat::MakeFromVectorRegister(VectorAdd(DestRotation, BiasTimesB));

			Dest.Translations[TransformIndex] += Source.Translations[TransformIndex] * ScaleWeight;
			Dest.Scales3D[TransformIndex] += Source.Scales3D[TransformIndex] * ScaleWeight;
		}
	}

	void BlendOverwritePerBoneWithScale(
		const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source,
		const TArrayView<const int32>& LODBoneIndexToWeightIndexMap, const TArrayView<const float>& BoneWeights, const float DefaultScaleWeight)
	{
		const int32 NumTransforms = Source.Num();

		check(Dest.Num() >= NumTransforms);

		for (int32 LODBoneIndex = 0; LODBoneIndex < NumTransforms; ++LODBoneIndex)
		{
			const int32 PerBoneIndex = LODBoneIndexToWeightIndexMap[LODBoneIndex];
			const float ScaleWeight = BoneWeights.IsValidIndex(PerBoneIndex) ? BoneWeights[PerBoneIndex] : DefaultScaleWeight;

			Dest.Translations[LODBoneIndex] = Source.Translations[LODBoneIndex] * ScaleWeight;
			Dest.Rotations[LODBoneIndex] = Source.Rotations[LODBoneIndex] * ScaleWeight;
			Dest.Scales3D[LODBoneIndex] = Source.Scales3D[LODBoneIndex] * ScaleWeight;
		}
	}

	void BlendAddPerBoneWithScale(
		const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source,
		const TArrayView<const int32>& LODBoneIndexToWeightIndexMap, const TArrayView<const float>& BoneWeights, const float DefaultScaleWeight)
	{
		const int32 NumTransforms = Source.Num();

		check(Dest.Num() >= NumTransforms);

		using QuatVectorRegister = FQuat::QuatVectorRegister;
		const QuatVectorRegister Zero = VectorZero();

		for (int32 LODBoneIndex = 0; LODBoneIndex < NumTransforms; ++LODBoneIndex)
		{
			const int32 PerBoneIndex = LODBoneIndexToWeightIndexMap[LODBoneIndex];
			const float ScaleWeight = BoneWeights.IsValidIndex(PerBoneIndex) ? BoneWeights[PerBoneIndex] : DefaultScaleWeight;
			const ScalarRegister VScaleWeight(ScaleWeight);

			const QuatVectorRegister SourceRotation = VectorLoadAligned(&Source.Rotations[LODBoneIndex]);
			const QuatVectorRegister DestRotation = VectorLoadAligned(&Dest.Rotations[LODBoneIndex]);

			const QuatVectorRegister BlendedRotation = VectorMultiply(SourceRotation, VScaleWeight);

			// Blend rotation
			//     To ensure the 'shortest route', we make sure the dot product between the both rotations is positive.
			//     const float Bias = (|A.B| >= 0 ? 1 : -1)
			//     return A + B * Bias;

			const QuatVectorRegister RotationDot = VectorDot4(DestRotation, BlendedRotation);
			const QuatVectorRegister QuatRotationDirMask = VectorCompareGE(RotationDot, Zero);
			const QuatVectorRegister NegativeB = VectorSubtract(Zero, BlendedRotation);
			const QuatVectorRegister BiasTimesB = VectorSelect(QuatRotationDirMask, BlendedRotation, NegativeB);

			Dest.Rotations[LODBoneIndex] = FQuat::MakeFromVectorRegister(VectorAdd(DestRotation, BiasTimesB));

			Dest.Translations[LODBoneIndex] += Source.Translations[LODBoneIndex] * ScaleWeight;
			Dest.Scales3D[LODBoneIndex] += Source.Scales3D[LODBoneIndex] * ScaleWeight;
		}
	}
}

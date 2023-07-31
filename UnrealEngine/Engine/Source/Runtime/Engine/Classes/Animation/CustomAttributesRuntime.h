// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "Animation/AttributesRuntime.h"
PRAGMA_ENABLE_DEPRECATION_WARNINGS

UE_STATIC_DEPRECATE(5.0, true, "CustomAttributesRuntime.h has been deprecated - please include and use AttributesRuntime.h instead.");

struct ENGINE_API FCustomAttributesRuntime
{
#if WITH_EDITOR
	/** Editor functionality to retrieve custom attribute values from the raw data */
	UE_DEPRECATED(5.0, "Use UE::Animation::Attributes::GetAttributeValue with different signature")
	static void GetAttributeValue(struct UE::Anim::FStackAttributeContainer& OutAttributes, const struct FCompactPoseBoneIndex& PoseBoneIndex, const struct FCustomAttribute& Attribute, const struct FAnimExtractContext& ExtractionContext) {}
	UE_DEPRECATED(5.0, "Use UE::Animation::Attributes::GetAttributeValue with different signature")
	static void GetAttributeValue(const struct FCustomAttribute& Attribute, float Time, float& OutValue) {}
	UE_DEPRECATED(5.0, "Use UE::Animation::Attributes::GetAttributeValue with different signature")
	static void GetAttributeValue(const struct FCustomAttribute& Attribute, float Time, int32& OutValue) {}
	UE_DEPRECATED(5.0, "Use UE::Animation::Attributes::GetAttributeValue with different signature")
	static void GetAttributeValue(const struct FCustomAttribute& Attribute, float Time, FString& OutValue) {}
#endif // WITH_EDITOR

	/** Blend custom attribute values from N set of inputs */
	UE_DEPRECATED(5.0, "Functionality moved, see UE::Animation::Attributes::BlendAttributes")
	static void BlendAttributes(const TArrayView<const UE::Anim::FStackAttributeContainer> SourceAttributes, const TArrayView<const float> SourceWeights, UE::Anim::FStackAttributeContainer& OutAttributes) { UE::Anim::Attributes::BlendAttributes(SourceAttributes, SourceWeights, OutAttributes); }

	/** Blend custom attribute values from N set of inputs (ptr-values) */
	UE_DEPRECATED(5.0, "Functionality moved, see UE::Animation::Attributes::BlendAttributes")
	static void BlendAttributes(const TArrayView<const UE::Anim::FStackAttributeContainer* const> SourceAttributes, const TArrayView<const float> SourceWeights, UE::Anim::FStackAttributeContainer& OutAttributes) { UE::Anim::Attributes::BlendAttributes(SourceAttributes, SourceWeights, OutAttributes); }

	/** Blend custom attribute values from N set of inputs, using input weight remapping */
	UE_DEPRECATED(5.0, "Functionality moved, see UE::Animation::Attributes::BlendAttributes")
	static void BlendAttributes(const TArrayView<const UE::Anim::FStackAttributeContainer> SourceAttributes, const TArrayView<const float> SourceWeights, const TArrayView<const int32> SourceWeightsIndices, UE::Anim::FStackAttributeContainer& OutAttributes) { UE::Anim::Attributes::BlendAttributes(SourceAttributes, SourceWeights, SourceWeightsIndices, OutAttributes); }

	/* Blend custom attribute values from 2 inputs, using per-bone weights */ 
	UE_DEPRECATED(5.0, "Functionality moved, see UE::Animation::Attributes::BlendAttributesPerBone")
	static void BlendAttributesPerBone(const UE::Anim::FStackAttributeContainer& SourceAttributes1, const UE::Anim::FStackAttributeContainer& SourceAttributes2, const TArrayView<const float> WeightsOfSource2, UE::Anim::FStackAttributeContainer& OutAttributes) { UE::Anim::Attributes::BlendAttributesPerBone(SourceAttributes1, SourceAttributes2, WeightsOfSource2, OutAttributes); }
		
	/* Blend custom attribute values from N set of inputs, using bone filter pose weights */ 
	UE_DEPRECATED(5.0, "Functionality moved, see UE::Animation::Attributes::BlendAttributesPerBoneFilter")
	static void BlendAttributesPerBoneFilter(const TArrayView<const UE::Anim::FStackAttributeContainer> BlendAttributes, const TArray<FPerBoneBlendWeight>& BoneBlendWeights, UE::Anim::FStackAttributeContainer& OutAttributes)
	{
		UE::Anim::Attributes::BlendAttributesPerBoneFilter(OutAttributes, BlendAttributes, BoneBlendWeights, OutAttributes);
	}
		
	/** Add any new or override existing custom attributes */
	UE_DEPRECATED(5.0, "Functionality moved, see UE::Animation::Attributes::OverrideAttributes")
	static void OverrideAttributes(const UE::Anim::FStackAttributeContainer& SourceAttributes, UE::Anim::FStackAttributeContainer& OutAttributes, float Weight) { UE::Anim::Attributes::OverrideAttributes(SourceAttributes, OutAttributes, Weight); }

	/** Add any new or accumulate with existing custom attributes */
	UE_DEPRECATED(5.0, "Functionality moved, see UE::Animation::Attributes::AccumulateAttributes")
	static void AccumulateAttributes(const UE::Anim::FStackAttributeContainer& SourceAttributes, UE::Anim::FStackAttributeContainer& OutAttributes, float Weight) { UE::Anim::Attributes::AccumulateAttributes(SourceAttributes, OutAttributes, Weight, AAT_None); }

	/** Add (negated) any new or subtract from existing custom attributes */
	UE_DEPRECATED(5.0, "Functionality moved, see UE::Animation::Attributes::OverrideAttributes")
	static void SubtractAttributes(const UE::Anim::FStackAttributeContainer& SourceAttributes, UE::Anim::FStackAttributeContainer& OutAttributes) { UE::Anim::Attributes::ConvertToAdditive(SourceAttributes, OutAttributes); }

	/** Copy attributes from source, and remap the bone indices according to BoneMapToSource */
	UE_DEPRECATED(5.0, "Functionality moved, see UE::Animation::Attributes::CopyAndRemapAttributes")
	static void CopyAndRemapAttributes(const UE::Anim::FHeapAttributeContainer& SourceAttributes, UE::Anim::FStackAttributeContainer& OutAttributes, const TMap<int32, int32>& BoneMapToSource, const FBoneContainer& RequiredBones) { UE::Anim::Attributes::CopyAndRemapAttributes(SourceAttributes, OutAttributes, BoneMapToSource, RequiredBones); }

	/** Interpolates between two sets of attributes */
	UE_DEPRECATED(5.0, "Functionality moved, see UE::Animation::Attributes::InterpolateAttributes")
	static void InterpolateAttributes(const UE::Anim::FHeapAttributeContainer& SourceAttributes, UE::Anim::FHeapAttributeContainer& OutAttributes, float Alpha) { UE::Anim::Attributes::InterpolateAttributes(OutAttributes, SourceAttributes, Alpha); }

	UE_DEPRECATED(5.0, "Functionality moved, see UE::Animation::Attributes::GetAttributeBlendType")
	/** Helper functionality to retrieve the correct blend type (from UAnimationSettings) for the provided attribute name */
	static ECustomAttributeBlendType GetAttributeBlendType(const FName& InName) { return UE::Anim::Attributes::GetAttributeBlendType(InName); }
};

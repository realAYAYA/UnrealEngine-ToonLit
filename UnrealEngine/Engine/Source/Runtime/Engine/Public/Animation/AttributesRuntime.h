// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Animation/AnimTypes.h"
#include "UObject/NameTypes.h"
#include "Containers/ContainersFwd.h"

#include "Containers/Map.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "UObject/StructOnScope.h"

#include "Animation/IAttributeBlendOperator.h"

#include "UObject/Class.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "Animation/AttributesContainer.h"
#include "Animation/WrappedAttribute.h"

struct FCompactPoseBoneIndex;
struct FBlendSampleData;
struct FBoneContainer;

enum class ECustomAttributeBlendType : uint8;
enum EAdditiveAnimationType;

struct FAnimatedBoneAttribute;
struct FCompactPoseBoneIndex;
struct FAnimExtractContext;

class UMirrorDataTable;

namespace UE
{
	namespace Anim
	{
		struct ENGINE_API FStackAttributeContainer : public TAttributeContainer<FCompactPoseBoneIndex, FAnimStackAllocator> {};
		struct ENGINE_API FHeapAttributeContainer : public TAttributeContainer<FCompactPoseBoneIndex, FDefaultAllocator> {};
		struct ENGINE_API FMeshAttributeContainer : public TAttributeContainer<FMeshPoseBoneIndex, FDefaultAllocator> {};

		/** Accessor for internal data of the TAttributeContainer.  The TAttributeContainer methods are preferred - this accessor should only be used in limited situations */
		template<class BoneIndexType, typename InAllocator>
        struct TAttributeContainerAccessor
		{
			static TArray<TWrappedAttribute<InAllocator>>& GetValues(TAttributeContainer<BoneIndexType, InAllocator>& Attributes,  int32 TypeIndex) {return Attributes.GetValuesInternal(TypeIndex);}
			static TArray<FAttributeId>& GetKeys(TAttributeContainer<BoneIndexType, InAllocator>& Attributes, int32 TypeIndex) {return Attributes.GetKeysInternal(TypeIndex);}
		};

		struct ENGINE_API FStackAttributeContainerAccessor : public TAttributeContainerAccessor<FCompactPoseBoneIndex, FAnimStackAllocator> {};
		struct ENGINE_API FHeapAttributeContainerAccessor : public TAttributeContainerAccessor<FCompactPoseBoneIndex, FDefaultAllocator> {};
		
		/** Helper functionality for attributes animation runtime */
		struct ENGINE_API Attributes
		{
#if WITH_EDITOR
			static void GetAttributeValue(FStackAttributeContainer& OutAttributes, const FCompactPoseBoneIndex& PoseBoneIndex, const  FAnimatedBoneAttribute& Attribute, const FAnimExtractContext& ExtractionContext);
#endif // WITH_EDITOR

			/** Blend custom attribute values from N set of inputs */
			static void BlendAttributes(const TArrayView<const FStackAttributeContainer> SourceAttributes, const TArrayView<const float> SourceWeights, FStackAttributeContainer& OutAttributes);

			/** Blend custom attribute values from N set of inputs (ptr-values) */
			static void BlendAttributes(const TArrayView<const FStackAttributeContainer* const> SourceAttributes, const TArrayView<const float> SourceWeights, FStackAttributeContainer& OutAttributes);

			/** Blend custom attribute values from N set of inputs, using input weight remapping */
			static void BlendAttributes(const TArrayView<const FStackAttributeContainer> SourceAttributes, const TArrayView<const float> SourceWeights, const TArrayView<const int32> SourceWeightsIndices, FStackAttributeContainer& OutAttributes);

			/* Blend custom attribute values from 2 inputs, using per-bone weights */
			static void BlendAttributesPerBone(const FStackAttributeContainer& SourceAttributes1, const FStackAttributeContainer& SourceAttributes2, const TArrayView<const float> WeightsOfSource2, FStackAttributeContainer& OutAttributes);

			/* Blend custom attribute values from N set of inputs, using N number of blend samples */
			static void BlendAttributesPerBone(TArrayView<const FStackAttributeContainer> SourceAttributes, TArrayView<const int32> PerBoneInterpolationIndices, TArrayView<const FBlendSampleData> BlendSampleDataCache, FStackAttributeContainer& OutAttributes);

			/* Blend custom attribute values from N set of inputs, using N number of blend samples */
			static void BlendAttributesPerBone(TArrayView<const FStackAttributeContainer> SourceAttributes, TArrayView<const int32> PerBoneInterpolationIndices, TArrayView<const FBlendSampleData> BlendSampleDataCache, TArrayView<const int32> BlendSampleDataCacheIndices, FStackAttributeContainer& OutAttributes);

			/* Blend custom attribute values from N set of inputs, using bone filter pose weights */
			static void BlendAttributesPerBoneFilter(const FStackAttributeContainer& BaseAttributes, const TArrayView<const FStackAttributeContainer> BlendAttributes, const TArray<FPerBoneBlendWeight>& BoneBlendWeights, FStackAttributeContainer& OutAttributes);

			/** Add any new or override existing custom attributes */
			static void OverrideAttributes(const FStackAttributeContainer& SourceAttributes, FStackAttributeContainer& OutAttributes, float Weight);

			/** Add any new or accumulate with existing custom attributes */
			static void AccumulateAttributes(const FStackAttributeContainer& SourceAttributes, FStackAttributeContainer& OutAttributes, float Weight, EAdditiveAnimationType AdditiveType);

			/** Add (negated) any new or subtract from existing custom attributes */
			static void ConvertToAdditive(const FStackAttributeContainer& BaseAttributes, FStackAttributeContainer& OutAdditiveAttributes);

			/** Copy attributes from source, and remap the bone indices according to BoneMapToSource */
			static void CopyAndRemapAttributes(const FMeshAttributeContainer& SourceAttributes, FStackAttributeContainer& OutAttributes, const TMap<int32, int32>& BoneMapToSource, const FBoneContainer& RequiredBones);

			/** Interpolates between two sets of attributes */
			static void InterpolateAttributes(FMeshAttributeContainer& FromAttributes, const FMeshAttributeContainer& ToAttributes, float Alpha);

			/** Mirror (swap) attributes with the specified MirrorDataTable. Attributes are swapped using the bone mapping such that bones which are mirrored swap attributes */
			static void MirrorAttributes(FStackAttributeContainer& Attributes, const UMirrorDataTable& MirrorDataTable);
			
			/** Helper functionality to retrieve the correct blend type (from UAnimationSettings) for the provided attribute name */
			static ECustomAttributeBlendType GetAttributeBlendType(const FName& InName);

			/** Helper functionality to retrieve the correct blend type (from UAnimationSettings) for the provided attribute info */
			static ECustomAttributeBlendType GetAttributeBlendType(const FAttributeId& Info);
		};
	}
}

struct ENGINE_API UE_DEPRECATED(5.0, "FStackCustomAttributes has been deprecated use UE::Anim::FStackAttributeContainer instead") FStackCustomAttributes : public UE::Anim::FStackAttributeContainer {};
struct ENGINE_API UE_DEPRECATED(5.0, "FStackCustomAttributes has been deprecated use UE::Anim::FStackAttributeContainer instead") FHeapCustomAttributes : public UE::Anim::FHeapAttributeContainer {};


// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchDerivedDataKey.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Animation/Skeleton.h"

namespace UE::PoseSearch
{

//////////////////////////////////////////////////////////////////////////
// FFeatureVectorHelper
int32 FFeatureVectorHelper::GetVectorCardinality(EComponentStrippingVector ComponentStrippingVector)
{
	switch (ComponentStrippingVector)
	{
	case EComponentStrippingVector::None:
		return 3;
	case EComponentStrippingVector::StripXY:
		return 1;
	case EComponentStrippingVector::StripZ:
		return 2;
	default:
		checkNoEntry();
		return 0;
	}
}

void FFeatureVectorHelper::EncodeVector(TArrayView<float> Values, int32 DataOffset, const FVector& Vector, EComponentStrippingVector ComponentStrippingVector)
{
	switch (ComponentStrippingVector)
	{
	case EComponentStrippingVector::None:
		Values[DataOffset + 0] = Vector.X;
		Values[DataOffset + 1] = Vector.Y;
		Values[DataOffset + 2] = Vector.Z;
		break;
	case EComponentStrippingVector::StripXY:
		Values[DataOffset + 0] = Vector.Z;
		break;
	case EComponentStrippingVector::StripZ:
		Values[DataOffset + 0] = Vector.X;
		Values[DataOffset + 1] = Vector.Y;
		break;
	default:
		checkNoEntry();
		break;
	}
}

FVector FFeatureVectorHelper::DecodeVector(TConstArrayView<float> Values, int32 DataOffset, EComponentStrippingVector ComponentStrippingVector)
{
	switch (ComponentStrippingVector)
	{
	case EComponentStrippingVector::None:
		return FVector(Values[DataOffset + 0], Values[DataOffset + 1], Values[DataOffset + 2]);
	case EComponentStrippingVector::StripXY:
		return FVector(0, 0, Values[DataOffset + 0]);
	case EComponentStrippingVector::StripZ:
		return FVector(Values[DataOffset + 0], Values[DataOffset + 1], 0);
	default:
		checkNoEntry();
		return FVector::Zero();
	}
}

void FFeatureVectorHelper::EncodeVector2D(TArrayView<float> Values, int32 DataOffset, const FVector2D& Vector2D)
{
	Values[DataOffset + 0] = Vector2D.X;
	Values[DataOffset + 1] = Vector2D.Y;
}

FVector2D FFeatureVectorHelper::DecodeVector2D(TConstArrayView<float> Values, int32 DataOffset)
{
	return FVector2D(Values[DataOffset + 0], Values[DataOffset + 1]);
}

void FFeatureVectorHelper::EncodeFloat(TArrayView<float> Values, int32 DataOffset, const float Value)
{
	Values[DataOffset + 0] = Value;
}

float FFeatureVectorHelper::DecodeFloat(TConstArrayView<float> Values, int32 DataOffset)
{
	return Values[DataOffset];
}

void FFeatureVectorHelper::Copy(TArrayView<float> Values, int32 DataOffset, int32 DataCardinality, TConstArrayView<float> OriginValues)
{
	check(Values.Num() == OriginValues.Num() && DataOffset + DataCardinality <= Values.Num());

	float* RESTRICT ValuesData = Values.GetData() + DataOffset;
	const float* RESTRICT OriginValuesData = OriginValues.GetData() + DataOffset;

	check(ValuesData != OriginValuesData);

	for (int32 i = 0; i < DataCardinality; ++i, ++ValuesData, ++OriginValuesData)
	{
		*ValuesData = *OriginValuesData;
	}
}

} // namespace UE::PoseSearch

//////////////////////////////////////////////////////////////////////////
// UPoseSearchFeatureChannel
void UPoseSearchFeatureChannel::GetPermutationTimeOffsets(EPermutationTimeType PermutationTimeType, float DesiredPermutationTimeOffset, float& OutPermutationSampleTimeOffset, float& OutPermutationOriginTimeOffset)
{
	switch (PermutationTimeType)
	{
	case EPermutationTimeType::UseSampleTime:
		OutPermutationSampleTimeOffset = 0.f;
		OutPermutationOriginTimeOffset = 0.f;
		break;
	case EPermutationTimeType::UsePermutationTime:
		OutPermutationSampleTimeOffset = DesiredPermutationTimeOffset;
		OutPermutationOriginTimeOffset = DesiredPermutationTimeOffset;
		break;
	case EPermutationTimeType::UseSampleToPermutationTime:
		OutPermutationSampleTimeOffset = DesiredPermutationTimeOffset;
		OutPermutationOriginTimeOffset = 0.f;
		break;
	default:
		checkNoEntry();
		OutPermutationSampleTimeOffset = 0.f;
		OutPermutationOriginTimeOffset = 0.f;
		break;
	}
}

#if WITH_EDITOR
FString UPoseSearchFeatureChannel::GetLabel() const
{
	TStringBuilder<256> Label;
	if (const UPoseSearchFeatureChannel* OuterChannel = Cast<UPoseSearchFeatureChannel>(GetOuter()))
	{
		Label.Append(OuterChannel->GetLabel());
		Label.Append(TEXT("_"));
	}
	Label.Append(GetName());
	return Label.ToString();
}

bool UPoseSearchFeatureChannel::CanBeNormalizedWith(const UPoseSearchFeatureChannel* Other) const
{
	if (this == Other)
	{
		return true;
	}

	if (ChannelCardinality != Other->ChannelCardinality)
	{
		return false;
	}

	if (GetClass() != Other->GetClass())
	{
		return false;
	}

	if (GetSchema()->Skeleton != Other->GetSchema()->Skeleton)
	{
		return false;
	}

	if (GetLabel() != Other->GetLabel())
	{
		return false;
	}

	return true;
}

const UPoseSearchSchema* UPoseSearchFeatureChannel::GetSchema() const
{
	UObject* Outer = GetOuter();
	while (Outer != nullptr)
	{
		if (const UPoseSearchSchema* Schema = Cast<UPoseSearchSchema>(Outer))
		{
			return Schema;
		}
		Outer = Outer->GetOuter();
	}
	checkNoEntry();
	return nullptr;
}
#endif // WITH_EDITOR

USkeleton* UPoseSearchFeatureChannel::GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle)
{
	bInvalidSkeletonIsError = false;
#if WITH_EDITOR
	return GetSchema()->Skeleton;
#else
	checkNoEntry();
	return nullptr;
#endif
}



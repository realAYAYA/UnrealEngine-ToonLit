// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchDerivedDataKey.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Animation/Skeleton.h"

namespace UE::PoseSearch
{

//////////////////////////////////////////////////////////////////////////
// FFeatureVectorHelper
void FFeatureVectorHelper::EncodeQuat(TArrayView<float> Values, int32& DataOffset, const FQuat& Quat)
{
	// unitary quaternions are non Euclidean representation of rotation, so they cannot be used to calculate cost functions within the context of kdtrees, 
	// so we convert them in a matrix, and pick 2 axis (we choose X,Y), skipping the 3rd since correlated to the cross product of the first two (this saves memory and cpu cycles)

	const FMatrix M = Quat.ToMatrix();
	const FVector X = M.GetScaledAxis(EAxis::X);
	const FVector Y = M.GetScaledAxis(EAxis::Y);

	Values[DataOffset + 0] = X.X;
	Values[DataOffset + 1] = X.Y;
	Values[DataOffset + 2] = X.Z;
	Values[DataOffset + 3] = Y.X;
	Values[DataOffset + 4] = Y.Y;
	Values[DataOffset + 5] = Y.Z;

	DataOffset += EncodeQuatCardinality;
}

void FFeatureVectorHelper::EncodeQuat(TArrayView<float> Values, int32& DataOffset, TConstArrayView<float> PrevValues, TConstArrayView<float> CurValues, TConstArrayView<float> NextValues, float LerpValue)
{
	FQuat Quat = DecodeQuatAtOffset(CurValues, DataOffset);

	// linear interpolation
	if (!FMath::IsNearlyZero(LerpValue))
	{
		if (LerpValue < 0.f)
		{
			Quat = FQuat::Slerp(Quat, DecodeQuatAtOffset(PrevValues, DataOffset), -LerpValue);
		}
		else
		{
			Quat = FQuat::Slerp(Quat, DecodeQuatAtOffset(NextValues, DataOffset), LerpValue);
		}
	}

	// @todo: do we need to add options for cubic interpolation?
	EncodeQuat(Values, DataOffset, Quat);
}

FQuat FFeatureVectorHelper::DecodeQuat(TConstArrayView<float> Values, int32& DataOffset)
{
	const FQuat Quat = DecodeQuatAtOffset(Values, DataOffset);
	DataOffset += EncodeQuatCardinality;
	return Quat;
}

FQuat FFeatureVectorHelper::DecodeQuatAtOffset(TConstArrayView<float> Values, int32 DataOffset)
{
	// reconstructing the chosen 2 axis (X,Y - from EncodeQuat) from the 6 floats from Values 
	const FVector X(Values[DataOffset + 0], Values[DataOffset + 1], Values[DataOffset + 2]);
	const FVector Y(Values[DataOffset + 3], Values[DataOffset + 4], Values[DataOffset + 5]);
	// calculating the 3rd axis required to reconstruct the rotation matrix
	const FVector Z = FVector::CrossProduct(X, Y);
	const FMatrix M(X, Y, Z, FVector::ZeroVector);
	// converting the matrix to the unitary quaternion
	return FQuat(M);
}

void FFeatureVectorHelper::EncodeVector(TArrayView<float> Values, int32& DataOffset, const FVector& Vector)
{
	Values[DataOffset + 0] = Vector.X;
	Values[DataOffset + 1] = Vector.Y;
	Values[DataOffset + 2] = Vector.Z;
	DataOffset += EncodeVectorCardinality;
}

void FFeatureVectorHelper::EncodeVector(TArrayView<float> Values, int32& DataOffset, TConstArrayView<float> PrevValues, TConstArrayView<float> CurValues, TConstArrayView<float> NextValues, float LerpValue, bool bNormalize)
{
	FVector Vector = DecodeVectorAtOffset(CurValues, DataOffset);

	// linear interpolation
	if (!FMath::IsNearlyZero(LerpValue))
	{
		if (LerpValue < 0.f)
		{
			Vector = FMath::Lerp(Vector, DecodeVectorAtOffset(PrevValues, DataOffset), -LerpValue);
		}
		else
		{
			Vector = FMath::Lerp(Vector, DecodeVectorAtOffset(NextValues, DataOffset), LerpValue);
		}
	}

	// @todo: do we need to add options for cubic interpolation?
	if (bNormalize)
	{
		Vector = Vector.GetSafeNormal(UE_SMALL_NUMBER, FVector::XAxisVector);
	}

	EncodeVector(Values, DataOffset, Vector);
}

FVector FFeatureVectorHelper::DecodeVector(TConstArrayView<float> Values, int32& DataOffset)
{
	const FVector Vector = DecodeVectorAtOffset(Values, DataOffset);
	DataOffset += EncodeVectorCardinality;
	return Vector;
}

FVector FFeatureVectorHelper::DecodeVectorAtOffset(TConstArrayView<float> Values, int32 DataOffset)
{
	return FVector(Values[DataOffset + 0], Values[DataOffset + 1], Values[DataOffset + 2]);
}

void FFeatureVectorHelper::EncodeVector2D(TArrayView<float> Values, int32& DataOffset, const FVector2D& Vector2D)
{
	Values[DataOffset + 0] = Vector2D.X;
	Values[DataOffset + 1] = Vector2D.Y;
	DataOffset += EncodeVector2DCardinality;
}

void FFeatureVectorHelper::EncodeVector2D(TArrayView<float> Values, int32& DataOffset, TConstArrayView<float> PrevValues, TConstArrayView<float> CurValues, TConstArrayView<float> NextValues, float LerpValue)
{
	FVector2D Vector2D = DecodeVector2DAtOffset(CurValues, DataOffset);

	// linear interpolation
	if (!FMath::IsNearlyZero(LerpValue))
	{
		if (LerpValue < 0.f)
		{
			Vector2D = FMath::Lerp(Vector2D, DecodeVector2DAtOffset(PrevValues, DataOffset), -LerpValue);
		}
		else
		{
			Vector2D = FMath::Lerp(Vector2D, DecodeVector2DAtOffset(NextValues, DataOffset), LerpValue);
		}
	}

	// @todo: do we need to add options for cubic interpolation?
	EncodeVector2D(Values, DataOffset, Vector2D);
}

FVector2D FFeatureVectorHelper::DecodeVector2D(TConstArrayView<float> Values, int32& DataOffset)
{
	const FVector2D Vector2D = DecodeVector2DAtOffset(Values, DataOffset);
	DataOffset += EncodeVector2DCardinality;
	return Vector2D;
}

FVector2D FFeatureVectorHelper::DecodeVector2DAtOffset(TConstArrayView<float> Values, int32 DataOffset)
{
	return FVector2D(Values[DataOffset + 0], Values[DataOffset + 1]);
}

void FFeatureVectorHelper::EncodeFloat(TArrayView<float> Values, int32& DataOffset, const float Value)
{
	Values[DataOffset + 0] = Value;
	DataOffset += EncodeFloatCardinality;
}

void FFeatureVectorHelper::EncodeFloat(TArrayView<float> Values, int32& DataOffset, TConstArrayView<float> PrevValues, TConstArrayView<float> CurValues, TConstArrayView<float> NextValues, float LerpValue)
{
	float Value = DecodeFloatAtOffset(CurValues, DataOffset);

	// linear interpolation
	if (!FMath::IsNearlyZero(LerpValue))
	{
		if (LerpValue < 0.f)
		{
			Value = FMath::Lerp(Value, DecodeFloatAtOffset(PrevValues, DataOffset), -LerpValue);
		}
		else
		{
			Value = FMath::Lerp(Value, DecodeFloatAtOffset(NextValues, DataOffset), LerpValue);
		}
	}

	// @todo: do we need to add options for cubic interpolation?
	EncodeFloat(Values, DataOffset, Value);
}

float FFeatureVectorHelper::DecodeFloat(TConstArrayView<float> Values, int32& DataOffset)
{
	const float Value = DecodeFloatAtOffset(Values, DataOffset);
	DataOffset += EncodeFloatCardinality;
	return Value;
}

float FFeatureVectorHelper::DecodeFloatAtOffset(TConstArrayView<float> Values, int32 DataOffset)
{
	return Values[DataOffset];
}

} // namespace UE::PoseSearch

//////////////////////////////////////////////////////////////////////////
// UPoseSearchFeatureChannel
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



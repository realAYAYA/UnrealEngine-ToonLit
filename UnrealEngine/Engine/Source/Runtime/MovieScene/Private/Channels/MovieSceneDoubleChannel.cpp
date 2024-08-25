// Copyright Epic Games, Inc. All Rights Reserved.

#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneCurveChannelImpl.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneInterpolation.h"
#include "HAL/Platform.h"
#include "MovieSceneFrameMigration.h"
#include "MovieSceneFwd.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/SequencerObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneDoubleChannel)

static_assert(
		sizeof(FMovieSceneDoubleValue) == 32,
		"The size of the float channel value has changed. You need to update the padding byte at the end of the structure. "
		"You also need to update the layout in FMovieSceneDoubleValue so that they match!");

bool FMovieSceneDoubleValue::Serialize(FArchive& Ar)
{
	return TMovieSceneCurveChannelImpl<FMovieSceneDoubleChannel>::SerializeChannelValue(*this, Ar);
}

bool FMovieSceneDoubleValue::operator==(const FMovieSceneDoubleValue& DoubleValue) const
{
	return (Value == DoubleValue.Value) && (InterpMode == DoubleValue.InterpMode) && (TangentMode == DoubleValue.TangentMode) && (Tangent == DoubleValue.Tangent);
}

bool FMovieSceneDoubleValue::operator!=(const FMovieSceneDoubleValue& Other) const
{
	return !(*this == Other);
}

int32 FMovieSceneDoubleChannel::AddConstantKey(FFrameNumber InTime, double InValue)
{
	return FMovieSceneDoubleChannelImpl::AddConstantKey(this, InTime, InValue);
}

int32 FMovieSceneDoubleChannel::AddLinearKey(FFrameNumber InTime, double InValue)
{
	return FMovieSceneDoubleChannelImpl::AddLinearKey(this, InTime, InValue);
}

int32 FMovieSceneDoubleChannel::AddCubicKey(FFrameNumber InTime, double InValue, ERichCurveTangentMode TangentMode, const FMovieSceneTangentData& Tangent)
{
	return FMovieSceneDoubleChannelImpl::AddCubicKey(this, InTime, InValue, TangentMode, Tangent);
}

bool FMovieSceneDoubleChannel::Evaluate(FFrameTime InTime,  double& OutValue) const
{
	return FMovieSceneDoubleChannelImpl::Evaluate(this, InTime, OutValue);
}

bool FMovieSceneDoubleChannel::Evaluate(FFrameTime InTime, float& OutValue) const
{
	double Temp;
	const bool bResult = Evaluate(InTime, Temp);
	OutValue = (float)Temp;
	return bResult;
}

UE::MovieScene::Interpolation::FCachedInterpolation FMovieSceneDoubleChannel::GetInterpolationForTime(FFrameTime InTime) const
{
	return FMovieSceneDoubleChannelImpl::GetInterpolationForTime(this, InTime);
}

void FMovieSceneDoubleChannel::Set(TArray<FFrameNumber> InTimes, TArray<FMovieSceneDoubleValue> InValues)
{
	FMovieSceneDoubleChannelImpl::Set(this, InTimes, InValues);
}

void FMovieSceneDoubleChannel::SetKeysOnly(TArrayView<FFrameNumber> InTimes, TArrayView<FMovieSceneDoubleValue> InValues)
{
	check(InTimes.Num() == InValues.Num());

	Times = MoveTemp(InTimes);
	Values = MoveTemp(InValues);

	KeyHandles.Reset();
}

void FMovieSceneDoubleChannel::AutoSetTangents(float Tension)
{
	FMovieSceneDoubleChannelImpl::AutoSetTangents(this, Tension);
}

void FMovieSceneDoubleChannel::PopulateCurvePoints(double StartTimeSeconds, double EndTimeSeconds, double TimeThreshold, double ValueThreshold, FFrameRate InTickResolution, TArray<TTuple<double, double>>& InOutPoints) const
{
	FMovieSceneDoubleChannelImpl::PopulateCurvePoints(this, StartTimeSeconds, EndTimeSeconds, TimeThreshold, ValueThreshold, InTickResolution, InOutPoints);
}

void FMovieSceneDoubleChannel::GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles)
{
	GetData().GetKeys(WithinRange, OutKeyTimes, OutKeyHandles);
}

void FMovieSceneDoubleChannel::GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes)
{
	GetData().GetKeyTimes(InHandles, OutKeyTimes);
}

void FMovieSceneDoubleChannel::SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes)
{
	GetData().SetKeyTimes(InHandles, InKeyTimes);
	AutoSetTangents();
}

void FMovieSceneDoubleChannel::DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles)
{
	GetData().DuplicateKeys(InHandles, OutNewHandles);
}

void FMovieSceneDoubleChannel::DeleteKeys(TArrayView<const FKeyHandle> InHandles)
{
	GetData().DeleteKeys(InHandles);
	AutoSetTangents();
}

void FMovieSceneDoubleChannel::DeleteKeysFrom(FFrameNumber InTime, bool bDeleteKeysBefore)
{
	FMovieSceneDoubleChannelImpl::DeleteKeysFrom(this, InTime, bDeleteKeysBefore);
	AutoSetTangents();
}

void FMovieSceneDoubleChannel::ChangeFrameResolution(FFrameRate SourceRate, FFrameRate DestinationRate)
{
	FMovieSceneDoubleChannelImpl::ChangeFrameResolution(this, SourceRate, DestinationRate);
}

TRange<FFrameNumber> FMovieSceneDoubleChannel::ComputeEffectiveRange() const
{
	return GetData().GetTotalRange();
}

int32 FMovieSceneDoubleChannel::GetNumKeys() const
{
	return Times.Num();
}

void FMovieSceneDoubleChannel::Reset()
{
	Times.Reset();
	Values.Reset();
	KeyHandles.Reset();
	bHasDefaultValue = false;
}

void FMovieSceneDoubleChannel::PostEditChange()
{
	AutoSetTangents();
}

void FMovieSceneDoubleChannel::Offset(FFrameNumber DeltaPosition)
{
	GetData().Offset(DeltaPosition);
	AutoSetTangents();
}

FKeyHandle FMovieSceneDoubleChannel::GetHandle(int32 Index)
{
	return GetData().GetHandle(Index);
}

int32 FMovieSceneDoubleChannel::GetIndex(FKeyHandle Handle)
{
	return GetData().GetIndex(Handle);
}

void FMovieSceneDoubleChannel::Optimize(const FKeyDataOptimizationParams& Params)
{
	FMovieSceneDoubleChannelImpl::Optimize(this, Params);
}

void FMovieSceneDoubleChannel::ClearDefault()
{
	bHasDefaultValue = false;
}

EMovieSceneKeyInterpolation GetInterpolationMode(FMovieSceneDoubleChannel* InChannel, const FFrameNumber& InTime, EMovieSceneKeyInterpolation DefaultInterpolationMode)
{
	return TMovieSceneCurveChannelImpl<FMovieSceneDoubleChannel>::GetInterpolationMode(InChannel, InTime, DefaultInterpolationMode);
}

FKeyHandle AddKeyToChannel(FMovieSceneDoubleChannel* Channel, FFrameNumber InFrameNumber, double InValue, EMovieSceneKeyInterpolation Interpolation)
{
	return TMovieSceneCurveChannelImpl<FMovieSceneDoubleChannel>::AddKeyToChannel(Channel, InFrameNumber, InValue, Interpolation);
}

void Dilate(FMovieSceneDoubleChannel* InChannel, FFrameNumber Origin, double DilationFactor)
{
	return TMovieSceneCurveChannelImpl<FMovieSceneDoubleChannel>::Dilate(InChannel, Origin, DilationFactor);
}

bool ValueExistsAtTime(const FMovieSceneDoubleChannel* InChannel, FFrameNumber InFrameNumber, const FMovieSceneDoubleValue& InValue)
{
	return TMovieSceneCurveChannelImpl<FMovieSceneDoubleChannel>::ValueExistsAtTime(InChannel, InFrameNumber, InValue);
}

bool ValueExistsAtTime(const FMovieSceneDoubleChannel* InChannel, FFrameNumber InFrameNumber, double InValue)
{
	return TMovieSceneCurveChannelImpl<FMovieSceneDoubleChannel>::ValueExistsAtTime(InChannel, InFrameNumber, InValue);
}

bool ValueExistsAtTime(const FMovieSceneDoubleChannel* InChannel, FFrameNumber InFrameNumber, float InValue)
{
	return TMovieSceneCurveChannelImpl<FMovieSceneDoubleChannel>::ValueExistsAtTime(InChannel, InFrameNumber, (double)InValue);
}

void AssignValue(FMovieSceneDoubleChannel* InChannel, FKeyHandle InKeyHandle, double InValue)
{
	return TMovieSceneCurveChannelImpl<FMovieSceneDoubleChannel>::AssignValue(InChannel, InKeyHandle, InValue);
}

void AssignValue(FMovieSceneDoubleChannel* InChannel, FKeyHandle InKeyHandle, float InValue)
{
	return TMovieSceneCurveChannelImpl<FMovieSceneDoubleChannel>::AssignValue(InChannel, InKeyHandle, (double)InValue);
}

void FMovieSceneDoubleChannel::AddKeys(const TArray<FFrameNumber>& InTimes, const TArray<FMovieSceneDoubleValue>& InValues)
{
	check(InTimes.Num() == InValues.Num());
	int32 Index = Times.Num();
	Times.Append(InTimes);
	Values.Append(InValues);
	for (; Index < Times.Num(); ++Index)
	{
		KeyHandles.AllocateHandle(Index);
	}
	AutoSetTangents();
}

#if WITH_EDITORONLY_DATA

bool FMovieSceneDoubleChannel::GetShowCurve() const
{
	return bShowCurve;
}

void FMovieSceneDoubleChannel::SetShowCurve(bool bInShowCurve)
{
	bShowCurve = bInShowCurve;
}

#endif

bool FMovieSceneDoubleChannel::Serialize(FArchive& Ar)
{
	return FMovieSceneDoubleChannelImpl::Serialize(this, Ar);
}

bool FMovieSceneDoubleChannel::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	// Load old content that was saved with rich curves.
	const bool bSeralizedFromRichCurve = FMovieSceneDoubleChannelImpl::SerializeFromRichCurve(this, Tag, Slot);
	if (bSeralizedFromRichCurve)
	{
		return true;
	}

	// Load pre-LWC content that was saved with a float channel.
	static const FName FloatChannelName("MovieSceneFloatChannel");
	if (Tag.GetType().IsStruct(FloatChannelName))
	{
		// We have to load the whole structure into a float channel, and then convert it into our data.
		// It's not ideal but it's the safest way to make it work.

		FMovieSceneFloatChannel TempChannel;

		// We also need to setup the temp channel object so that it matches the current channel. This is
		// because, for instance, the Translation/Rotation/Scale channels of the 3d transform section are
		// initialize with a default value of 0. But the default constructor of a channel leaves the
		// default value unset. So if we don't correctly initialize our temp object, it will have its
		// default value left unset unless the saved channel had a non-default value. So the bHasDefaultValue
		// would be left to "false" unless it was set to non-true in the channel... which mean it would
		// always be "false"!
		TMovieSceneCurveChannelImpl<FMovieSceneFloatChannel>::CopyChannel(this, &TempChannel);

		// Serialize the temp channel.
		FMovieSceneFloatChannel::StaticStruct()->SerializeItem(Slot, &TempChannel, nullptr);

		// Now copy the temp channel back into us.
		FMovieSceneDoubleChannelImpl::CopyChannel(&TempChannel, this);

		return true;
	}

	return false;
}



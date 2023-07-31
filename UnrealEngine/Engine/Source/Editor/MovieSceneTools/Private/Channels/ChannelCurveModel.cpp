// Copyright Epic Games, Inc. All Rights Reserved.

#include "Channels/ChannelCurveModel.h"

#include "Algo/BinarySearch.h"
#include "Channels/MovieSceneChannelData.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "Containers/UnrealString.h"
#include "CurveDataAbstraction.h"
#include "CurveDrawInfo.h"
#include "CurveEditorScreenSpace.h"
#include "Curves/RealCurve.h"
#include "Delegates/Delegate.h"
#include "HAL/PlatformCrt.h"
#include "ISequencer.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameRate.h"
#include "Misc/FrameTime.h"
#include "Misc/Optional.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Templates/UnrealTemplate.h"

class UObject;
struct FMovieSceneChannelMetaData;

template <class ChannelType, class ChannelValue, class KeyType>
FChannelCurveModel<ChannelType, ChannelValue, KeyType>::FChannelCurveModel(TMovieSceneChannelHandle<ChannelType> InChannel, UMovieSceneSection* OwningSection, TWeakPtr<ISequencer> InWeakSequencer)
{
	ChannelHandle = InChannel;
	WeakSection = OwningSection;
	WeakSequencer = InWeakSequencer;

	if (FMovieSceneChannelProxy* ChannelProxy = InChannel.GetChannelProxy())
	{
		OnDestroyHandle = ChannelProxy->OnDestroy.AddRaw(this, &FChannelCurveModel<ChannelType, ChannelValue, KeyType>::FixupCurve);
	}

	SupportedViews = ECurveEditorViewID::Absolute | ECurveEditorViewID::Normalized | ECurveEditorViewID::Stacked;
}

template <class ChannelType, class ChannelValue, class KeyType>
FChannelCurveModel<ChannelType, ChannelValue, KeyType>::~FChannelCurveModel()
{
	if (FMovieSceneChannelProxy* ChannelProxy = ChannelHandle.GetChannelProxy())
	{
		ChannelProxy->OnDestroy.Remove(OnDestroyHandle);
	}
}

template <class ChannelType, class ChannelValue, class KeyType>
const void* FChannelCurveModel<ChannelType, ChannelValue, KeyType>::GetCurve() const
{
	return ChannelHandle.Get();
}

template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::Modify()
{
	if (UMovieSceneSection* Section = WeakSection.Get())
	{
		Section->Modify();
	}
}


template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::DrawCurve(const FCurveEditor& CurveEditor, const FCurveEditorScreenSpace& ScreenSpace, TArray<TTuple<double, double>>& OutInterpolatingPoints) const
{
	ChannelType* Channel = ChannelHandle.Get();
	UMovieSceneSection* Section = WeakSection.Get();

	if (Channel && Section)
	{
		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		TMovieSceneChannelData<ChannelValue> ChannelData = Channel->GetData();
		TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
		TArrayView<const ChannelValue> Values = ChannelData.GetValues();

		const double DisplayOffset = GetInputDisplayOffset();
		const double StartTimeSeconds = ScreenSpace.GetInputMin() - DisplayOffset;
		const double EndTimeSeconds = ScreenSpace.GetInputMax() - DisplayOffset;

		const FFrameNumber StartFrame = (StartTimeSeconds * TickResolution).FloorToFrame();
		const FFrameNumber EndFrame = (EndTimeSeconds * TickResolution).CeilToFrame();

		const int32 StartingIndex = Algo::UpperBound(Times, StartFrame);
		const int32 EndingIndex = Algo::LowerBound(Times, EndFrame);

		TOptional<double> PreviousValue;
		for (int32 KeyIndex = StartingIndex; KeyIndex < EndingIndex; ++KeyIndex)
		{
			double Value = GetKeyValue(Values, KeyIndex);
			if (PreviousValue.IsSet() && PreviousValue.GetValue() != Value)
			{
				OutInterpolatingPoints.Add(MakeTuple(Times[KeyIndex] / TickResolution, PreviousValue.GetValue()));
			}

			OutInterpolatingPoints.Add(MakeTuple(Times[KeyIndex] / TickResolution, Value));
			PreviousValue = Value;
		}
	}
}

template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::GetKeys(const FCurveEditor& CurveEditor, double MinTime, double MaxTime, double MinValue, double MaxValue, TArray<FKeyHandle>& OutKeyHandles) const
{
	ChannelType* Channel = ChannelHandle.Get();
	UMovieSceneSection* Section = WeakSection.Get();

	if (Channel && Section)
	{
		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		TMovieSceneChannelData<ChannelValue> ChannelData = Channel->GetData();
		TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
		TArrayView<const ChannelValue> Values = ChannelData.GetValues();

		const FFrameNumber StartFrame = MinTime <= MIN_int32 ? MIN_int32 : (MinTime * TickResolution).CeilToFrame();
		const FFrameNumber EndFrame = MaxTime >= MAX_int32 ? MAX_int32 : (MaxTime * TickResolution).FloorToFrame();

		const int32 StartingIndex = Algo::LowerBound(Times, StartFrame);
		const int32 EndingIndex = Algo::UpperBound(Times, EndFrame);

		for (int32 KeyIndex = StartingIndex; KeyIndex < EndingIndex; ++KeyIndex)
		{
			if (GetKeyValue(Values, KeyIndex) >= MinValue && GetKeyValue(Values, KeyIndex) <= MaxValue)
			{
				OutKeyHandles.Add(ChannelData.GetHandle(KeyIndex));
			}
		}
	}
}

template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::GetKeyDrawInfo(ECurvePointType PointType, const FKeyHandle InKeyHandle, FKeyDrawInfo& OutDrawInfo) const
{
	OutDrawInfo.Brush = FAppStyle::Get().GetBrush("Sequencer.KeyDiamond");
	OutDrawInfo.ScreenSize = FVector2D(10, 10);
}

template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::GetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<FKeyPosition> OutKeyPositions) const
{
	ChannelType* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();

	if (Channel && Section)
	{
		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		TMovieSceneChannelData<ChannelValue> ChannelData = Channel->GetData();
		TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
		TArrayView<const ChannelValue> Values = ChannelData.GetValues();

		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			int32 KeyIndex = ChannelData.GetIndex(InKeys[Index]);
			if (KeyIndex != INDEX_NONE)
			{
				OutKeyPositions[Index].InputValue = Times[KeyIndex] / TickResolution;
				OutKeyPositions[Index].OutputValue = GetKeyValue(Values, KeyIndex);
			}
		}
	}
}

template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType)
{
	ChannelType* Channel = ChannelHandle.Get();
	UMovieSceneSection* Section = WeakSection.Get();

	if (Channel && Section && !IsReadOnly())
	{
		Section->MarkAsChanged();

		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		TMovieSceneChannelData<ChannelValue> ChannelData = Channel->GetData();
		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			int32 KeyIndex = ChannelData.GetIndex(InKeys[Index]);
			if (KeyIndex != INDEX_NONE)
			{
				FFrameNumber NewTime = (InKeyPositions[Index].InputValue * TickResolution).RoundToFrame();

				KeyIndex = ChannelData.MoveKey(KeyIndex, NewTime);
				SetKeyValue(KeyIndex, InKeyPositions[Index].OutputValue);

				Section->ExpandToFrame(NewTime);
			}
		}
		Channel->PostEditChange();
		if(WeakSequencer.IsValid())
		{ 
			const FMovieSceneChannelMetaData* MetaData = ChannelHandle.GetMetaData();
			WeakSequencer.Pin()->OnChannelChanged().Broadcast(MetaData, Section);
		}
		CurveModifiedDelegate.Broadcast();
	}
}

template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::GetCurveAttributes(FCurveAttributes& OutCurveAttributes) const
{
	OutCurveAttributes.SetPreExtrapolation(RCCE_None);
	OutCurveAttributes.SetPostExtrapolation(RCCE_None);
}

template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::GetTimeRange(double& MinTime, double& MaxTime) const
{
	ChannelType* Channel = ChannelHandle.Get();
	UMovieSceneSection* Section = WeakSection.Get();

	if (Channel && Section)
	{
		TArrayView<const FFrameNumber> Times = Channel->GetData().GetTimes();
		if (Times.Num() == 0)
		{
			MinTime = 0.f;
			MaxTime = 0.f;
		}
		else
		{
			FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
			double ToTime = TickResolution.AsInterval();
			MinTime = static_cast<double>(Times[0].Value) * ToTime;
			MaxTime = static_cast<double>(Times[Times.Num() - 1].Value) * ToTime;
		}
	}
}


template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::GetValueRange(double& MinValue, double& MaxValue) const
{
	ChannelType* Channel = ChannelHandle.Get();
	UMovieSceneSection* Section = WeakSection.Get();

	if (Channel && Section)
	{
		TArrayView<const FFrameNumber> Times = Channel->GetData().GetTimes();
		TArrayView<const ChannelValue> Values = Channel->GetData().GetValues();

		if (Times.Num() == 0)
		{
			// If there are no keys we just use the default value for the channel, defaulting to zero if there is no default.
			MinValue = MaxValue = Channel->GetDefault().Get(0.f);
		}
		else
		{
			FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
			double ToTime = TickResolution.AsInterval();
			int32 LastKeyIndex = Values.Num() - 1;
			MinValue = MaxValue = GetKeyValue(Values, 0);

			for (int32 i = 0; i < Values.Num(); i++)
			{
				double Key = GetKeyValue(Values, i);

				MinValue = FMath::Min(MinValue, Key);
				MaxValue = FMath::Max(MaxValue, Key);
			}
		}
	}
}

template <class ChannelType, class ChannelValue, class KeyType>
int32 FChannelCurveModel<ChannelType, ChannelValue, KeyType>::GetNumKeys() const
{
	ChannelType* Channel = ChannelHandle.Get();

	if (Channel)
	{
		TArrayView<const FFrameNumber> Times = Channel->GetData().GetTimes();

		return Times.Num();
	}

	return 0;
}

template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::GetNeighboringKeys(const FKeyHandle InKeyHandle, TOptional<FKeyHandle>& OutPreviousKeyHandle, TOptional<FKeyHandle>& OutNextKeyHandle) const
{
	ChannelType* Channel = ChannelHandle.Get();

	if (Channel)
	{
		TMovieSceneChannelData<ChannelValue> ChannelData = Channel->GetData();

		const int32 KeyIndex = ChannelData.GetIndex(InKeyHandle);
		if (KeyIndex != INDEX_NONE)
		{
			if (KeyIndex - 1 >= 0)
			{
				OutPreviousKeyHandle = ChannelData.GetHandle(KeyIndex - 1);
			}

			if (KeyIndex + 1 < ChannelData.GetTimes().Num())
			{
				OutNextKeyHandle = ChannelData.GetHandle(KeyIndex + 1);
			}
		}
	}
}

template <class ChannelType, class ChannelValue, class KeyType>
bool FChannelCurveModel<ChannelType, ChannelValue, KeyType>::Evaluate(double Time, double& OutValue) const
{
	ChannelType* Channel = ChannelHandle.Get();
	UMovieSceneSection* Section = WeakSection.Get();

	if (Channel && Section)
	{
		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		KeyType ThisValue = 0.f;
		if (Channel->Evaluate(Time * TickResolution, ThisValue))
		{
			OutValue = ThisValue;
			return true;
		}
	}

	return false;
}

template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::AddKeys(TArrayView<const FKeyPosition> InKeyPositions, TArrayView<const FKeyAttributes> InKeyAttributes, TArrayView<TOptional<FKeyHandle>>* OutKeyHandles)
{
	check(InKeyPositions.Num() == InKeyAttributes.Num() && (!OutKeyHandles || OutKeyHandles->Num() == InKeyPositions.Num()));

	ChannelType* Channel = ChannelHandle.Get();
	UMovieSceneSection* Section = WeakSection.Get();
	if (Channel && Section && !IsReadOnly())
	{
		Section->Modify();

		TMovieSceneChannelData<ChannelValue> ChannelData = Channel->GetData();
		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		TArray<FKeyHandle> NewKeyHandles;
		NewKeyHandles.SetNumUninitialized(InKeyPositions.Num());

		for (int32 Index = 0; Index < InKeyPositions.Num(); ++Index)
		{
			FKeyPosition Position = InKeyPositions[Index];

			FFrameNumber Time = (Position.InputValue * TickResolution).RoundToFrame();
			Section->ExpandToFrame(Time);

			ChannelValue Value = (ChannelValue)(Position.OutputValue);

			FKeyHandle NewHandle = ChannelData.UpdateOrAddKey(Time, Value);
			if (NewHandle != FKeyHandle::Invalid())
			{
				NewKeyHandles[Index] = NewHandle;

				if (OutKeyHandles)
				{
					(*OutKeyHandles)[Index] = NewHandle;
				}
			}
		}

		// We reuse SetKeyAttributes here as there is complex logic determining which parts of the attributes are valid to set.
		// For now we need to duplicate the new key handle array due to API mismatch. This will auto calculate tangents if needed.
		SetKeyAttributes(NewKeyHandles, InKeyAttributes);
		Channel->PostEditChange();
		if (WeakSequencer.IsValid())
		{
			const FMovieSceneChannelMetaData* MetaData = ChannelHandle.GetMetaData();
			WeakSequencer.Pin()->OnChannelChanged().Broadcast(MetaData, Section);
		}
		CurveModifiedDelegate.Broadcast();
	}
}

template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::RemoveKeys(TArrayView<const FKeyHandle> InKeys)
{
	ChannelType* Channel = ChannelHandle.Get();
	UMovieSceneSection* Section = WeakSection.Get();
	if (Channel && Section && !IsReadOnly())
	{
		Section->Modify();

		TMovieSceneChannelData<ChannelValue> ChannelData = Channel->GetData();

		for (FKeyHandle Handle : InKeys)
		{
			int32 KeyIndex = ChannelData.GetIndex(Handle);
			if (KeyIndex != INDEX_NONE)
			{
				ChannelData.RemoveKey(KeyIndex);
			}
		}
		Channel->PostEditChange();
		if (WeakSequencer.IsValid())
		{
			const FMovieSceneChannelMetaData* MetaData = ChannelHandle.GetMetaData();
			WeakSequencer.Pin()->OnChannelChanged().Broadcast(MetaData, Section);
		}
		CurveModifiedDelegate.Broadcast();
	}
}

template <class ChannelType, class ChannelValue, class KeyType>
bool FChannelCurveModel<ChannelType, ChannelValue, KeyType>::IsReadOnly() const
{
	UMovieSceneSection* Section = WeakSection.Get();
	if (Section)
	{
		return Section->IsReadOnly();
	}

	return false;
}

template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::FixupCurve()
{
	if (UMovieSceneSection* Section = WeakSection.Get())
	{
		FMovieSceneChannelProxy* NewChannelProxy = &Section->GetChannelProxy();
		ChannelHandle = NewChannelProxy->MakeHandle<ChannelType>(ChannelHandle.GetChannelIndex());
		OnDestroyHandle = NewChannelProxy->OnDestroy.AddRaw(this, &FChannelCurveModel<ChannelType, ChannelValue, KeyType>::FixupCurve);
	}
}
template <class ChannelType, class ChannelValue, class KeyType>
void FChannelCurveModel<ChannelType, ChannelValue, KeyType>::GetCurveColorObjectAndName(UObject** OutObject, FString& OutName) const
{
	if (UMovieSceneSection* Section = WeakSection.Get())
	{
		*OutObject = Section->GetImplicitObjectOwner();

		if (const FMovieSceneChannelMetaData* MetaData = ChannelHandle.GetMetaData())
		{
			OutName = FString::Printf(TEXT( "%s.%s" ), *MetaData->Group.ToString(), *MetaData->DisplayText.ToString());
			return;
		}
		OutName = GetIntentionName();
		return;
	}
	// Just call base if it doesn't work
	FCurveModel::GetCurveColorObjectAndName(OutObject, OutName);
}


// Explicit template instantiation
template class FChannelCurveModel<FMovieSceneDoubleChannel, FMovieSceneDoubleValue, double>;
template class FChannelCurveModel<FMovieSceneFloatChannel, FMovieSceneFloatValue, float>;
template class FChannelCurveModel<FMovieSceneIntegerChannel, int32, int32>;
template class FChannelCurveModel<FMovieSceneBoolChannel, bool, bool>;

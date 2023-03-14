// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "MovieSceneSection.h"
#include "ISequencer.h"
#include "MovieSceneCommonHelpers.h"
#include "Channels/MovieSceneChannelTraits.h"
#include "SequencerChannelTraits.h"
#include "Channels/MovieSceneChannelHandle.h"
#include "MovieSceneTimeHelpers.h"
#include "Channels/MovieSceneFloatChannel.h"


template<typename ChannelType, typename ValueType>
struct TSequencerKeyEditor
{
	TSequencerKeyEditor()
	{}

	TSequencerKeyEditor(
		FGuid                                    InObjectBindingID,
		TMovieSceneChannelHandle<ChannelType>    InChannelHandle,
		TWeakObjectPtr<UMovieSceneSection>       InWeakSection,
		TWeakPtr<ISequencer>                     InWeakSequencer,
		TWeakPtr<FTrackInstancePropertyBindings> InWeakPropertyBindings,
		TFunction<TOptional<ValueType>(UObject&, FTrackInstancePropertyBindings*)> InOnGetExternalValue
	)
		: ObjectBindingID(InObjectBindingID)
		, ChannelHandle(InChannelHandle)
		, WeakSection(InWeakSection)
		, WeakSequencer(InWeakSequencer)
		, WeakPropertyBindings(InWeakPropertyBindings)
		, OnGetExternalValue(InOnGetExternalValue)
	{}

	static TOptional<ValueType> Get(const FGuid& ObjectBindingID, ISequencer* Sequencer, FTrackInstancePropertyBindings* PropertyBindings, const TFunction<TOptional<ValueType>(UObject&, FTrackInstancePropertyBindings*)>& OnGetExternalValue)
	{
		if (!Sequencer || !ObjectBindingID.IsValid() || !OnGetExternalValue)
		{
			return TOptional<ValueType>();
		}

		for (TWeakObjectPtr<> WeakObject : Sequencer->FindBoundObjects(ObjectBindingID, Sequencer->GetFocusedTemplateID()))
		{
			if (UObject* Object = WeakObject.Get())
			{
				TOptional<ValueType> ExternalValue = OnGetExternalValue(*Object, PropertyBindings);
				if (ExternalValue.IsSet())
				{
					return ExternalValue;
				}
			}
		}

		return TOptional<ValueType>();
	}

	TOptional<ValueType> GetExternalValue() const
	{
		return Get(ObjectBindingID, WeakSequencer.Pin().Get(), WeakPropertyBindings.Pin().Get(), OnGetExternalValue);
	}

	ValueType GetCurrentValue() const
	{
		using namespace UE::MovieScene;

		ChannelType* Channel = ChannelHandle.Get();
		ISequencer* Sequencer = WeakSequencer.Pin().Get();
		UMovieSceneSection* OwningSection = WeakSection.Get();

		ValueType Result{};

		if (Channel && Sequencer && OwningSection)
		{
			const FFrameTime CurrentTime = UE::MovieScene::ClampToDiscreteRange(Sequencer->GetLocalTime().Time, OwningSection->GetRange());
			//If we have no keys and no default, key with the external value if it exists
			if (!EvaluateChannel(OwningSection, Channel, CurrentTime, Result))
			{
				if (TOptional<ValueType> ExternalValue = GetExternalValue())
				{
					if (ExternalValue.IsSet())
					{
						Result = ExternalValue.GetValue();
					}
				}
			}
		}

		return Result;
	}

	void SetValue(const ValueType& InValue)
	{
		using namespace UE::MovieScene;
		using namespace Sequencer;

		UMovieSceneSection* OwningSection = WeakSection.Get();
		if (!OwningSection)
		{
			return;
		}

		OwningSection->SetFlags(RF_Transactional);

		ChannelType* Channel = ChannelHandle.Get();
		ISequencer* Sequencer = WeakSequencer.Pin().Get();

		if (!OwningSection->TryModify() || !Channel || !Sequencer)
		{
			return;
		}

		const FFrameNumber CurrentTime = Sequencer->GetLocalTime().Time.FloorToFrame();
		const bool  bAutoSetTrackDefaults = Sequencer->GetAutoSetTrackDefaults();

		EMovieSceneKeyInterpolation Interpolation = Sequencer->GetKeyInterpolation();

		TArray<FKeyHandle> KeysAtCurrentTime;
		Channel->GetKeys(TRange<FFrameNumber>(CurrentTime), nullptr, &KeysAtCurrentTime);

		if (KeysAtCurrentTime.Num() > 0)
		{
			AssignValue(Channel, KeysAtCurrentTime[0], InValue);
		}
		else
		{
			bool bHasAnyKeys = Channel->GetNumKeys() != 0;

			if (bHasAnyKeys || bAutoSetTrackDefaults == false)
			{
				// When auto setting track defaults are disabled, add a key even when it's empty so that the changed
				// value is saved and is propagated to the property.
				AddKeyToChannel(Channel, CurrentTime, InValue, Interpolation);
				bHasAnyKeys = Channel->GetNumKeys() != 0;
			}

			if (bHasAnyKeys)
			{
				TRange<FFrameNumber> KeyRange     = TRange<FFrameNumber>(CurrentTime);
				TRange<FFrameNumber> SectionRange = OwningSection->GetRange();

				if (!SectionRange.Contains(KeyRange))
				{
					OwningSection->SetRange(TRange<FFrameNumber>::Hull(KeyRange, SectionRange));
				}
			}
		}

		// Always update the default value when auto-set default values is enabled so that the last changes
		// are always saved to the track.
		if (bAutoSetTrackDefaults)
		{
			SetChannelDefault(Channel, InValue);
		}
		 
		//need to tell channel change happened (float will call AutoSetTangents())
		Channel->PostEditChange();

		const FMovieSceneChannelMetaData* MetaData = ChannelHandle.GetMetaData();
		Sequencer->OnChannelChanged().Broadcast(MetaData, OwningSection);

	}

	void SetValueWithNotify(const ValueType& InValue, EMovieSceneDataChangeType NotifyType = EMovieSceneDataChangeType::TrackValueChanged)
	{
		SetValue(InValue);
		if (ISequencer* Sequencer = WeakSequencer.Pin().Get())
		{
			Sequencer->NotifyMovieSceneDataChanged(NotifyType);
		}
	}

	const FGuid& GetObjectBindingID() const
	{
		return ObjectBindingID;
	}

	ISequencer* GetSequencer() const
	{
		return WeakSequencer.Pin().Get();
	}

	FTrackInstancePropertyBindings* GetPropertyBindings() const
	{
		return WeakPropertyBindings.Pin().Get();
	}

private:

	FGuid ObjectBindingID;
	TMovieSceneChannelHandle<ChannelType> ChannelHandle;
	TWeakObjectPtr<UMovieSceneSection> WeakSection;
	TWeakPtr<ISequencer> WeakSequencer;
	TWeakPtr<FTrackInstancePropertyBindings> WeakPropertyBindings;
	TFunction<TOptional<ValueType>(UObject&, FTrackInstancePropertyBindings*)> OnGetExternalValue;
};

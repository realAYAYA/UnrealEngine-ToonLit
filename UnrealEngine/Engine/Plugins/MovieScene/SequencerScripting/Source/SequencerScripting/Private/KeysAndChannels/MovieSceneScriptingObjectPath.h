// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneScriptingChannel.h"
#include "Channels/MovieSceneChannelTraits.h"
#include "Channels/MovieSceneChannelHandle.h"
#include "Channels/MovieSceneObjectPathChannel.h"
#include "KeysAndChannels/MovieSceneScriptingChannel.h"
#include "KeyParams.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"

#include "MovieSceneScriptingObjectPath.generated.h"

/**
* Exposes a Sequencer Object Path type key to Python/Blueprints.
* Stores a reference to the data so changes to this class are forwarded onto the underlying data structures.
*/
UCLASS(BlueprintType)
class UMovieSceneScriptingObjectPathKey : public UMovieSceneScriptingKey, public TMovieSceneScriptingKey<FMovieSceneObjectPathChannel, FMovieSceneObjectPathChannelKeyValue>
{
	GENERATED_BODY()
public:
	/**
	 * Gets the time for this key from the owning channel.
	 * @param TimeUnit	Should the time be returned in Display Rate frames (possibly with a sub-frame value) or in Tick Resolution with no sub-frame values?
	 * @return			The time of this key which combines both the frame number and the sub-frame it is on. Sub-frame will be zero if you request Tick Resolution.	
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Keys", meta = (DisplayName = "Get Time (Object Path)"))
	virtual FFrameTime GetTime(ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate) const override { return GetTimeFromChannel(KeyHandle, OwningSequence, TimeUnit); }
	
	/**
	 * Sets the time for this key in the owning channel. Will replace any key that already exists at that frame number in this channel.
	 * @param NewFrameNumber	What frame should this key be moved to? This should be in the time unit specified by TimeUnit.
	 * @param SubFrame		If using Display Rate time, what is the sub-frame this should go to? Clamped [0-1), and ignored with when TimeUnit is set to Tick Resolution. 
	 * @param TimeUnit		Should the NewFrameNumber be interpreted as Display Rate frames or in Tick Resolution?
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Keys", meta = (DisplayName = "Set Time (Object Path)"))
	void SetTime(const FFrameNumber& NewFrameNumber, float SubFrame = 0.f, ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate) { SetTimeInChannel(KeyHandle, OwningSequence, NewFrameNumber, TimeUnit, SubFrame); }

	/**
	 * Gets the value for this key from the owning channel.
	 * @return	The object for this key.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Keys", meta = (DisplayName = "Get Value (Object Path)"))
	UObject* GetValue() const
	{
		FMovieSceneObjectPathChannelKeyValue Value = GetValueFromChannel(KeyHandle);
		return Value.Get();
	}

	/**
	 * Sets the value for this key, reflecting it in the owning channel.
	 * @param InNewValue	The new object for this key.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Keys", meta = (DisplayName = "Set Value (Object Path)"))
	void SetValue(UObject* InNewValue)
	{
		FMovieSceneObjectPathChannelKeyValue ReferenceKey = FMovieSceneObjectPathChannelKeyValue(InNewValue);
		SetValueInChannel(KeyHandle, ReferenceKey);
	}
};

UCLASS(BlueprintType)
class UMovieSceneScriptingObjectPathChannel : public UMovieSceneScriptingChannel
{
	GENERATED_BODY()

	using Impl = TMovieSceneScriptingChannel<FMovieSceneObjectPathChannel, UMovieSceneScriptingObjectPathKey, FMovieSceneObjectPathChannelKeyValue>;

public:
	/**
	 * Add a key to this channel. This initializes a new key and returns a reference to it.
	 * @param	InTime			The frame this key should go on. Respects TimeUnit to determine if it is a display rate frame or a tick resolution frame.
	 * @param	NewValue		The value that this key should be created with.
	 * @param	SubFrame		Optional [0-1) clamped sub-frame to put this key on. Ignored if TimeUnit is set to Tick Resolution.
	 * @param	TimeUnit 		Is the specified InTime in Display Rate frames or Tick Resolution.
	 * @return	The key that was created with the specified values at the specified time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Keys", meta = (DisplayName = "Add Key (Object Path)"))
	UMovieSceneScriptingObjectPathKey* AddKey(const FFrameNumber InTime, UObject* NewValue, float SubFrame = 0.f, ESequenceTimeUnit TimeUnit = ESequenceTimeUnit::DisplayRate)
	{
		FMovieSceneObjectPathChannelKeyValue ReferenceKey = FMovieSceneObjectPathChannelKeyValue(NewValue);
		return Impl::AddKeyInChannel(ChannelHandle, OwningSequence, OwningSection,InTime, ReferenceKey, SubFrame, TimeUnit, EMovieSceneKeyInterpolation::Auto);
	}

	/**
	 * Removes the specified key. Does nothing if the key is not specified or the key belongs to another channel.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Keys", meta = (DisplayName = "Remove Key (Object Path)"))
	virtual void RemoveKey(UMovieSceneScriptingKey* Key)
	{
		Impl::RemoveKeyFromChannel(ChannelHandle, Key);
	}

	/**
	 * Gets all of the keys in this channel.
	 * @return	An array of UMovieSceneScriptingObjectPathKey contained by this channel.
	 *			Returns all keys even if clipped by the owning section's boundaries or outside of the current sequence play range.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Keys", meta = (DisplayName = "Get Keys (Object Path)"))
	virtual TArray<UMovieSceneScriptingKey*> GetKeys() const override
	{
		return Impl::GetKeysInChannel(ChannelHandle, OwningSequence, OwningSection);
	}

	/**
	 * Set this channel's default value that should be used when no keys are present.
	 * Sets bHasDefaultValue to true automatically.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Keys", meta = (DisplayName = "Set Default (Object Path)"))
	void SetDefault(UObject* InDefaultValue)
	{
		FMovieSceneObjectPathChannel* Channel = ChannelHandle.Get();
		if (Channel)
		{
			Channel->SetDefault(InDefaultValue);

#if WITH_EDITOR
			const FMovieSceneChannelMetaData* MetaData = ChannelHandle.GetMetaData();
			if (MetaData && OwningSection.IsValid() && OwningSequence.IsValid() && OwningSequence->GetMovieScene())
			{
				OwningSection.Get()->MarkAsChanged();
				OwningSequence->GetMovieScene()->OnChannelChanged().Broadcast(MetaData, OwningSection.Get());
			}
#endif
			return;
		}

		FFrame::KismetExecutionMessage(TEXT("Invalid ChannelHandle for MovieSceneScriptingChannel, failed to set default value."), ELogVerbosity::Error);
	}

	/**
	 * Get this channel's default value that will be used when no keys are present. Only a valid
	 * value when HasDefault() returns true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Keys", meta = (DisplayName = "Get Default (Object Path)"))
	UObject* GetDefault() const
	{
		// FMovieSceneObjectPathChannelKeyValue doesn't implement GetDefault via TOptional, so we're wrapping this function by hand as well.
		FMovieSceneObjectPathChannel* Channel = ChannelHandle.Get();
		if (Channel)
		{
			FMovieSceneObjectPathChannelKeyValue ReferenceKey = Channel->GetDefault();
			return ReferenceKey.Get();
		}

		FFrame::KismetExecutionMessage(TEXT("Invalid ChannelHandle for MovieSceneScriptingChannel, failed to get default value."), ELogVerbosity::Error);
		return nullptr;
	}

	/**
	 * Remove this channel's default value causing the channel to have no effect where no keys are present
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Keys", meta = (DisplayName = "Remove Default (Object Path)"))
	void RemoveDefault()
	{
		// FMovieSceneObjectPathChannelKeyValue doesn't implement RemoveDefault, instead it implements ClearDefault(). Wrapping this function by hand,
		// and not falling back to the template as a result, but keeping the same function name so it is consistent with the other scripting channels.
		FMovieSceneObjectPathChannel* Channel = ChannelHandle.Get();
		if (Channel)
		{
			Channel->ClearDefault();
			return;
		}
		FFrame::KismetExecutionMessage(TEXT("Invalid ChannelHandle for MovieSceneScriptingChannel, failed to remove default value."), ELogVerbosity::Error);
	}

	/**
	* @return Does this channel have a default value set?
	*/
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Keys", meta = (DisplayName = "Has Default (Object Path)"))
	bool HasDefault() const
	{
		return GetDefault() != nullptr;
	}
public:
	TWeakObjectPtr<UMovieSceneSequence> OwningSequence;
	TWeakObjectPtr<UMovieSceneSection> OwningSection;
	TMovieSceneChannelHandle<FMovieSceneObjectPathChannel> ChannelHandle;
};

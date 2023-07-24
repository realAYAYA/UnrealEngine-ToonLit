// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "MovieSceneSection.h"
#include "Channels/MovieSceneChannelHandle.h"
#include "Channels/MovieSceneChannel.h"
#include "Curves/KeyHandle.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "MovieSceneKeyProxy.generated.h"

UINTERFACE()
class MOVIESCENE_API UMovieSceneKeyProxy : public UInterface
{
	GENERATED_BODY()
};


/**
 * Interface that can be implemented by any object that is used as a key editing proxy for a moviescene channel.
 * When used, UpdateValuesFromRawData should be called every frame to optionally retrieve the current values
 * of the key for this proxy.
 */
class MOVIESCENE_API IMovieSceneKeyProxy
{
public:

	GENERATED_BODY()

	/**
	 * To be called by the edit interface to update this instance's properties with the underlying raw data
	 */
	virtual void UpdateValuesFromRawData() = 0;

protected:

	/**
	 * Implementation function that sets the underlying key time/value to the specified values if possible.
	 * If the section is locked, InOutValue and InOutTime will be reset back to the current key's value
	 *
	 * @param InChannelHandle      The channel on which the underlying key value resides
	 * @param InSection            The section that owns the channel
	 * @param InKeyHandle          A handle to the key that we are reflecting
	 * @param InOutValue           Value to assign to the underlying key. If the section is locked, this will receive the existing key's value without changing the underlying key value.
	 * @param InOutTime            Time to move the underlying key to. If the section is locked, this will receive the existing key's time without changing the underlying time.
	 */
	template<typename ChannelType, typename ValueType>
	void OnProxyValueChanged(TMovieSceneChannelHandle<ChannelType> InChannelHandle, UMovieSceneSection* InSection, FKeyHandle InKeyHandle, ValueType& InOutValue, FFrameNumber& InOutTime);

	/**
	 * Implementation function that retrieves the underlying key time/value and applies then to the specified value and time parameters. Normally called once per tick.
	 *
	 * @param InChannelHandle      The channel on which the underlying key value resides
	 * @param InKeyHandle          A handle to the key that we are reflecting
	 * @param OutValue             (Out) Value to receive the underlying key's value
	 * @param OutTime              (Out) Time to receive the underlying key time
	 */
	template<typename ChannelType, typename ValueType>
	void RefreshCurrentValue(TMovieSceneChannelHandle<ChannelType> InChannelHandle, FKeyHandle InKeyHandle, ValueType& OutValue, FFrameNumber& OutTime);
};



template<typename ChannelType, typename ValueType>
void IMovieSceneKeyProxy::OnProxyValueChanged(TMovieSceneChannelHandle<ChannelType> InChannelHandle, UMovieSceneSection* InSection, FKeyHandle InKeyHandle, ValueType& InOutValue, FFrameNumber& InOutTime)
{
	auto* Channel = InChannelHandle.Get();
	if (!Channel || !InSection)
	{
		return;
	}

	auto ChannelData = Channel->GetData();

	int32 KeyIndex = ChannelData.GetIndex(InKeyHandle);
	if (KeyIndex != INDEX_NONE)
	{
		// If we have no section, or it's locked, don't let the user change the value
		if (!InSection || !InSection->TryModify())
		{
			InOutTime  = ChannelData.GetTimes()[KeyIndex];
			InOutValue = ChannelData.GetValues()[KeyIndex];
		}
		else
		{
			ChannelData.GetValues()[KeyIndex] = InOutValue;

			ChannelData.MoveKey(KeyIndex, InOutTime);
			InSection->ExpandToFrame(InOutTime);
		}
		Channel->PostEditChange();
	}
}



template<typename ChannelType, typename ValueType>
void IMovieSceneKeyProxy::RefreshCurrentValue(TMovieSceneChannelHandle<ChannelType> InChannelHandle, FKeyHandle InKeyHandle, ValueType& OutValue, FFrameNumber& OutTime)
{
	auto* Channel = InChannelHandle.Get();
	if (Channel)
	{
		auto ChannelData = Channel->GetData();
		int32 KeyIndex = ChannelData.GetIndex(InKeyHandle);
		if (KeyIndex != INDEX_NONE && KeyIndex < FMath::Min(ChannelData.GetValues().Num(), ChannelData.GetTimes().Num()))
		{
			OutValue = ChannelData.GetValues()[KeyIndex];
			OutTime  = ChannelData.GetTimes()[KeyIndex];
		}
	}
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Channels/IntegerChannelKeyProxy.h"

#include "Channels/MovieSceneIntegerChannel.h"
#include "HAL/PlatformCrt.h"

struct FPropertyChangedEvent;

void UIntegerChannelKeyProxy::Initialize(FKeyHandle InKeyHandle, TMovieSceneChannelHandle<FMovieSceneIntegerChannel> InChannelHandle, TWeakObjectPtr<UMovieSceneSection> InWeakSection)
{
	KeyHandle     = InKeyHandle;
	ChannelHandle = InChannelHandle;
	WeakSection   = InWeakSection;
}

void UIntegerChannelKeyProxy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnProxyValueChanged(ChannelHandle, WeakSection.Get(), KeyHandle, Value, Time);
}

void UIntegerChannelKeyProxy::UpdateValuesFromRawData()
{
	RefreshCurrentValue(ChannelHandle, KeyHandle, Value, Time);
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Channels/BoolChannelKeyProxy.h"

#include "Channels/MovieSceneBoolChannel.h"
#include "HAL/PlatformCrt.h"

struct FPropertyChangedEvent;

void UBoolChannelKeyProxy::Initialize(FKeyHandle InKeyHandle, TMovieSceneChannelHandle<FMovieSceneBoolChannel> InChannelHandle, TWeakObjectPtr<UMovieSceneSection> InWeakSection)
{
	KeyHandle     = InKeyHandle;
	ChannelHandle = InChannelHandle;
	WeakSection   = InWeakSection;
}

void UBoolChannelKeyProxy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnProxyValueChanged(ChannelHandle, WeakSection.Get(), KeyHandle, bValue, Time);
}

void UBoolChannelKeyProxy::UpdateValuesFromRawData()
{
	RefreshCurrentValue(ChannelHandle, KeyHandle, bValue, Time);
}
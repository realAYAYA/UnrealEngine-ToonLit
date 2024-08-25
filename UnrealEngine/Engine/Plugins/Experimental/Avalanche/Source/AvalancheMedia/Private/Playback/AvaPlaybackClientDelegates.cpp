// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/AvaPlaybackClientDelegates.h"

namespace UE::AvaPlaybackClient::Delegates
{
	namespace Private
	{
		static FOnConnectionEvent OnConnectionEvent;
		static FOnPlaybackStatusChanged OnPlaybackStatusChanged;
		static FOnPlaybackAssetStatusChanged OnPlaybackAssetStatusChanged;
		static FOnPlaybackSequenceEvent OnPlaybackSequenceEvent;
		static FOnPlaybackTransitionEvent OnPlaybackTransitionEvent;
	}
	
	FOnConnectionEvent& GetOnConnectionEvent()
	{
		return Private::OnConnectionEvent;
	}
	
	FOnPlaybackStatusChanged& GetOnPlaybackStatusChanged()
	{
		return Private::OnPlaybackStatusChanged;
	}
	
	FOnPlaybackAssetStatusChanged& GetOnPlaybackAssetStatusChanged()
	{
		return Private::OnPlaybackAssetStatusChanged;	
	}
	
	FOnPlaybackSequenceEvent& GetOnPlaybackSequenceEvent()
	{
		return Private::OnPlaybackSequenceEvent;
	}

	FOnPlaybackTransitionEvent& GetOnPlaybackTransitionEvent()
	{
		return Private::OnPlaybackTransitionEvent;
	}
}

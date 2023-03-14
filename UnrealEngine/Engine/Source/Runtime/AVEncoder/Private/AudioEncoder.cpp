// Copyright Epic Games, Inc. All Rights Reserved

#include "AudioEncoder.h"
#include "Misc/ScopeLock.h"

namespace AVEncoder
{
	void FAudioEncoder::RegisterListener(IAudioEncoderListener& Listener)
	{
		FScopeLock lock{ &ListenersMutex };
		check(Listeners.Find(&Listener) == INDEX_NONE);
		Listeners.AddUnique(&Listener);
	}

	void FAudioEncoder::UnregisterListener(IAudioEncoderListener& Listener)
	{
		FScopeLock lock{ &ListenersMutex };
		int32 Count = Listeners.Remove(&Listener);
		check(Count == 1);
	}

	void FAudioEncoder::OnEncodedAudioFrame(const FMediaPacket& Packet)
	{
		FScopeLock lock{ &ListenersMutex };
		for (auto&& L : Listeners)
		{
			L->OnEncodedAudioFrame(Packet);
		}
	}
}

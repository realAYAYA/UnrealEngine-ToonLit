// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "MediaPacket.h"
#include "Misc/Timespan.h"
#include "SampleBuffer.h"

namespace AVEncoder
{
	struct FAudioFrame
	{
		FTimespan Timestamp;
		FTimespan Duration;
		Audio::TSampleBuffer<float> Data;
	};

	class IAudioEncoderListener
	{
	public:
		virtual void OnEncodedAudioFrame(const FMediaPacket& Packet) = 0;
	};

	class AVENCODER_API FAudioEncoder
	{
	public:
		virtual ~FAudioEncoder() {}
		virtual const TCHAR* GetName() const = 0;
		virtual const TCHAR* GetType() const = 0;
		virtual bool Initialize(const FAudioConfig& Config) = 0;

		/**
		* Shutdown MUST be called before destruction
		*/
		virtual void Shutdown() = 0;

		virtual void Encode(const FAudioFrame& Frame) = 0;
		virtual FAudioConfig GetConfig() const = 0;

		virtual void RegisterListener(IAudioEncoderListener& Listener);
		virtual void UnregisterListener(IAudioEncoderListener& Listener);

	protected:
		void OnEncodedAudioFrame(const FMediaPacket& Packet);
	private:

		FCriticalSection ListenersMutex;
		TArray<IAudioEncoderListener*> Listeners;

	};
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Templates/SharedPointerInternals.h"
#include "Templates/UniquePtr.h"
#include "AudioMixer.h"

// Forward declares.
class USoundWave;
class FSoundWaveProxy;


namespace Audio
{
	// Forward declares.
	struct IDecoderInput;
	using FSoundWaveProxyPtr = TSharedPtr<FSoundWaveProxy, ESPMode::ThreadSafe>;

	// Just loose for now.
	AUDIOCODECENGINE_API TUniquePtr<IDecoderInput> CreateBackCompatDecoderInput(FName InOldFormatName, const FSoundWaveProxyPtr& InSoundWave);
}

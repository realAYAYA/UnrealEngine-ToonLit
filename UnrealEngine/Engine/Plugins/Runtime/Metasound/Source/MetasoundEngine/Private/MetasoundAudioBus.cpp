// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundAudioBus.h"

#include "AudioDevice.h"
#include "DecoderInputFactory.h"

namespace Metasound
{
	FAudioBusAsset::FAudioBusAsset(const TUniquePtr<Audio::IProxyData>& InInitData)
	{
		if (InInitData.IsValid())
		{
			if (InInitData->CheckTypeCast<FAudioBusProxy>())
			{
				AudioBusProxy = MakeShared<FAudioBusProxy, ESPMode::ThreadSafe>(InInitData->GetAs<FAudioBusProxy>());
			}
		}
	}
}

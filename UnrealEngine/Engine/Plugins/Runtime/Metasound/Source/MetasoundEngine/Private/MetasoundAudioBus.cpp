// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundAudioBus.h"

#include "AudioDevice.h"

namespace Metasound
{
	FAudioBusAsset::FAudioBusAsset(const TSharedPtr<Audio::IProxyData>& InInitData)
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

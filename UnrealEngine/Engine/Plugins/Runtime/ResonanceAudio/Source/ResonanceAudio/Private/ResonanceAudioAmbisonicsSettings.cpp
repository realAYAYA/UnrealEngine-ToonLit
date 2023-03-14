//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

#include "ResonanceAudioAmbisonicsSettings.h"

#include "AudioDevice.h"
#include "HAL/LowLevelMemTracker.h"
#include "ResonanceAudioAmbisonicsSettingsProxy.h"
#include "ResonanceAudioCommon.h"

TUniquePtr<ISoundfieldEncodingSettingsProxy> UResonanceAudioSoundfieldSettings::GetProxy() const
{
	AUDIO_SPATIALIZATION_PLUGIN_LLM_SCOPE
	FResonanceAmbisonicsSettingsProxy* Proxy = new FResonanceAmbisonicsSettingsProxy();

	switch (RenderMode)
	{
		case EResonanceRenderMode::StereoPanning:
		{
			Proxy->RenderingMode = vraudio::kStereoPanning;
			break;
		}
		case EResonanceRenderMode::BinauralLowQuality:
		{
			Proxy->RenderingMode = vraudio::kBinauralLowQuality;
			break;
		}
		case EResonanceRenderMode::BinauralMediumQuality:
		{
			Proxy->RenderingMode = vraudio::kBinauralMediumQuality;
			break;
		}
		case EResonanceRenderMode::BinauralHighQuality:
		{
			Proxy->RenderingMode = vraudio::kBinauralHighQuality;
			break;
		}
		case EResonanceRenderMode::RoomEffectsOnly:
		{
			Proxy->RenderingMode = vraudio::kRoomEffectsOnly;
			break;
		}
		default:
		{
			checkNoEntry();
		}
	}

	return TUniquePtr<ISoundfieldEncodingSettingsProxy>(Proxy);
}

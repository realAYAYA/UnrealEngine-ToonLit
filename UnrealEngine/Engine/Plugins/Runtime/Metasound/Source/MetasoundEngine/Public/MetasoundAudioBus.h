// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundDataReference.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "IAudioProxyInitializer.h"
#include "Sound/AudioBus.h"

namespace Metasound
{
	// Forward declare ReadRef
	class FAudioBus;

	// Metasound data type that holds onto a weak ptr. Mostly used as a placeholder until we have a proper proxy type.
	class METASOUNDENGINE_API FAudioBusAsset
	{
		FAudioBusProxyPtr AudioBusProxy;
		
	public:

		FAudioBusAsset() = default;
		FAudioBusAsset(const FAudioBusAsset&) = default;
		FAudioBusAsset& operator=(const FAudioBusAsset& Other) = default;

		FAudioBusAsset(const TUniquePtr<Audio::IProxyData>& InInitData);

		const FAudioBusProxyPtr& GetAudioBusProxy() const
		{
			return AudioBusProxy;
		}
		
	};

	DECLARE_METASOUND_DATA_REFERENCE_TYPES(FAudioBusAsset, METASOUNDENGINE_API, FAudioBusAssetTypeInfo, FAudioBusAssetReadRef, FAudioBusAssetWriteRef)
}
 

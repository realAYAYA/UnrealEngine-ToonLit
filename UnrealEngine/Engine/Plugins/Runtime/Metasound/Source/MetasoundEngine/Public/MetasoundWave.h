// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundDataReference.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "IAudioProxyInitializer.h"
#include "Sound/SoundWave.h"
#include "DSP/InterpolatedLinearPitchShifter.h"


namespace Metasound
{
	// Forward declare ReadRef
	class FWaveAsset;
	typedef TDataReadReference<FWaveAsset> FWaveAssetReadRef;

	// Helper utility to test if exact types are required for a datatype.
	template <>
	struct TIsExplicit<FWaveAsset>
	{
		static constexpr bool Value = true;
	};

	// Metasound data type that holds onto a weak ptr. Mostly used as a placeholder until we have a proper proxy type.
	class METASOUNDENGINE_API FWaveAsset
	{
		FSoundWaveProxyPtr SoundWaveProxy;
	public:

		FWaveAsset() = default;
		FWaveAsset(const FWaveAsset&) = default;
		FWaveAsset& operator=(const FWaveAsset& Other) = default;

		FWaveAsset(const TSharedPtr<Audio::IProxyData>& InInitData);

		bool IsSoundWaveValid() const;

		const FSoundWaveProxyPtr& GetSoundWaveProxy() const
		{
			return SoundWaveProxy;
		}

		const FSoundWaveProxy* operator->() const
		{
			return SoundWaveProxy.Get();
		}

		FSoundWaveProxy* operator->()
		{
			return SoundWaveProxy.Get();
		}
	};

	DECLARE_METASOUND_DATA_REFERENCE_TYPES(FWaveAsset, METASOUNDENGINE_API, FWaveAssetTypeInfo, FWaveAssetReadRef, FWaveAssetWriteRef)
}
 

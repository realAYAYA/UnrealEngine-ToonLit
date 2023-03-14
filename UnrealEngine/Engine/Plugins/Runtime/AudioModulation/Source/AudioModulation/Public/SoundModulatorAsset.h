// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_AUDIOMODULATION_METASOUND_SUPPORT
#include "IAudioModulation.h"
#include "IAudioProxyInitializer.h"
#include "Internationalization/Text.h"

#include "MetasoundDataReferenceMacro.h"

namespace AudioModulation
{
	extern const FString AUDIOMODULATION_API PluginAuthor;
	extern const FText AUDIOMODULATION_API PluginNodeMissingPrompt;

	class AUDIOMODULATION_API FSoundModulatorAsset
	{
		FSoundModulatorAssetProxyPtr Proxy;

	public:
		FSoundModulatorAsset() = default;

		FSoundModulatorAsset(const FSoundModulatorAsset& InOther)
			: Proxy(InOther.Proxy)
		{
		}

		FSoundModulatorAsset& operator=(const FSoundModulatorAsset& InOther)
		{
			Proxy = InOther.Proxy;
			return *this;
		}

		FSoundModulatorAsset& operator=(FSoundModulatorAsset&& InOther)
		{
			Proxy = MoveTemp(InOther.Proxy);
			return *this;
		}

		FSoundModulatorAsset(const Audio::IProxyDataPtr& InInitData);

		Audio::FModulatorId GetModulatorId() const
		{
			if (Proxy.IsValid())
			{
				return Proxy->GetModulatorId();
			}

			return INDEX_NONE;
		}

		bool IsValid() const
		{
			return Proxy.IsValid();
		}

		const FSoundModulatorAssetProxyPtr& GetProxy() const
		{
			return Proxy;
		}

		const FSoundModulatorAssetProxy* operator->() const
		{
			return Proxy.Get();
		}

		FSoundModulatorAssetProxy* operator->()
		{
			return Proxy.Get();
		}
	};

	class AUDIOMODULATION_API FSoundModulationParameterAsset
	{
		FSoundModulationParameterAssetProxyPtr Proxy;

	public:
		FSoundModulationParameterAsset() = default;
		FSoundModulationParameterAsset(const FSoundModulationParameterAsset& InOther) = default;
		FSoundModulationParameterAsset(FSoundModulationParameterAsset&& InOther) = default;
		FSoundModulationParameterAsset(const Audio::IProxyDataPtr& InInitData);

		FSoundModulationParameterAsset& operator=(const FSoundModulationParameterAsset& InOther) = default;
		FSoundModulationParameterAsset& operator=(FSoundModulationParameterAsset && InOther) = default;

		bool IsValid() const
		{
			return Proxy.IsValid();
		}

		const FSoundModulationParameterAssetProxyPtr& GetProxy() const
		{
			return Proxy;
		}

		const FSoundModulationParameterAssetProxy* operator->() const
		{
			return Proxy.Get();
		}

		FSoundModulationParameterAssetProxy* operator->()
		{
			return Proxy.Get();
		}
	};
} // namespace AudioModulation

DECLARE_METASOUND_DATA_REFERENCE_TYPES(AudioModulation::FSoundModulatorAsset, AUDIOMODULATION_API, FSoundModulatorAssetTypeInfo, FSoundModulatorAssetReadRef, FSoundModulatorAssetWriteRef)
DECLARE_METASOUND_DATA_REFERENCE_TYPES(AudioModulation::FSoundModulationParameterAsset, AUDIOMODULATION_API, FSoundModulationParameterAssetTypeInfo, FSoundModulationParameterAssetReadRef, FSoundModulationParameterAssetWriteRef)
#endif // WITH_AUDIOMODULATION_METASOUND_SUPPORT

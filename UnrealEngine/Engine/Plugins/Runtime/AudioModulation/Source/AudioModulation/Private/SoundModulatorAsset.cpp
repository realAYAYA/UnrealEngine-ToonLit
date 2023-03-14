// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_AUDIOMODULATION_METASOUND_SUPPORT
#include "SoundModulatorAsset.h"
#include "Templates/SharedPointer.h"

namespace AudioModulation
{
	const FString PluginAuthor = TEXT("Epic Games, Inc.");
	const FText PluginNodeMissingPrompt = NSLOCTEXT("AudioModulation", "DefaultMissingNodePrompt", "The node was likely removed, renamed, or the AudioModulation plugin is not loaded.");

	FSoundModulatorAsset::FSoundModulatorAsset(const Audio::IProxyDataPtr& InProxyPtr)
	{
		if (!InProxyPtr.IsValid())
		{
			return;
		}

		if (ensure(InProxyPtr->CheckTypeCast<FSoundModulatorAssetProxy>()))
		{
			FSoundModulatorAssetProxy* ModulatorProxy = static_cast<FSoundModulatorAssetProxy*>(InProxyPtr.Get());
			Proxy = MakeShared<FSoundModulatorAssetProxy, ESPMode::ThreadSafe>(*ModulatorProxy);
		}
	}

	FSoundModulationParameterAsset::FSoundModulationParameterAsset(const Audio::IProxyDataPtr& InProxyPtr)
	{
		if (!InProxyPtr.IsValid())
		{
			return;
		}

		if (ensure(InProxyPtr->CheckTypeCast<FSoundModulationParameterAssetProxy>()))
		{
			FSoundModulationParameterAssetProxy* ParamProxy = static_cast<FSoundModulationParameterAssetProxy*>(InProxyPtr.Get());
			check(ParamProxy);

			Proxy = MakeShared<FSoundModulationParameterAssetProxy, ESPMode::ThreadSafe>(*ParamProxy);
		}
	}
} // namespace AudioModulation
#endif // WITH_AUDIOMODULATION_METASOUND_SUPPORT

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "IAudioCodecRegistry.h"
#include "DecoderBackCompat.h"

class FAudioCodecEngineModule : public IModuleInterface
{
public:
	void StartupModule() override
	{
		Audio::ICodecRegistry::Get().RegisterCodec(
			MakeUnique<Audio::FBackCompatCodec>()
		);
	}
	void ShutdownModule() override
	{
		using namespace Audio;
		if (ICodecRegistry::FCodecPtr Codec = ICodecRegistry::Get().FindCodecByName(
			FBackCompatCodec::GetDetailsStatic().Name,
			FBackCompatCodec::GetDetailsStatic().Version))
		{
			ICodecRegistry::Get().UnregisterCodec(Codec);
		}
	}
};

IMPLEMENT_MODULE(FAudioCodecEngineModule, AudioCodecEngine);

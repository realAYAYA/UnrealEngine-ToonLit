// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioCaptureDeviceInterface.h"
#include "AudioCaptureWasapi.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

namespace Audio
{
	
	class FAudioCaptureWasapiFactory : public IAudioCaptureFactory
	{
	public:
		virtual TUniquePtr<IAudioCaptureStream> CreateNewAudioCaptureStream() override
		{
			return TUniquePtr<IAudioCaptureStream>(new FAudioCaptureWasapiStream());
		}
	};

	class FAudioCaptureWasapiModule : public IModuleInterface
	{
	private:
		FAudioCaptureWasapiFactory AudioCaptureFactory;

	public:
		virtual void StartupModule() override
		{
			IModularFeatures::Get().RegisterModularFeature(IAudioCaptureFactory::GetModularFeatureName(), &AudioCaptureFactory);
		}

		virtual void ShutdownModule() override
		{
			IModularFeatures::Get().UnregisterModularFeature(IAudioCaptureFactory::GetModularFeatureName(), &AudioCaptureFactory);
		}
	};
}

IMPLEMENT_MODULE(Audio::FAudioCaptureWasapiModule, AudioCaptureWasapi)

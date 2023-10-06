// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#if PLATFORM_IOS || PLATFORM_MAC || PLATFORM_TVOS

#include "IElectraPlayerInterface.h"
#include "ElectraPlayerPrivate.h"
#include "ElectraPlayer.h"
#include "ParameterDictionary.h"

#include "RHIDefinitions.h"

#include "AppleElectraDecoderResourceManager.h"

// ----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------

namespace Electra
{

/**
 * Shutdown of module
 */
bool PlatformShutdown()
{
	return true;
}

TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> PlatformCreateVideoDecoderResourceDelegate(const TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>& AdapterDelegate)
{
	return nullptr;
}

} //namespace Electra

// ----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------

/**
 * Public call for pre-use initialization after module is loaded, allowing for user passed parameters
 */
bool FElectraPlayerPlatform::StartupPlatformResources(const Electra::FParamDict& Params)
{
	FElectraDecoderResourceManagerApple::FCallbacks Callbacks;
	auto GetDeviceTypeCallback = reinterpret_cast<void(*)(void**, int64*)>(Params.GetValue(TEXT("GetDeviceTypeCallback")).SafeGetPointer());

	Callbacks.GetMetalDevice = [GetDeviceTypeCallback](void **OutMetalDevice, void*) -> bool
	{
		if (GetDeviceTypeCallback)
		{
			void* DevicePointer = nullptr;
			int64 DeviceType = 0;
			GetDeviceTypeCallback(&DevicePointer, &DeviceType);
			const ERHIInterfaceType RHIType = (ERHIInterfaceType)DeviceType;
			if (DevicePointer && RHIType == ERHIInterfaceType::Metal)
			{
				*OutMetalDevice = DevicePointer;
				return true;
			}
		}
		return false;
	};
	return FElectraDecoderResourceManagerApple::Startup(Callbacks);
}


void FElectraPlayer::PlatformNotifyOfOptionChange()
{
}

void FElectraPlayer::PlatformSuspendOrResumeDecoders(bool /*bSuspend*/, const Electra::FParamDict& /*InOptions*/)
{
}

#endif

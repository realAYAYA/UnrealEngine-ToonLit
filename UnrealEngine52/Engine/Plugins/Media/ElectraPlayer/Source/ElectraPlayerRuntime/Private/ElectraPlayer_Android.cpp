// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#if PLATFORM_ANDROID

#include "IElectraPlayerInterface.h"
#include "ElectraPlayerPrivate.h"
#include "ElectraPlayer.h"
#include "ParameterDictionary.h"
#include "VideoDecoderResourceDelegate.h"

// ----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------

namespace Electra {

/**
 * Early startup during module load process
 */
bool PlatformEarlyStartup()
{
	return true;
}

/**
 * Any platform specific memory hooks
 */
bool PlatformMemorySetup()
{
	return true;
}

/**
 * Shutdown of module
 */
bool PlatformShutdown()
{
	return true;
}

class FVideoDecoderResourceDelegate : public IVideoDecoderResourceDelegate
{
public:
	FVideoDecoderResourceDelegate(const TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>& InAdapterDelegate) : PlayerAdapterDelegate(InAdapterDelegate) {}
	virtual ~FVideoDecoderResourceDelegate() {}

	virtual jobject VideoDecoderResourceDelegate_GetCodecSurface() override
	{
		if (const TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>& PinnedAdapterDelegate = PlayerAdapterDelegate.Pin())
		{
			TSharedPtr<IElectraPlayerResourceDelegate, ESPMode::ThreadSafe> ResourceDelegate = PinnedAdapterDelegate->GetResourceDelegate();
			if (ResourceDelegate)
			{
				return ResourceDelegate->GetCodecSurface();
			}
		}
		return nullptr;
	}

private:
	TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PlayerAdapterDelegate;
};

TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> PlatformCreateVideoDecoderResourceDelegate(const TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> & AdapterDelegate)
{
	return MakeShared<FVideoDecoderResourceDelegate, ESPMode::ThreadSafe>(AdapterDelegate);
}

} //namespace Electra

// ----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------

/**
 * Public call for pre-use initialization after module is loaded, allowing for user passed parameters
 */
bool FElectraPlayerPlatform::StartupPlatformResources(const Electra::FParamDict& Params)
{
	return true;
}


void FElectraPlayer::PlatformNotifyOfOptionChange()
{
	if (TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = AdapterDelegate.Pin())
	{
		FVariantValue Value = PinnedAdapterDelegate->QueryOptions(IElectraPlayerAdapterDelegate::EOptionType::Android_VideoSurface);
		if (Value.IsValid())
		{
			if (CurrentPlayer.IsValid() && CurrentPlayer->AdaptivePlayer.IsValid())
			{
				CurrentPlayer->AdaptivePlayer->Android_UpdateSurface(Value.GetSharedPointer<IOptionPointerValueContainer>());
			}
		}
	}
}

void FElectraPlayer::PlatformSuspendOrResumeDecoders(bool bSuspend)
{
	if (CurrentPlayer.IsValid() && CurrentPlayer->AdaptivePlayer.IsValid())
	{
		CurrentPlayer->AdaptivePlayer->Android_SuspendOrResumeDecoder(bSuspend);
	}
}

#endif

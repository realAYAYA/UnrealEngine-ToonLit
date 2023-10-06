// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#include "IElectraPlayerInterface.h"
#include "ElectraPlayerPrivate.h"
#include "ElectraPlayer.h"
#include "ParameterDictionary.h"

#include "LinuxElectraDecoderResourceManager.h"

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
	return FElectraDecoderResourceManagerLinux::Startup();
}


void FElectraPlayer::PlatformNotifyOfOptionChange()
{
}

void FElectraPlayer::PlatformSuspendOrResumeDecoders(bool /*bSuspend*/, const Electra::FParamDict& /*InOptions*/)
{
}

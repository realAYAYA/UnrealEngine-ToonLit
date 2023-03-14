// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#include "IElectraPlayerInterface.h"
#include "ElectraPlayerPrivate.h"
#include "ElectraPlayer.h"
#include "ParameterDictionary.h"

// ----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------

namespace Electra
{

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
	return true;
}


void FElectraPlayer::PlatformNotifyOfOptionChange()
{
}

void FElectraPlayer::PlatformSuspendOrResumeDecoders(bool /*bSuspend*/)
{
}

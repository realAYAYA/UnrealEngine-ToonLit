// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioCaptureCoreModule.h"
#include "AudioCaptureCoreLog.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAudioCaptureCore)


void FAudioCaptureCoreModule::StartupModule()
{

}

void FAudioCaptureCoreModule::ShutdownModule()
{

}


#if defined(REQUIRE_EXPLICIT_GMALLOC_INIT) && REQUIRE_EXPLICIT_GMALLOC_INIT
extern "C" void IMPLEMENT_MODULE_AudioCaptureCore() { }
#else
IMPLEMENT_MODULE(FAudioCaptureCoreModule, AudioCaptureCore);
#endif

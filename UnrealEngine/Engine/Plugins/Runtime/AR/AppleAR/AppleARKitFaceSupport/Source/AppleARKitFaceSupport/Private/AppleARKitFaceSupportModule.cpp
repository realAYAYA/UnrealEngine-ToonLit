// Copyright Epic Games, Inc. All Rights Reserved.

#include "AppleARKitFaceSupportModule.h"
#include "AppleARKitSystem.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"
#include "CoreGlobals.h"
#include "AppleARKitFaceSupportImpl.h"
#include "Misc/ConfigCacheIni.h"


IMPLEMENT_MODULE(FAppleARKitFaceSupportModule, AppleARKitFaceSupport);

DEFINE_LOG_CATEGORY(LogAppleARKitFace);

TSharedPtr<FAppleARKitFaceSupport, ESPMode::ThreadSafe> FaceSupportInstance;

void FAppleARKitFaceSupportModule::StartupModule()
{
	ensureMsgf(FModuleManager::Get().LoadModule("AppleARKit"), TEXT("ARKitFaceSupport depends on the AppleARKit module."));

	FaceSupportInstance = MakeShared<FAppleARKitFaceSupport, ESPMode::ThreadSafe>();
	FaceSupportInstance->Init();

	// LiveLink listener needs to be created here so that the editor can receive remote publishing events
#if PLATFORM_DESKTOP
	bool bEnableLiveLinkForFaceTracking = false;
	GConfig->GetBool(TEXT("/Script/AppleARKit.AppleARKitSettings"), TEXT("bEnableLiveLinkForFaceTracking"), bEnableLiveLinkForFaceTracking, GEngineIni);
	if (bEnableLiveLinkForFaceTracking)
	{
		FAppleARKitLiveLinkSourceFactory::CreateLiveLinkRemoteListener();
	}
#endif
}

void FAppleARKitFaceSupportModule::ShutdownModule()
{
	FaceSupportInstance->Shutdown();
	FaceSupportInstance = nullptr;
}


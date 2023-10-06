// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingVCamModule.h"

#include "BuiltinProviders/VCamPixelStreamingSession.h"
#include "IDecoupledOutputProviderModule.h"
#include "VCamPixelStreamingSessionLogic.h"

#include "Modules/ModuleManager.h"

namespace UE::PixelStreamingVCam::Private
{
	void FPixelStreamingVCamModule::StartupModule()
	{
		using namespace DecoupledOutputProvider;
		IDecoupledOutputProviderModule& DecouplingModule = IDecoupledOutputProviderModule::Get();
		DecouplingModule.RegisterLogicFactory(
			UVCamPixelStreamingSession::StaticClass(),
			FOutputProviderLogicFactoryDelegate::CreateLambda([](const FOutputProviderLogicCreationArgs& Args)
			{
				return MakeShared<FVCamPixelStreamingSessionLogic>();
			})
		);
	}

	void FPixelStreamingVCamModule::ShutdownModule()
	{
		// DecoupledOutputProvider will also be destroyed in a moment (part of same plugin) so we do not have to call IDecoupledOutputProviderModule::UnregisterLogicFactory.
		// In fact doing so would not even work because UVCamPixelStreamingSession::StaticClass() would return garbage.
	}
}

IMPLEMENT_MODULE(UE::PixelStreamingVCam::Private::FPixelStreamingVCamModule, PixelStreamingVCam);

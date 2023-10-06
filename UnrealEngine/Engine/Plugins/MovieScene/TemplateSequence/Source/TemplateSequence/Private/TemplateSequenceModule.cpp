// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "CameraAnimationSequencePlayer.h"

class FTemplateSequenceModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		UCameraAnimationSequenceCameraStandIn::RegisterCameraStandIn();
	}

	virtual void ShutdownModule() override
	{
		UCameraAnimationSequenceCameraStandIn::UnregisterCameraStandIn();
	}
};

IMPLEMENT_MODULE(FTemplateSequenceModule, TemplateSequence);

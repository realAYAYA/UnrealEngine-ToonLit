// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "TextComponentTypes.h"

class FMovieSceneTextTrackModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FMovieSceneTextTrackModule, MovieSceneTextTrack)

void FMovieSceneTextTrackModule::StartupModule()
{
	UE::MovieScene::FTextComponentTypes::Get();
}

void FMovieSceneTextTrackModule::ShutdownModule()
{
	UE::MovieScene::FTextComponentTypes::Destroy();
}

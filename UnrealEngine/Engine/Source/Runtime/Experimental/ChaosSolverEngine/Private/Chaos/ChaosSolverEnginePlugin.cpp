// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/ChaosSolverEnginePlugin.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Chaos/ChaosDebugDrawComponent.h"
#include "Chaos/ChaosSolverActor.h"
#include "ChaosSolversModule.h"
#include "ChaosVDRecordingStateScreenMessageHandler.h"

class FChaosSolverEnginePlugin : public IChaosSolverEnginePlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FChaosSolverEnginePlugin, ChaosSolverEngine )

void FChaosSolverEnginePlugin::StartupModule()
{
	FChaosSolversModule* const ChaosModule = FChaosSolversModule::GetModule();
	check(ChaosModule);
	ChaosModule->SetSolverActorClass(AChaosSolverActor::StaticClass(), AChaosSolverActor::StaticClass());

	UChaosDebugDrawComponent::BindWorldDelegates();

#if WITH_CHAOS_VISUAL_DEBUGGER
	FChaosVDRecordingStateScreenMessageHandler::Get().Initialize();
#endif

}

void FChaosSolverEnginePlugin::ShutdownModule()
{

#if WITH_CHAOS_VISUAL_DEBUGGER
	FChaosVDRecordingStateScreenMessageHandler::Get().TearDown();
#endif

}




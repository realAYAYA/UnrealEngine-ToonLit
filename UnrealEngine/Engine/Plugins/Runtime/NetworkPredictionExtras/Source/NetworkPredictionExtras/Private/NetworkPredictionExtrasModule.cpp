// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPredictionExtrasModule.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "Internationalization/Internationalization.h"
#include "MockRootMotionSourceObject.h"

#if WITH_EDITOR
#include "Editor.h"
#endif


#define LOCTEXT_NAMESPACE "FNetworkPredictionModule"

class FNetworkPredictionExtrasModule : public INetworkPredictionExtrasModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FDelegateHandle PieHandle;
	FDelegateHandle PostEngineInitHandle;
};

void FNetworkPredictionExtrasModule::StartupModule()
{
	PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda([]()
	{
		UMockRootMotionSourceClassMap::InitSingleton();
	});

#if WITH_EDITOR
	PieHandle = FEditorDelegates::PreBeginPIE.AddLambda([this](const bool bBegan)
	{
		UMockRootMotionSourceClassMap::BuildClassMap();
	});

#endif
}


void FNetworkPredictionExtrasModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
	PostEngineInitHandle.Reset();

#if WITH_EDITOR
	FEditorDelegates::PreBeginPIE.Remove(PieHandle);
	PieHandle.Reset();
#endif
}

IMPLEMENT_MODULE( FNetworkPredictionExtrasModule, NetworkPredictionExtras )
#undef LOCTEXT_NAMESPACE


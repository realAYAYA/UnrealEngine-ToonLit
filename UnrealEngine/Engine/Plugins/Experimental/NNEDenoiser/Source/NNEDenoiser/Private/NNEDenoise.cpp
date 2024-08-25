// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDenoiser.h"
#include "Misc/CoreDelegates.h"
#include "NNEDenoiserLog.h"
#include "NNEDenoiserViewExtension.h"

DEFINE_LOG_CATEGORY(LogNNEDenoiser);

void FNNEDenoiserModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogNNEDenoiser, Log, TEXT("NNEDenoiser module starting up"));

	FCoreDelegates::OnPostEngineInit.AddLambda([this] ()
	{
		ViewExtension = FSceneViewExtensions::NewExtension<UE::NNEDenoiser::Private::FViewExtension>();
	});
}

void FNNEDenoiserModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	UE_LOG(LogNNEDenoiser, Log, TEXT("NNEDenoiser module shut down"));

	ViewExtension.Reset();
}

IMPLEMENT_MODULE(FNNEDenoiserModule, NNEDenoiser)
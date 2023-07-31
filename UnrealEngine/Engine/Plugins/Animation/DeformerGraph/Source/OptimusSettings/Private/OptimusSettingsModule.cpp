// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusSettingsModule.h"

#include "Animation/MeshDeformer.h"
#include "Features/IModularFeatures.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "OptimusSettings.h"


UOptimusSettings::UOptimusSettings(const FObjectInitializer& ObjectInitlaizer)
	: UDeveloperSettings(ObjectInitlaizer)
{
}

#if WITH_EDITOR

UOptimusSettings::FOnUpdateSettings UOptimusSettings::OnSettingsChange;

void UOptimusSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnSettingsChange.Broadcast(this);
}

#endif // WITH_EDITOR


void FOptimusSettingsModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(IMeshDeformerProvider::ModularFeatureName, this);

#if WITH_EDITOR
	UOptimusSettings::OnSettingsChange.AddRaw(this, &FOptimusSettingsModule::CacheDefaultMeshDeformers);
#endif

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FOptimusSettingsModule::CacheDefaultMeshDeformers);
}

void FOptimusSettingsModule::ShutdownModule()
{
#if WITH_EDITOR
	UOptimusSettings::OnSettingsChange.RemoveAll(this);
#endif

	IModularFeatures::Get().UnregisterModularFeature(IMeshDeformerProvider::ModularFeatureName, this);
}

void FOptimusSettingsModule::CacheDefaultMeshDeformers()
{
	CacheDefaultMeshDeformers(GetDefault<UOptimusSettings>());
}

void FOptimusSettingsModule::CacheDefaultMeshDeformers(UOptimusSettings const* InSettings)
{
	const bool bLoadDeformers = InSettings != nullptr && InSettings->DefaultMode != EOptimusDefaultDeformerMode::Never;
	DefaultDeformer = bLoadDeformers && !InSettings->DefaultDeformer.IsNull() ? InSettings->DefaultDeformer.LoadSynchronous() : nullptr;
	DefaultRecomputeTangentDeformer = bLoadDeformers && !InSettings->DefaultRecomputeTangentDeformer.IsNull() ? InSettings->DefaultRecomputeTangentDeformer.LoadSynchronous() : nullptr;
}

TObjectPtr<UMeshDeformer> FOptimusSettingsModule::GetDefaultMeshDeformer(FDefaultMeshDeformerSetup const& Setup)
{
	const UOptimusSettings* Settings = GetDefault<UOptimusSettings>();
	if (Settings != nullptr && 
		(Settings->DefaultMode == EOptimusDefaultDeformerMode::Always ||
		(Settings->DefaultMode == EOptimusDefaultDeformerMode::SkinCacheOnly && Setup.bIsUsingSkinCache)))
	{
		if (Setup.bIsRequestingRecomputeTangent && DefaultRecomputeTangentDeformer)
		{
			return DefaultRecomputeTangentDeformer;
		}
		if (DefaultDeformer)
		{ 
			return DefaultDeformer;
		}
	}
	return nullptr;
}

IMPLEMENT_MODULE(FOptimusSettingsModule, OptimusSettings)

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshReductionManagerModule.h"
#include "IMeshReductionInterfaces.h"
#include "HAL/IConsoleManager.h"
#include "CoreGlobals.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY_STATIC(LogMeshReduction, Verbose, All);

static FAutoConsoleVariable CVarMeshReductionModule(
	TEXT("r.MeshReductionModule"),
	TEXT("QuadricMeshReduction"),
	TEXT("Name of what mesh reduction module to choose. If blank it chooses any that exist.\n"),
	ECVF_ReadOnly);

static FAutoConsoleVariable CVarSkeletalMeshReductionModule(
	TEXT("r.SkeletalMeshReductionModule"),
	TEXT("SkeletalMeshReduction"),
	TEXT("Name of what skeletal mesh reduction module to choose. If blank it chooses any that exist.\n"),
	ECVF_ReadOnly);

static FAutoConsoleVariable CVarProxyLODMeshReductionModule(
	TEXT("r.ProxyLODMeshReductionModule"),
	TEXT("QuadricMeshProxyLODReduction"),
	TEXT("Name of the Proxy LOD reduction module to choose. If blank it chooses any that exist.\n"),
	ECVF_ReadOnly);


IMPLEMENT_MODULE(FMeshReductionManagerModule, MeshReductionInterface);

FMeshReductionManagerModule::FMeshReductionManagerModule()
	: StaticMeshReduction(nullptr)
	, SkeletalMeshReduction(nullptr)
	, MeshMerging(nullptr)
	, DistributedMeshMerging(nullptr)
{
}

void FMeshReductionManagerModule::StartupModule()
{
	checkf(StaticMeshReduction   == nullptr, TEXT("Static Reduction instance should be null during startup"));
	checkf(SkeletalMeshReduction == nullptr, TEXT("Skeletal Reduction instance should be null during startup"));
	checkf(MeshMerging           == nullptr, TEXT("Mesh Merging instance should be null during startup"));

	// This module could be launched very early by static meshes loading before the settings class that stores this value has had a chance to load.  Have to read from the config file early in the startup process
	FString MeshReductionModuleName;
	GConfig->GetString(TEXT("/Script/Engine.MeshSimplificationSettings"), TEXT("r.MeshReductionModule"), MeshReductionModuleName, GEngineIni);
	CVarMeshReductionModule->Set(*MeshReductionModuleName);

	FString SkeletalMeshReductionModuleName;
	GConfig->GetString(TEXT("/Script/Engine.SkeletalMeshSimplificationSettings"), TEXT("r.SkeletalMeshReductionModule"), SkeletalMeshReductionModuleName, GEngineIni);
	// If nothing was specified, default to simplygon
	if (SkeletalMeshReductionModuleName.IsEmpty())
	{
		SkeletalMeshReductionModuleName = FString("SimplygonMeshReduction");
	}
	CVarSkeletalMeshReductionModule->Set(*SkeletalMeshReductionModuleName);

	FString HLODMeshReductionModuleName;
	GConfig->GetString(TEXT("/Script/Engine.ProxyLODMeshSimplificationSettings"), TEXT("r.ProxyLODMeshReductionModule"), HLODMeshReductionModuleName, GEngineIni);
	// If nothing was requested, default to simplygon for mesh merging reduction
	if (HLODMeshReductionModuleName.IsEmpty())
	{
		HLODMeshReductionModuleName = FString("SimplygonMeshReduction");
	}
	CVarProxyLODMeshReductionModule->Set(*HLODMeshReductionModuleName);

	// Retrieve reduction interfaces 
	TArray<FName> ModuleNames;
	FModuleManager::Get().FindModules(TEXT("*MeshReduction"), ModuleNames);
	for (FName ModuleName : ModuleNames)
	{
		FModuleManager::Get().LoadModule(ModuleName);
	}

	if (FModuleManager::Get().ModuleExists(TEXT("SimplygonSwarm")))
	{
		FModuleManager::Get().LoadModule("SimplygonSwarm");
	}
	
	TArray<IMeshReductionModule*> MeshReductionModules = IModularFeatures::Get().GetModularFeatureImplementations<IMeshReductionModule>(IMeshReductionModule::GetModularFeatureName());
	
	const FString RequestedMeshReductionModuleName         = CVarMeshReductionModule->GetString();
	const FString RequestedSkeletalMeshReductionModuleName = CVarSkeletalMeshReductionModule->GetString();
	const FString RequestedProxyLODReductionModuleName     = CVarProxyLODMeshReductionModule->GetString();

	// actual module names that will be used.
	FString StaticMeshModuleName;
	FString SkeletalMeshModuleName;
	FString MeshMergingModuleName;
	FString DistributedMeshMergingModuleName;

	for (IMeshReductionModule* Module : MeshReductionModules)
	{
		// Is this a requested module?
		const FString ModuleName = Module->GetName();
		const bool bIsRequestedMeshReductionModule         = ModuleName.Equals(RequestedMeshReductionModuleName);
		const bool bIsRequestedSkeletalMeshReductionModule = ModuleName.Equals(RequestedSkeletalMeshReductionModuleName);
		const bool bIsRequestedProxyLODReductionModule     = ModuleName.Equals(RequestedProxyLODReductionModuleName);
	

		// Look for MeshReduction interface
		IMeshReduction* StaticMeshReductionInterface = Module->GetStaticMeshReductionInterface();
		if (StaticMeshReductionInterface)
		{
			if ( bIsRequestedMeshReductionModule || StaticMeshReduction == nullptr )
			{
				StaticMeshReduction  = StaticMeshReductionInterface;
				StaticMeshModuleName = ModuleName;
			}
		}

		// Look for Skeletal MeshReduction interface
		IMeshReduction* SkeletalMeshReductionInterface = Module->GetSkeletalMeshReductionInterface();
		if (SkeletalMeshReductionInterface)
		{
			if ( bIsRequestedSkeletalMeshReductionModule || SkeletalMeshReduction == nullptr )
			{
				SkeletalMeshReduction  = SkeletalMeshReductionInterface;
				SkeletalMeshModuleName = ModuleName;
			}
		}

		// Look for MeshMerging interface
		IMeshMerging* MeshMergingInterface = Module->GetMeshMergingInterface();
		if (MeshMergingInterface)
		{
			if ( bIsRequestedProxyLODReductionModule || MeshMerging == nullptr )
			{
				MeshMerging           = MeshMergingInterface;
				MeshMergingModuleName = ModuleName;
			}
		}

		// Look for Distributed MeshMerging interface
		IMeshMerging* DistributedMeshMergingInterface = Module->GetDistributedMeshMergingInterface();
		if (DistributedMeshMergingInterface)
		{
			if ( bIsRequestedMeshReductionModule || DistributedMeshMerging == nullptr )
			{
				DistributedMeshMerging           = DistributedMeshMergingInterface;
				DistributedMeshMergingModuleName = ModuleName;
			}
		}
	}

	// Set the names that will appear as defaults in the project settings

	CVarMeshReductionModule->Set(*StaticMeshModuleName);
	CVarSkeletalMeshReductionModule->Set(*SkeletalMeshModuleName);
	CVarProxyLODMeshReductionModule->Set(*MeshMergingModuleName);

	if (!StaticMeshReduction)
	{
		UE_LOG(LogMeshReduction, Log, TEXT("No automatic static mesh reduction module available"));
	}
	else
	{
		UE_LOG(LogMeshReduction, Log, TEXT("Using %s for automatic static mesh reduction"), *StaticMeshModuleName);
	}

	if (!SkeletalMeshReduction)
	{
		UE_LOG(LogMeshReduction, Log, TEXT("No automatic skeletal mesh reduction module available"));
	}
	else
	{
		UE_LOG(LogMeshReduction, Log, TEXT("Using %s for automatic skeletal mesh reduction"), *SkeletalMeshReductionModuleName);
	}

	if (!MeshMerging)
	{
		UE_LOG(LogMeshReduction, Log, TEXT("No automatic mesh merging module available"));
	}
	else
	{
		UE_LOG(LogMeshReduction, Log, TEXT("Using %s for automatic mesh merging"), *MeshMergingModuleName);
	}


	if (!DistributedMeshMerging)
	{
		UE_LOG(LogMeshReduction, Log, TEXT("No distributed automatic mesh merging module available"));
	}
	else
	{
		UE_LOG(LogMeshReduction, Log, TEXT("Using %s for distributed automatic mesh merging"), *DistributedMeshMergingModuleName);
	}
}

void FMeshReductionManagerModule::ShutdownModule()
{
	StaticMeshReduction = SkeletalMeshReduction = nullptr;
	MeshMerging = DistributedMeshMerging = nullptr;
}

IMeshReduction* FMeshReductionManagerModule::GetStaticMeshReductionInterface() const
{
	return StaticMeshReduction;
}

IMeshReduction* FMeshReductionManagerModule::GetSkeletalMeshReductionInterface() const
{
	return SkeletalMeshReduction;
}

IMeshMerging* FMeshReductionManagerModule::GetMeshMergingInterface() const
{
	return MeshMerging;
}

IMeshMerging* FMeshReductionManagerModule::GetDistributedMeshMergingInterface() const
{
	return DistributedMeshMerging;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/CompileShadersTestBedCommandlet.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CollectionManagerModule.h"
#include "CollectionManagerTypes.h"
#include "GlobalShader.h"
#include "ICollectionManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ShaderCompiler.h"

DEFINE_LOG_CATEGORY_STATIC(LogCompileShadersTestBedCommandlet, Log, All);

UCompileShadersTestBedCommandlet::UCompileShadersTestBedCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UCompileShadersTestBedCommandlet::Main(const FString& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UCompileShadersTestBedCommandlet::Main);

	StaticExec(nullptr, TEXT("log LogMaterial Verbose"));

	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	// Display help
	if (Switches.Contains("help"))
	{
		UE_LOG(LogCompileShadersTestBedCommandlet, Log, TEXT("CompileShadersTestBed"));
		UE_LOG(LogCompileShadersTestBedCommandlet, Log, TEXT("This commandlet compiles global and default material shaders.  Used to profile and test shader compilation."));
		UE_LOG(LogCompileShadersTestBedCommandlet, Log, TEXT(" Optional: -collection=<name>                (You can also specify a collection of assets to narrow down the results e.g. if you maintain a collection that represents the actually used in-game assets)."));
		UE_LOG(LogCompileShadersTestBedCommandlet, Log, TEXT(" Optional: -materials=<path1>+<path2>        (You can also specify a list of material asset paths separated by a '+' to narrow down the results."));
		return 0;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.SearchAllAssets(true);

	// Optional list of materials to compile.
	TArray<FAssetData> MaterialList;

	FARFilter Filter;

	// Parse collection
	FString CollectionName;
	if (FParse::Value(*Params, TEXT("collection="), CollectionName, true))
	{
		if (!CollectionName.IsEmpty())
		{
			// Get the list of materials from a collection
			Filter.PackagePaths.Add(FName(TEXT("/Game")));
			Filter.bRecursivePaths = true;
			Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());

			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
			CollectionManagerModule.Get().GetObjectsInCollection(FName(*CollectionName), ECollectionShareType::CST_All, Filter.SoftObjectPaths, ECollectionRecursionFlags::SelfAndChildren);

			AssetRegistry.GetAssets(Filter, MaterialList);

			Filter.ClassPaths.Empty();
			Filter.ClassPaths.Add(UMaterialInstance::StaticClass()->GetClassPathName());
			Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());

			AssetRegistry.GetAssets(Filter, MaterialList);
		}
	}

	UE_LOG(LogCompileShadersTestBedCommandlet, Log, TEXT("Found %d/%d Materials."), MaterialList.Num(), Filter.SoftObjectPaths.Num());

	// Process -materials= switches separated by a '+'
	TArray<FString> CmdLineMaterialEntries;
	const TCHAR* MaterialsSwitchName = TEXT("Materials");
	if (const FString* MaterialsSwitches = ParamVals.Find(MaterialsSwitchName))
	{
		MaterialsSwitches->ParseIntoArray(CmdLineMaterialEntries, TEXT("+"));
	}

	if (CmdLineMaterialEntries.Num())
	{
		// re-use the filter and only filter based on the passed in objects.
		Filter.ClassPaths.Empty();
		Filter.SoftObjectPaths.Empty();
		for (const FString& MaterialPathString : CmdLineMaterialEntries)
		{
			const FSoftObjectPath MaterialPath(MaterialPathString);
			if (!Filter.SoftObjectPaths.Contains(MaterialPath))
			{
				Filter.SoftObjectPaths.Add(MaterialPath);
			}
		}

		AssetRegistry.GetAssets(Filter, MaterialList);
	}

	static constexpr bool bLimitExecutationTime = false;

	// For all active platforms
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

	for (ITargetPlatform* Platform : Platforms)
	{
		// Compile default materials
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(DefaultMaterials);

			UE_LOG(LogCompileShadersTestBedCommandlet, Log, TEXT("Compile default materials"));

			for (int32 Domain = 0; Domain < MD_MAX; ++Domain)
			{
				UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(static_cast<EMaterialDomain>(Domain));

				DefaultMaterial->BeginCacheForCookedPlatformData(Platform);
				while (!DefaultMaterial->IsCachedCookedPlatformDataLoaded(Platform))
				{
					GShaderCompilingManager->ProcessAsyncResults(bLimitExecutationTime, false /* bBlockOnGlobalShaderCompilation */);
				}
				DefaultMaterial->ClearCachedCookedPlatformData(Platform);
			}
		}

		// Compile global shaders
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(GlobalShaders);

			TArray<FName> DesiredShaderFormats;
			Platform->GetAllTargetedShaderFormats(DesiredShaderFormats);

			UE_LOG(LogCompileShadersTestBedCommandlet, Log, TEXT("Compile global shaders"));

			for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
			{
				const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);
				CompileGlobalShaderMap(ShaderPlatform, Platform, false);
			}

			GShaderCompilingManager->ProcessAsyncResults(bLimitExecutationTime, true /* bBlockOnGlobalShaderCompilation */);
		}

		TSet<UMaterialInterface*> MaterialsToCompile;

		// Begin Material Compiles
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(BeginCacheForCookedPlatformData);

			// Sort the material lists by name so the order is stable.
			Algo::SortBy(MaterialList, [](const FAssetData& AssetData) { return AssetData.GetSoftObjectPath(); }, [](const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.LexicalLess(B); });

			UE_LOG(LogCompileShadersTestBedCommandlet, Log, TEXT("Begin Cache For Cooked PlatformData"));

			for (const FAssetData& AssetData : MaterialList)
			{
				if (UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(AssetData.GetAsset()))
				{
					UE_LOG(LogCompileShadersTestBedCommandlet, Display, TEXT("BeginCache for %s"), *MaterialInterface->GetFullName());
					MaterialInterface->BeginCacheForCookedPlatformData(Platform);
					MaterialsToCompile.Add(MaterialInterface);
				}
			}
		}

		int32 PreviousOutstandingJobs = 0;

		// Submit all the jobs.
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SubmitJobs);

			UE_LOG(LogCompileShadersTestBedCommandlet, Log, TEXT("Submit Jobs"));

			while (MaterialsToCompile.Num())
			{
				for (auto It = MaterialsToCompile.CreateIterator(); It; ++It)
				{
					UMaterialInterface* MaterialInterface = *It;
					if (MaterialInterface->IsCachedCookedPlatformDataLoaded(Platform))
					{
						It.RemoveCurrent();
						UE_LOG(LogCompileShadersTestBedCommandlet, Display, TEXT("Finished cache for %s."), *MaterialInterface->GetFullName());
						UE_LOG(LogCompileShadersTestBedCommandlet, Display, TEXT("Materials remaining: %d"), MaterialsToCompile.Num());
					}

					GShaderCompilingManager->ProcessAsyncResults(bLimitExecutationTime, false /* bBlockOnGlobalShaderCompilation */);

					const int32 CurrentOutstandingJobs = GShaderCompilingManager->GetNumOutstandingJobs();
					if (CurrentOutstandingJobs != PreviousOutstandingJobs)
					{
						UE_LOG(LogCompileShadersTestBedCommandlet, Log, TEXT("Outstanding Jobs: %d"), CurrentOutstandingJobs);
						PreviousOutstandingJobs = CurrentOutstandingJobs;
					}

					// Flush rendering commands to release any RHI resources (shaders and shader maps).
					// Delete any FPendingCleanupObjects (shader maps).
					FlushRenderingCommands();
				}
			}
		}

		// Process the shader maps and save to the DDC.
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ProcessShaderCompileResults);

			UE_LOG(LogCompileShadersTestBedCommandlet, Log, TEXT("ProcessAsyncResults"));

			while (GShaderCompilingManager->IsCompiling())
			{
				GShaderCompilingManager->ProcessAsyncResults(bLimitExecutationTime, false /* bBlockOnGlobalShaderCompilation */);

				const int32 CurrentOutstandingJobs = GShaderCompilingManager->GetNumOutstandingJobs();
				if (CurrentOutstandingJobs != PreviousOutstandingJobs)
				{
					UE_LOG(LogCompileShadersTestBedCommandlet, Display, TEXT("Outstanding Jobs: %d"), CurrentOutstandingJobs);
					PreviousOutstandingJobs = CurrentOutstandingJobs;
				}
				
				// Flush rendering commands to release any RHI resources (shaders and shader maps).
				// Delete any FPendingCleanupObjects (shader maps).
				FlushRenderingCommands();
			}
		}

		// Perform cleanup and clear cached data for cooking.
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ClearCachedCookedPlatformData);

			UE_LOG(LogCompileShadersTestBedCommandlet, Display, TEXT("Clear Cached Cooked Platform Data"));

			for (const FAssetData& AssetData : MaterialList)
			{
				if (UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(AssetData.GetAsset()))
				{
					MaterialInterface->ClearAllCachedCookedPlatformData();
				}
			}
		}
	}

	return 0;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookedEditorPackageManager.h"
#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "GameDelegates.h"

DEFINE_LOG_CATEGORY_STATIC(LogCookedEditorTargetPlatform, Log, All)




TUniquePtr<ICookedEditorPackageManager> ICookedEditorPackageManager::FactoryForTargetPlatform(ITargetPlatform* TP, bool bIsCookedCooker)
{
	if (FGameDelegates::Get().GetCookedEditorPackageManagerFactoryDelegate().IsBound())
	{
		return FGameDelegates::Get().GetCookedEditorPackageManagerFactoryDelegate().Execute();
	}
	return TUniquePtr<ICookedEditorPackageManager>(new FIniCookedEditorPackageManager(bIsCookedCooker));
}



void ICookedEditorPackageManager::AddPackagesFromPath(TArray<FName>& Packages, const TCHAR* Path, EPackageSearchMode SearchMode) const
{
	UAssetManager& AssetManager = UAssetManager::Get();
	IAssetRegistry& AssetRegistry = AssetManager.GetAssetRegistry();

	TArray<FAssetData> AssetDatas;
	// look up the path in the asset registry, so we can use it to make sure the asset can be cooked
	AssetRegistry.GetAssetsByPath(Path, AssetDatas, SearchMode == EPackageSearchMode::Recurse, true);
	for (const FAssetData& AssetData : AssetDatas)
	{
		if (AssetData.IsUAsset() && AssetManager.VerifyCanCookPackage(nullptr, AssetData.PackageName, false))
		{
			if (AllowAssetToBeGathered(AssetData))
			{
				Packages.Add(AssetData.PackageName);
				UE_LOG(LogCookedEditorTargetPlatform, Verbose, TEXT("  Adding asset %s to be cooked"), *AssetData.PackageName.ToString());
			}
		}
		else
		{
			UE_LOG(LogCookedEditorTargetPlatform, Verbose, TEXT("  skipping asset package %s"), *AssetData.PackageName.ToString());
		}
	}
}

void ICookedEditorPackageManager::GatherAllPackagesExceptDisabled(TArray<FName>& PackageNames, const ITargetPlatform* TargetPlatform, const TArray<FString>& DisabledPlugins) const
{
	GetEnginePackagesToCook(PackageNames);
	GetProjectPackagesToCook(PackageNames);

	// copy array to set for faster contains calls
	TSet<FString> CookedEditorDisabledPlugins;
	CookedEditorDisabledPlugins.Append(DisabledPlugins);

	// walk over plugins and cook their content
	for (TSharedRef<IPlugin> Plugin : IPluginManager::Get().GetEnabledPluginsWithContent())
	{
		if (!CookedEditorDisabledPlugins.Contains(Plugin->GetName()))
		{
			// check if this engine or project plugin shuold be cooked
			bool bShouldCook = (Plugin->GetLoadedFrom() == EPluginLoadedFrom::Engine) ? AllowEnginePluginContentToBeCooked(Plugin) :
				AllowProjectPluginContentToBeCooked(Plugin);

			if (bShouldCook)
			{
				UE_LOG(LogCookedEditorTargetPlatform, Display, TEXT("Adding enabled plugin with content: %s"), *Plugin->GetName());
				AddPackagesFromPath(PackageNames, *Plugin->GetMountedAssetPath(), EPackageSearchMode::Recurse);
				
			}
		}
	}

	FilterGatheredPackages(PackageNames);
}

void ICookedEditorPackageManager::FilterGatheredPackages(TArray<FName>& PackageNames) const
{

}



FIniCookedEditorPackageManager::FIniCookedEditorPackageManager(bool bInIsCookedCooker)
	: bIsCookedCooker(bInIsCookedCooker)
{
	EngineAssetPaths = GetConfigArray(TEXT("EngineAssetPaths"));
	ProjectAssetPaths = GetConfigArray(TEXT("ProjectAssetPaths"));
	DisallowedPathsToGather = GetConfigArray(TEXT("DisallowedPathsToGather"));
	DisabledPlugins = GetConfigArray(TEXT("DisabledPlugins"));

	TArray<FString> DisallowedObjectClassNamesToLoad = GetConfigArray(TEXT("DisallowedObjectClassesToLoad"));;
	for (const FString& ClassName : DisallowedObjectClassNamesToLoad)
	{
		check(FPackageName::IsValidObjectPath(ClassName));
		UClass* Class = FindObject<UClass>(nullptr, *ClassName);
		check(Class);

		DisallowedObjectClassesToLoad.Add(Class);
	}

	TArray<FString> DisallowedAssetClassNamesToGather = GetConfigArray(TEXT("DisallowedAssetClassesToGather"));;
	for (const FString& ClassName : DisallowedAssetClassNamesToGather)
	{
		check(FPackageName::IsValidObjectPath(ClassName));
		UClass* Class = FindObject<UClass>(nullptr, *ClassName);
		check(Class);

		DisallowedAssetClassesToGather.Add(Class);
	}
}

TArray<FString> FIniCookedEditorPackageManager::GetConfigArray(const TCHAR* Key) const
{
	const TCHAR* SharedIniSection = TEXT("CookedEditorSettings");
	const TCHAR* SpecificiIniSection = bIsCookedCooker ? TEXT("CookedEditorSettings_CookedCooker") : TEXT("CookedEditorSettings_CookedEditor");

	TArray<FString> TempArray;
	TArray<FString> ResultArray;

	GConfig->GetArray(SpecificiIniSection, Key, ResultArray, GGameIni);
	GConfig->GetArray(SharedIniSection, Key, TempArray, GGameIni);
	ResultArray.Append(TempArray);

	return ResultArray;
}

void FIniCookedEditorPackageManager::GatherAllPackages(TArray<FName>& PackageNames, const ITargetPlatform* TargetPlatform) const
{
	GatherAllPackagesExceptDisabled(PackageNames, TargetPlatform, DisabledPlugins);
}

void FIniCookedEditorPackageManager::FilterGatheredPackages(TArray<FName>& PackageNames) const
{
	// now filter based on ini settings
	PackageNames.RemoveAll([this](FName& AssetPath)
		{
			FNameBuilder AssetPathBuilder(AssetPath);
			const FStringView AssetPathView(AssetPathBuilder);
			for (const FString& Path : DisallowedPathsToGather)
			{
				if (AssetPathView.StartsWith(Path))
				{
					return true;
				}
			}
			return false;
		});
	PackageNames.Remove(NAME_None);
}

void FIniCookedEditorPackageManager::GetEnginePackagesToCook(TArray<FName>& PackagesToCook) const
{
	for (const FString& Path : EngineAssetPaths)
	{
		AddPackagesFromPath(PackagesToCook, *Path, EPackageSearchMode::Recurse);
	}

	// specific assets to cook
	PackagesToCook.Append(GetConfigArray(TEXT("EngineSpecificAssetsToCook")));
}

void FIniCookedEditorPackageManager::GetProjectPackagesToCook(TArray<FName>& PackagesToCook) const
{
	for (const FString& Path : ProjectAssetPaths)
	{
		AddPackagesFromPath(PackagesToCook, *Path, EPackageSearchMode::Recurse);
	}

	// make sure editor startup map is cooked
	FString EditorStartupMap;
	if (GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("EditorStartupMap"), EditorStartupMap, GEngineIni))
	{
		PackagesToCook.Add(*EditorStartupMap);
	}

	// specific assets to cook
	PackagesToCook.Append(GetConfigArray(TEXT("ProjectSpecificAssetsToCook")));
}

bool FIniCookedEditorPackageManager::AllowObjectToBeCooked(const class UObject* Obj) const
{
	for (UClass* Class : DisallowedObjectClassesToLoad)
	{
		if (Obj->IsA(Class))
		{
			return false;
		}
	}
	return true;
}

bool FIniCookedEditorPackageManager::AllowAssetToBeGathered(const struct FAssetData& AssetData) const
{
	for (UClass* Class : DisallowedAssetClassesToGather)
	{
		if (AssetData.IsInstanceOf(Class))
		{
			return false;
		}
	}

	return true;
}

bool FIniCookedEditorPackageManager::AllowEnginePluginContentToBeCooked(const TSharedRef<IPlugin>) const
{
	return true;
}

bool FIniCookedEditorPackageManager::AllowProjectPluginContentToBeCooked(const TSharedRef<IPlugin>) const
{
	return true;
}




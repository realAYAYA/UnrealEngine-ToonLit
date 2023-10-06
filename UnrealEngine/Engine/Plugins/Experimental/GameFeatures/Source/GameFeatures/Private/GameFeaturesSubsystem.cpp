// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeaturesSubsystem.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GameFeaturesSubsystemSettings.h"
#include "GameFeaturesProjectPolicies.h"
#include "GameFeatureData.h"
#include "GameFeaturePluginStateMachine.h"
#include "GameFeatureStateChangeObserver.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Stats/StatsMisc.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManagerSettings.h"
#include "InstallBundleTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeaturesSubsystem)

DEFINE_LOG_CATEGORY(LogGameFeatures);

const uint32 FInstallBundlePluginProtocolMetaData::FDefaultValues::CurrentVersionNum = 1;
//Missing InstallBundles on purpose as the default is just an empty TArray and should always be encoded
const bool FInstallBundlePluginProtocolMetaData::FDefaultValues::Default_bUninstallBeforeTerminate = false;
const bool FInstallBundlePluginProtocolMetaData::FDefaultValues::Default_bUserPauseDownload = false;
const bool FInstallBundlePluginProtocolMetaData::FDefaultValues::Default_bAllowIniLoading = false;
const bool FInstallBundlePluginProtocolMetaData::FDefaultValues::Default_bDoNotDownload = false;
const EInstallBundleRequestFlags FInstallBundlePluginProtocolMetaData::FDefaultValues::Default_InstallBundleFlags = EInstallBundleRequestFlags::Defaults;
const EInstallBundleReleaseRequestFlags FInstallBundlePluginProtocolMetaData::FDefaultValues::Default_ReleaseInstallBundleFlags = EInstallBundleReleaseRequestFlags::None;

namespace UE::GameFeatures
{
	static const FString SubsystemErrorNamespace(TEXT("GameFeaturePlugin.Subsystem."));

	namespace PluginURLStructureInfo
	{
		const TCHAR* OptionAssignOperator = TEXT("=");
		const TCHAR* OptionSeperator = TEXT("?");
		const TCHAR* OptionListSeperator = TEXT(",");
	}

	namespace CommonErrorCodes
	{
		const TCHAR* PluginNotAllowed = TEXT("Plugin_Denied_By_GameSpecificPolicy");
		const TCHAR* PluginFiltered = TEXT("Plugin_Filtered_By_GameSpecificPolicy");
		const TCHAR* PluginDetailsNotFound = TEXT("Plugin_Details_Not_Found");
		const TCHAR* DependencyFailedRegister = TEXT("Failed_Dependency_Register");
		const TCHAR* BadURL = TEXT("Bad_URL");
		const TCHAR* UnreachableState = TEXT("State_Currently_Unreachable");

		const TCHAR* CancelAddonCode = TEXT("_Cancel");
	}

	static bool GCachePluginDetails = true;
	static FAutoConsoleVariableRef CVarCachePluginDetails(
		TEXT("GameFeaturePlugin.CachePluginDetails"),
		GCachePluginDetails,
		TEXT("Should use plugin details caching."),
		ECVF_Default);

#if !UE_BUILD_SHIPPING
	static float GBuiltInPluginLoadTimeReportThreshold = 3.0;
	static FAutoConsoleVariableRef CVarBuiltInPluginLoadTimeReportThreshold(
		TEXT("GameFeaturePlugin.BuiltInPluginLoadTimeReportThreshold"),
		GBuiltInPluginLoadTimeReportThreshold,
		TEXT("Built-in plugins that take longer than this amount of time, in seconds, will be reported to the log during startup in non-shipping builds."),
		ECVF_Default);

	static float GBuiltInPluginLoadTimeErrorThreshold = 600;
	static FAutoConsoleVariableRef CVarBuiltInPluginLoadTimeErrorThreshold(
		TEXT("GameFeaturePlugin.BuiltInPluginLoadTimeErrorThreshold"),
		GBuiltInPluginLoadTimeErrorThreshold,
		TEXT("Built-in plugins that take longer than this amount of time, in seconds, will cause an error during startup in non-shipping builds."),
		ECVF_Default);

	static int32 GBuiltInPluginLoadTimeMaxReportCount = 10;
	static FAutoConsoleVariableRef CVarBuiltInPluginLoadTimeMaxReportCount(
		TEXT("GameFeaturePlugin.BuiltInPluginLoadTimeMaxReportCount"),
		GBuiltInPluginLoadTimeMaxReportCount,
		TEXT("When listing worst offenders for built-in plugin load time, show no more than this many plugins, to reduce log spam."),
		ECVF_Default);
#endif // !UE_BUILD_SHIPPING
}

#define GAME_FEATURE_PLUGIN_STATE_LEX_TO_STRING(inEnum, inText)  \
    case(EGameFeaturePluginState::inEnum):                       \
    {                                                            \
        return TEXT(#inEnum);                                    \
    }                                                            

namespace GameFeaturePluginStatePrivate
{
	FString LexToString(EGameFeaturePluginState InEnum)
	{
		switch (InEnum)
		{
			GAME_FEATURE_PLUGIN_STATE_LIST(GAME_FEATURE_PLUGIN_STATE_LEX_TO_STRING)

		default:
			{
				ensureAlwaysMsgf(false, TEXT("Logic error causing a missing LexToString value for EGameFeaturePluginState:%d"), static_cast<uint8>(InEnum));
				return TEXT("ERROR_UNSUPPORTED_ENUM");
			}
		}
	}
}
#undef GAME_FEATURE_PLUGIN_STATE_LEX_TO_STRING

const FString LexToString(const EGameFeatureTargetState GameFeatureTargetState)
{
	switch (GameFeatureTargetState)
	{
		case EGameFeatureTargetState::Installed:
			return TEXT("Installed");
		case EGameFeatureTargetState::Registered:
			return TEXT("Registered"); 
		
		case EGameFeatureTargetState::Loaded:
			return TEXT("Loaded"); 
		case EGameFeatureTargetState::Active:
			return TEXT("Active"); 
		default:
			check(false);
			return TEXT("Unknown");
	}

	static_assert((uint8)EGameFeatureTargetState::Count == 4, TEXT("Update LexToString to include new EGameFeatureTargetState"));
}

void LexFromString(EGameFeatureTargetState& ValueOut, const TCHAR* StringIn)
{
	//Default value if parsing fails
	ValueOut = EGameFeatureTargetState::Count;

	if (!StringIn || StringIn[0] == '\0')
	{
		ensureAlwaysMsgf(false, TEXT("Invalid empty FString used for EGameFeatureTargetState LexFromString."));
		return;
	}

	for (uint8 EnumIndex = 0; EnumIndex < static_cast<uint8>(EGameFeatureTargetState::Count); ++EnumIndex)
	{
		EGameFeatureTargetState StateToCheck = static_cast<EGameFeatureTargetState>(EnumIndex);
		if (LexToString(StateToCheck).Equals(StringIn, ESearchCase::IgnoreCase))
		{
			ValueOut = StateToCheck;
			return;
		}
	}

	ensureAlwaysMsgf(false, TEXT("Invalid FString { %s } used for EGameFeatureTargetState LexFromString. Does not correspond to any EGameFeatureTargetState!"), StringIn);
}

FGameFeaturePluginIdentifier::FGameFeaturePluginIdentifier(FString PluginURL)
{
	FromPluginURL(MoveTemp(PluginURL));
}

FGameFeaturePluginIdentifier::FGameFeaturePluginIdentifier(FGameFeaturePluginIdentifier&& Other)
	: FGameFeaturePluginIdentifier(MoveTemp(Other.PluginURL))
{
	Other.IdentifyingURLSubset.Reset();
	Other.PluginProtocol = EGameFeaturePluginProtocol::Unknown;
}

FGameFeaturePluginIdentifier& FGameFeaturePluginIdentifier::operator=(FGameFeaturePluginIdentifier&& Other)
{
	FromPluginURL(MoveTemp(Other.PluginURL));
	Other.IdentifyingURLSubset.Reset();
	Other.PluginProtocol = EGameFeaturePluginProtocol::Unknown;
	return *this;
}

void FGameFeaturePluginIdentifier::FromPluginURL(FString PluginURLIn)
{
	//Make sure we have no stale data
	IdentifyingURLSubset.Reset();
	PluginURL = MoveTemp(PluginURLIn);

	PluginProtocol = UGameFeaturesSubsystem::GetPluginURLProtocol(PluginURL);

	if (ensureAlwaysMsgf( ((PluginProtocol != EGameFeaturePluginProtocol::Unknown) 
						&& (PluginProtocol != EGameFeaturePluginProtocol::Count)),
						TEXT("Invalid PluginProtocol in PluginURL %s"), *PluginURL))
	{

		int32 PluginProtocolEndIndex = FCString::Strlen(UE::GameFeatures::GameFeaturePluginProtocolPrefix(PluginProtocol));
		int32 FirstOptionIndex = PluginURL.Find(UE::GameFeatures::PluginURLStructureInfo::OptionSeperator, ESearchCase::IgnoreCase, ESearchDir::FromStart, PluginProtocolEndIndex);
		
		//If we don't have any options, then the IdentifyingURLSubset is just our entire URL except the protocol string
		if (FirstOptionIndex == INDEX_NONE)
		{
			IdentifyingURLSubset = FStringView(PluginURL).RightChop(PluginProtocolEndIndex);
		}
		//The IdentifyingURLSubset will be the string between the end of the protocol string and before the first option
		else
		{
			const int32 IdentifierCharCount = (FirstOptionIndex - PluginProtocolEndIndex);
			IdentifyingURLSubset = FStringView(PluginURL).Mid(PluginProtocolEndIndex, IdentifierCharCount);
		}
	}
}

bool FGameFeaturePluginIdentifier::operator==(const FGameFeaturePluginIdentifier& Other) const
{
	return ((PluginProtocol == Other.PluginProtocol) &&
			(IdentifyingURLSubset.Equals(Other.IdentifyingURLSubset, ESearchCase::CaseSensitive)));
}

bool FGameFeaturePluginIdentifier::ExactMatchesURL(const FString& PluginURLIn) const
{
	return GetFullPluginURL().Equals(PluginURLIn, ESearchCase::IgnoreCase);
}

void FGameFeatureStateChangeContext::SetRequiredWorldContextHandle(FName Handle)
{
	WorldContextHandle = Handle;
}

bool FGameFeatureStateChangeContext::ShouldApplyToWorldContext(const FWorldContext& WorldContext) const
{
	if (WorldContextHandle.IsNone())
	{
		return true;
	}
	if (WorldContext.ContextHandle == WorldContextHandle)
	{
		return true;
	}
	return false;
}

bool FGameFeatureStateChangeContext::ShouldApplyUsingOtherContext(const FGameFeatureStateChangeContext& OtherContext) const
{
	if (OtherContext == *this)
	{
		return true;
	}

	// If other context is less restrictive, apply
	if (OtherContext.WorldContextHandle.IsNone())
	{
		return true;
	}

	return false;
}

FSimpleDelegate FGameFeatureDeactivatingContext::PauseDeactivationUntilComplete(FString InPauserTag)
{
	UE_LOG(LogGameFeatures, Display, TEXT("Deactivation of %.*s paused by %s"), PluginName.Len(), PluginName.GetData(), *InPauserTag);

	++NumPausers;
	return FSimpleDelegate::CreateLambda(
		[CompletionCallback=CompletionCallback, PauserTag=MoveTemp(InPauserTag)]() { CompletionCallback(PauserTag); }
	);
}

void UGameFeaturesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogGameFeatures, Log, TEXT("Initializing game features subsystem"));

	// Create the game-specific policy manager
	check(!bInitializedPolicyManager && (GameSpecificPolicies == nullptr));

	const FSoftClassPath& PolicyClassPath = GetDefault<UGameFeaturesSubsystemSettings>()->GameFeaturesManagerClassName;

	UClass* SingletonClass = nullptr;
	if (!PolicyClassPath.IsNull())
	{
		SingletonClass = LoadClass<UGameFeaturesProjectPolicies>(nullptr, *PolicyClassPath.ToString());
	}

	if (SingletonClass == nullptr)
	{
		SingletonClass = UDefaultGameFeaturesProjectPolicies::StaticClass();
	}
		
	GameSpecificPolicies = NewObject<UGameFeaturesProjectPolicies>(this, SingletonClass);
	check(GameSpecificPolicies);

	UAssetManager::CallOrRegister_OnAssetManagerCreated(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAssetManagerCreated));

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ListGameFeaturePlugins"),
		TEXT("Prints game features plugins and their current state to log. (options: [-activeonly] [-alphasort] [-csv])"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateUObject(this, &ThisClass::ListGameFeaturePlugins),
		ECVF_Default);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("LoadGameFeaturePlugin"),
		TEXT("Loads and activates a game feature plugin by PluginName or URL"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
		{
			if (Args.Num() > 0)
			{
				FString PluginURL;
				if (!UGameFeaturesSubsystem::Get().GetPluginURLByName(Args[0], /*out*/ PluginURL))
				{
					PluginURL = Args[0];
				}
				UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(PluginURL, FGameFeaturePluginLoadComplete());
			}
			else
			{
				Ar.Logf(TEXT("Expected a game feature plugin URL as an argument"));
			}
		}),
		ECVF_Cheat);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("DeactivateGameFeaturePlugin"),
		TEXT("Deactivates a game feature plugin by PluginName or URL"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
		{
			if (Args.Num() > 0)
			{
				FString PluginURL;
				if (!UGameFeaturesSubsystem::Get().GetPluginURLByName(Args[0], /*out*/ PluginURL))
				{
					PluginURL = Args[0];
				}
				UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(PluginURL, FGameFeaturePluginLoadComplete());
			}
			else
			{
				Ar.Logf(TEXT("Expected a game feature plugin URL as an argument"));
			}
		}),
		ECVF_Cheat);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("UnloadGameFeaturePlugin"),
		TEXT("Unloads a game feature plugin by PluginName or URL"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
		{
			if (Args.Num() > 0)
			{
				FString PluginURL;
				if (!UGameFeaturesSubsystem::Get().GetPluginURLByName(Args[0], /*out*/ PluginURL))
				{
					PluginURL = Args[0];
				}
				UGameFeaturesSubsystem::Get().UnloadGameFeaturePlugin(PluginURL);
			}
			else
			{
				Ar.Logf(TEXT("Expected a game feature plugin URL as an argument"));
			}
		}),
		ECVF_Cheat);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("UnloadAndKeepRegisteredGameFeaturePlugin"),
		TEXT("Unloads a game feature plugin by PluginName or URL but keeps it registered"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
			{
				if (Args.Num() > 0)
				{
					FString PluginURL;
					if (!UGameFeaturesSubsystem::Get().GetPluginURLByName(Args[0], /*out*/ PluginURL))
					{
						PluginURL = Args[0];
					}
					UGameFeaturesSubsystem::Get().UnloadGameFeaturePlugin(PluginURL, true);
				}
				else
				{
					Ar.Logf(TEXT("Expected a game feature plugin URL as an argument"));
				}
			}),
		ECVF_Cheat);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ReleaseGameFeaturePlugin"),
		TEXT("Releases a game feature plugin's InstallBundle data by PluginName or URL"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
			{
				if (Args.Num() > 0)
				{
					FString PluginURL;
					if (!UGameFeaturesSubsystem::Get().GetPluginURLByName(Args[0], /*out*/ PluginURL))
					{
						PluginURL = Args[0];
					}
					UGameFeaturesSubsystem::Get().ReleaseGameFeaturePlugin(PluginURL);
				}
				else
				{
					Ar.Logf(TEXT("Expected a game feature plugin URL as an argument"));
				}
			}),
		ECVF_Cheat);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("CancelGameFeaturePlugin"),
		TEXT("Cancel any state changes for a game feature plugin by PluginName or URL"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
			{
				if (Args.Num() > 0)
				{
					FString PluginURL;
					if (!UGameFeaturesSubsystem::Get().GetPluginURLByName(Args[0], /*out*/ PluginURL))
					{
						PluginURL = Args[0];
					}
					UGameFeaturesSubsystem::Get().CancelGameFeatureStateChange(PluginURL);
				}
				else
				{
					Ar.Logf(TEXT("Expected a game feature plugin URL as an argument"));
				}
			}),
		ECVF_Cheat);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("TerminateGameFeaturePlugin"),
		TEXT("Terminates a game feature plugin by PluginName or URL"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
			{
				if (Args.Num() > 0)
				{
					FString PluginURL;
					if (!UGameFeaturesSubsystem::Get().GetPluginURLByName(Args[0], /*out*/ PluginURL))
					{
						PluginURL = Args[0];
					}
					UGameFeaturesSubsystem::Get().TerminateGameFeaturePlugin(PluginURL);
				}
				else
				{
					Ar.Logf(TEXT("Expected a game feature plugin URL as an argument"));
				}
			}),
		ECVF_Cheat);
}

void UGameFeaturesSubsystem::Deinitialize()
{
	UE_LOG(LogGameFeatures, Log, TEXT("Shutting down game features subsystem"));

	if ((GameSpecificPolicies != nullptr) && bInitializedPolicyManager)
	{
		GameSpecificPolicies->ShutdownGameFeatureManager();
	}
	GameSpecificPolicies = nullptr;
	bInitializedPolicyManager = false;
}

void UGameFeaturesSubsystem::OnAssetManagerCreated()
{
	check(!bInitializedPolicyManager && (GameSpecificPolicies != nullptr));

	// Make sure the game has the appropriate asset manager configuration or we won't be able to load game feature data assets
	FPrimaryAssetId DummyGameFeatureDataAssetId(UGameFeatureData::StaticClass()->GetFName(), NAME_None);
	FPrimaryAssetRules GameDataRules = UAssetManager::Get().GetPrimaryAssetRules(DummyGameFeatureDataAssetId);
	if (GameDataRules.IsDefault())
	{
		UE_LOG(LogGameFeatures, Error, TEXT("Asset manager settings do not include a rule for assets of type %s, which is required for game feature plugins to function"), *UGameFeatureData::StaticClass()->GetName());
	}

	// Create the game-specific policy
	UE_LOG(LogGameFeatures, Verbose, TEXT("Initializing game features policy (type %s)"), *GameSpecificPolicies->GetClass()->GetName());
	GameSpecificPolicies->InitGameFeatureManager();
	bInitializedPolicyManager = true;
}

TSharedPtr<FStreamableHandle> UGameFeaturesSubsystem::LoadGameFeatureData(const FString& GameFeatureToLoad)
{
	return UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(FSoftObjectPath(GameFeatureToLoad));
}

void UGameFeaturesSubsystem::UnloadGameFeatureData(const UGameFeatureData* GameFeatureToUnload)
{
	UAssetManager& LocalAssetManager = UAssetManager::Get();
	LocalAssetManager.UnloadPrimaryAsset(GameFeatureToUnload->GetPrimaryAssetId());
}

void UGameFeaturesSubsystem::AddGameFeatureToAssetManager(const UGameFeatureData* GameFeatureToAdd, const FString& PluginName, TArray<FName>& OutNewPrimaryAssetTypes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GFP_AddToAssetManager);
	check(GameFeatureToAdd);
	FString PluginRootPath = TEXT("/") + PluginName + TEXT("/");
	UAssetManager& LocalAssetManager = UAssetManager::Get();
	IAssetRegistry& LocalAssetRegistry = LocalAssetManager.GetAssetRegistry();

	LocalAssetManager.PushBulkScanning();

	// Add the GameFeatureData itself to the primary asset list
#if WITH_EDITOR
	// In the editor, we may not have scanned the FAssetData yet if during startup, but that is fine because we can gather bundles from the object itself, so just create the FAssetData from the object
	LocalAssetManager.RegisterSpecificPrimaryAsset(GameFeatureToAdd->GetPrimaryAssetId(), FAssetData(GameFeatureToAdd));
#else
	// In non-editor, the asset bundle data is compiled out, so it must be gathered from the asset registry instead
	LocalAssetManager.RegisterSpecificPrimaryAsset(GameFeatureToAdd->GetPrimaryAssetId(), LocalAssetRegistry.GetAssetByObjectPath(FSoftObjectPath(GameFeatureToAdd), true));
#endif // WITH_EDITOR

	// @TODO: HACK - There is no guarantee that the plugin mount point was added before the initial asset scan.
	// If not, ScanPathsForPrimaryAssets will fail to find primary assets without a syncronous scan.
	// A proper fix for this would be to handle all the primary asset discovery internally ins the asset manager 
	// instead of doing it here.
	// We just mounted the folder that contains these primary assets and the editor background scan may not
	// not be finished by the time this is called, but a rescan will happen later in OnAssetRegistryFilesLoaded 
	// as long as LocalAssetRegistry.IsLoadingAssets() is true.
	const bool bForceSynchronousScan = !LocalAssetRegistry.IsLoadingAssets();

	for (FPrimaryAssetTypeInfo TypeInfo : GameFeatureToAdd->GetPrimaryAssetTypesToScan())
	{
		// @TODO: we shouldn't be accessing private data here. Need a better way to do this
		for (FDirectoryPath& Path : TypeInfo.Directories)
		{
			// Convert plugin-relative paths to full package paths
			FixPluginPackagePath(Path.Path, PluginRootPath, false);
		}

		// This function also fills out runtime data on the copy
		if (!LocalAssetManager.ShouldScanPrimaryAssetType(TypeInfo))
		{
			continue;
		}

		FPrimaryAssetTypeInfo ExistingAssetTypeInfo;
		const bool bAlreadyExisted = LocalAssetManager.GetPrimaryAssetTypeInfo(FPrimaryAssetType(TypeInfo.PrimaryAssetType), /*out*/ ExistingAssetTypeInfo);
		LocalAssetManager.ScanPathsForPrimaryAssets(TypeInfo.PrimaryAssetType, TypeInfo.AssetScanPaths, TypeInfo.AssetBaseClassLoaded, TypeInfo.bHasBlueprintClasses, TypeInfo.bIsEditorOnly, bForceSynchronousScan);

		if (!bAlreadyExisted)
		{
			OutNewPrimaryAssetTypes.Add(TypeInfo.PrimaryAssetType);

			// If we did not previously scan anything for a primary asset type that is in our config, try to reuse the cook rules from the config instead of the one in the gamefeaturedata, which should not be modifying cook rules
			const FPrimaryAssetTypeInfo* ConfigTypeInfo = LocalAssetManager.GetSettings().PrimaryAssetTypesToScan.FindByPredicate([&TypeInfo](const FPrimaryAssetTypeInfo& PATI) -> bool { return PATI.PrimaryAssetType == TypeInfo.PrimaryAssetType; });
			if (ConfigTypeInfo)
			{
				LocalAssetManager.SetPrimaryAssetTypeRules(TypeInfo.PrimaryAssetType, ConfigTypeInfo->Rules);
			}
			else
			{
				LocalAssetManager.SetPrimaryAssetTypeRules(TypeInfo.PrimaryAssetType, TypeInfo.Rules);
			}
		}
	}

	LocalAssetManager.PopBulkScanning();
}

void UGameFeaturesSubsystem::RemoveGameFeatureFromAssetManager(const UGameFeatureData* GameFeatureToRemove, const FString& PluginName, const TArray<FName>& AddedPrimaryAssetTypes)
{
	check(GameFeatureToRemove);
	FString PluginRootPath = TEXT("/") + PluginName + TEXT("/");
	UAssetManager& LocalAssetManager = UAssetManager::Get();

	for (FPrimaryAssetTypeInfo TypeInfo : GameFeatureToRemove->GetPrimaryAssetTypesToScan())
	{
		if (AddedPrimaryAssetTypes.Contains(TypeInfo.PrimaryAssetType))
		{
			LocalAssetManager.RemovePrimaryAssetType(TypeInfo.PrimaryAssetType);
			continue;
		}

		for (FDirectoryPath& Path : TypeInfo.Directories)
		{
			FixPluginPackagePath(Path.Path, PluginRootPath, false);
		}

		// This function also fills out runtime data on the copy
		if (!LocalAssetManager.ShouldScanPrimaryAssetType(TypeInfo))
		{
			continue;
		}

		LocalAssetManager.RemoveScanPathsForPrimaryAssets(TypeInfo.PrimaryAssetType, TypeInfo.AssetScanPaths, TypeInfo.AssetBaseClassLoaded, TypeInfo.bHasBlueprintClasses, TypeInfo.bIsEditorOnly);
	}
}

void UGameFeaturesSubsystem::AddObserver(UObject* Observer)
{
	//@TODO: GameFeaturePluginEnginePush: May want to warn if one is added after any game feature plugins are already initialized, or go to a CallOrRegister sort of pattern
	check(Observer);
	if (ensureAlwaysMsgf(Cast<IGameFeatureStateChangeObserver>(Observer) != nullptr, TEXT("Observers must implement the IGameFeatureStateChangeObserver interface.")))
	{
		Observers.AddUnique(Observer);
	}
}

void UGameFeaturesSubsystem::RemoveObserver(UObject* Observer)
{
	check(Observer);
	Observers.RemoveSingleSwap(Observer);
}

FString UGameFeaturesSubsystem::GetPluginURL_FileProtocol(const FString& PluginDescriptorPath)
{
	return TEXT("file:") + PluginDescriptorPath;
}

FString UGameFeaturesSubsystem::GetPluginURL_InstallBundleProtocol(const FString& PluginName, TArrayView<const FString> BundleNames)
{
	FInstallBundlePluginProtocolMetaData ProtocolMetadata;
	for (const FString& BundleName : BundleNames)
	{
		ProtocolMetadata.InstallBundles.Add(FName(BundleName));
	}
	return GetPluginURL_InstallBundleProtocol(PluginName, ProtocolMetadata);
}

FString UGameFeaturesSubsystem::GetPluginURL_InstallBundleProtocol(const FString& PluginName, const FString& BundleName)
{
	return GetPluginURL_InstallBundleProtocol(PluginName, MakeArrayView(&BundleName, 1));
}

FString UGameFeaturesSubsystem::GetPluginURL_InstallBundleProtocol(const FString& PluginName, const TArrayView<const FName> BundleNames)
{
	FInstallBundlePluginProtocolMetaData ProtocolMetadata;
	ProtocolMetadata.InstallBundles.Append(BundleNames.GetData(), BundleNames.Num());
	return GetPluginURL_InstallBundleProtocol(PluginName, ProtocolMetadata);
}

FString UGameFeaturesSubsystem::GetPluginURL_InstallBundleProtocol(const FString& PluginName, FName BundleName)
{
	return GetPluginURL_InstallBundleProtocol(PluginName, MakeArrayView(&BundleName, 1));
}

FString UGameFeaturesSubsystem::GetPluginURL_InstallBundleProtocol(const FString& PluginName, const FInstallBundlePluginProtocolMetaData& ProtocolMetadata)
{
	ensure (ProtocolMetadata.InstallBundles.Num() > 0);
	FString Path;
	Path += UE::GameFeatures::GameFeaturePluginProtocolPrefix(EGameFeaturePluginProtocol::InstallBundle);
	Path += PluginName;
	Path += UE::GameFeatures::PluginURLStructureInfo::OptionSeperator;
	Path += ProtocolMetadata.ToString();

	return Path;
}

EGameFeaturePluginProtocol UGameFeaturesSubsystem::GetPluginURLProtocol(FStringView PluginURL)
{
	for (EGameFeaturePluginProtocol Protocol : TEnumRange<EGameFeaturePluginProtocol>())
	{
		if (UGameFeaturesSubsystem::IsPluginURLProtocol(PluginURL, Protocol))
		{
			return Protocol;
		}
	}
	return EGameFeaturePluginProtocol::Unknown;
}

bool UGameFeaturesSubsystem::IsPluginURLProtocol(FStringView PluginURL, EGameFeaturePluginProtocol PluginProtocol)
{
	return PluginURL.StartsWith(UE::GameFeatures::GameFeaturePluginProtocolPrefix(PluginProtocol));
}

void UGameFeaturesSubsystem::OnGameFeatureTerminating(const FString& PluginName, const FString& PluginURL)
{
	CallbackObservers(EObserverCallback::Terminating, PluginURL, &PluginName);

	if (!PluginName.IsEmpty())
	{
		// Unmap plugin name to plugin URL
		GameFeaturePluginNameToPathMap.Remove(PluginName);
	}
}

void UGameFeaturesSubsystem::OnGameFeatureCheckingStatus(const FString& PluginURL)
{
	CallbackObservers(EObserverCallback::CheckingStatus, PluginURL);
}

void UGameFeaturesSubsystem::OnGameFeatureStatusKnown(const FString& PluginName, const FString& PluginURL)
{
	// Map plugin name to plugin URL
	if (ensure(!GameFeaturePluginNameToPathMap.Contains(PluginName)))
	{
		GameFeaturePluginNameToPathMap.Add(PluginName, PluginURL);
	}
}

void UGameFeaturesSubsystem::OnGameFeatureRegistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL)
{
	CallbackObservers(EObserverCallback::Registering, PluginURL, &PluginName, GameFeatureData);

	for (UGameFeatureAction* Action : GameFeatureData->GetActions())
	{
		if (Action != nullptr)
		{
			Action->OnGameFeatureRegistering();
		}
	}
}

void UGameFeaturesSubsystem::OnGameFeatureUnregistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL)
{
	CallbackObservers(EObserverCallback::Unregistering, PluginURL, &PluginName, GameFeatureData);

	for (UGameFeatureAction* Action : GameFeatureData->GetActions())
	{
		if (Action != nullptr)
		{
			Action->OnGameFeatureUnregistering();
		}
	}
}

void UGameFeaturesSubsystem::OnGameFeatureLoading(const UGameFeatureData* GameFeatureData, const FString& PluginURL)
{
	CallbackObservers(EObserverCallback::Loading, PluginURL, nullptr, GameFeatureData);

	for (UGameFeatureAction* Action : GameFeatureData->GetActions())
	{
		if (Action != nullptr)
		{
			Action->OnGameFeatureLoading();
		}
	}
}

void UGameFeaturesSubsystem::OnGameFeatureActivating(const UGameFeatureData* GameFeatureData, const FString& PluginName, FGameFeatureActivatingContext& Context, const FString& PluginURL)
{
	CallbackObservers(EObserverCallback::Activating, PluginURL, &PluginName, GameFeatureData);

	for (UGameFeatureAction* Action : GameFeatureData->GetActions())
	{
		if (Action != nullptr)
		{
			Action->OnGameFeatureActivating(Context);
		}
	}
}

void UGameFeaturesSubsystem::OnGameFeatureDeactivating(const UGameFeatureData* GameFeatureData, const FString& PluginName, FGameFeatureDeactivatingContext& Context, const FString& PluginURL)
{
	CallbackObservers(EObserverCallback::Deactivating, PluginURL, &PluginName, GameFeatureData, &Context);

	for (UGameFeatureAction* Action : GameFeatureData->GetActions())
	{
		if (Action != nullptr)
		{
			Action->OnGameFeatureDeactivating(Context);
		}
	}
}

void UGameFeaturesSubsystem::OnGameFeaturePauseChange(const FString& PluginURL, const FString& PluginName, FGameFeaturePauseStateChangeContext& Context)
{
	CallbackObservers(EObserverCallback::PauseChanged, PluginURL, &PluginName, nullptr, &Context);
}

const UGameFeatureData* UGameFeaturesSubsystem::GetDataForStateMachine(UGameFeaturePluginStateMachine* GFSM) const
{
	return GFSM->GetGameFeatureDataForActivePlugin();
}

const UGameFeatureData* UGameFeaturesSubsystem::GetRegisteredDataForStateMachine(UGameFeaturePluginStateMachine* GFSM) const
{
	return GFSM->GetGameFeatureDataForRegisteredPlugin();
}

void UGameFeaturesSubsystem::GetGameFeatureDataForActivePlugins(TArray<const UGameFeatureData*>& OutActivePluginFeatureDatas)
{
	for (auto StateMachineIt = GameFeaturePluginStateMachines.CreateConstIterator(); StateMachineIt; ++StateMachineIt)
	{
		if (UGameFeaturePluginStateMachine* GFSM = StateMachineIt.Value())
		{
			if (const UGameFeatureData* GameFeatureData = GFSM->GetGameFeatureDataForActivePlugin())
			{
				OutActivePluginFeatureDatas.Add(GameFeatureData);
			}
		}
	}
}

const UGameFeatureData* UGameFeaturesSubsystem::GetGameFeatureDataForActivePluginByURL(const FString& PluginURL)
{
	if (UGameFeaturePluginStateMachine* GFSM = FindGameFeaturePluginStateMachine(PluginURL))
	{
		return GFSM->GetGameFeatureDataForActivePlugin();
	}

	return nullptr;
}

const UGameFeatureData* UGameFeaturesSubsystem::GetGameFeatureDataForRegisteredPluginByURL(const FString& PluginURL, bool bCheckForRegistering /*= false*/)
{
	if (UGameFeaturePluginStateMachine* GFSM = FindGameFeaturePluginStateMachine(PluginURL))
	{
		return GFSM->GetGameFeatureDataForRegisteredPlugin(bCheckForRegistering);
	}

	return nullptr;
}

bool UGameFeaturesSubsystem::IsGameFeaturePluginInstalled(const FString& PluginURL) const
{
	if (const UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		return StateMachine->GetCurrentState() >= EGameFeaturePluginState::Installed;
	}
	return false;
}

bool UGameFeaturesSubsystem::IsGameFeaturePluginRegistered(const FString& PluginURL, bool bCheckForRegistering /*= false*/) const
{
	if (const UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		const EGameFeaturePluginState CurrentState = StateMachine->GetCurrentState();

		return StateMachine->GetCurrentState() >= EGameFeaturePluginState::Registered || (bCheckForRegistering && CurrentState == EGameFeaturePluginState::Registering);
	}
	return false;
}

bool UGameFeaturesSubsystem::IsGameFeaturePluginLoaded(const FString& PluginURL) const
{
	if (const UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		return StateMachine->GetCurrentState() >= EGameFeaturePluginState::Loaded;
	}
	return false;
}

void UGameFeaturesSubsystem::LoadGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginLoadComplete& CompleteDelegate)
{
	const bool bIsPluginAllowed = GameSpecificPolicies->IsPluginAllowed(PluginURL);
	if (!bIsPluginAllowed)
	{
		CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::PluginNotAllowed)));
		return;
	}

	UGameFeaturePluginStateMachine* StateMachine = FindOrCreateGameFeaturePluginStateMachine(PluginURL);
	
	if (!StateMachine->IsRunning() && StateMachine->GetCurrentState() == EGameFeaturePluginState::Active)
	{
		// TODO: Resolve the activated case here, this is needed because in a PIE environment the plugins
		// are not sandboxed, and we need to do simulate a successful activate call in order run GFP systems 
		// on whichever Role runs second between client and server.

		// Refire the observer for Activated and do nothing else.
		CallbackObservers(EObserverCallback::Activating, PluginURL, &StateMachine->GetPluginName(), StateMachine->GetGameFeatureDataForActivePlugin());
	}

	ChangeGameFeatureDestination(StateMachine, FGameFeaturePluginStateRange(EGameFeaturePluginState::Loaded, EGameFeaturePluginState::Active), CompleteDelegate);
}

void UGameFeaturesSubsystem::LoadAndActivateGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginLoadComplete& CompleteDelegate)
{
	ChangeGameFeatureTargetState(PluginURL, EGameFeatureTargetState::Active, CompleteDelegate);
}

void UGameFeaturesSubsystem::ChangeGameFeatureTargetState(const FString& PluginURL, EGameFeatureTargetState TargetState, const FGameFeaturePluginChangeStateComplete& CompleteDelegate)
{
	EGameFeaturePluginState TargetPluginState = EGameFeaturePluginState::MAX;

	switch (TargetState)
	{
	case EGameFeatureTargetState::Installed:	TargetPluginState = EGameFeaturePluginState::Installed;		break;
	case EGameFeatureTargetState::Registered:	TargetPluginState = EGameFeaturePluginState::Registered;	break;
	case EGameFeatureTargetState::Loaded:		TargetPluginState = EGameFeaturePluginState::Loaded;		break;
	case EGameFeatureTargetState::Active:		TargetPluginState = EGameFeaturePluginState::Active;		break;
	}

	// Make sure we have coverage on all values of EGameFeatureTargetState
	static_assert(std::underlying_type<EGameFeatureTargetState>::type(EGameFeatureTargetState::Count) == 4, "");
	check(TargetPluginState != EGameFeaturePluginState::MAX);

	const bool bIsPluginAllowed = GameSpecificPolicies->IsPluginAllowed(PluginURL);

	UGameFeaturePluginStateMachine* StateMachine = nullptr;
	if (!bIsPluginAllowed)
	{
		StateMachine = FindGameFeaturePluginStateMachine(PluginURL);
		if (!StateMachine)
		{
			UE_LOG(LogGameFeatures, Log, TEXT("Cannot create GFP State Machine: Plugin not allowed %s"), *PluginURL);

			CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::PluginNotAllowed)));
			return;
		}
	}
	else
	{
		StateMachine = FindOrCreateGameFeaturePluginStateMachine(PluginURL);
	}
	
	check(StateMachine);

	if (!bIsPluginAllowed)
	{
		if (TargetPluginState > StateMachine->GetCurrentState() || TargetPluginState > StateMachine->GetDestination())
		{
			UE_LOG(LogGameFeatures, Log, TEXT("Cannot change game feature target state: Plugin not allowed %s"), *PluginURL);

			CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::PluginNotAllowed)));
			return;
		}
	}

	if (TargetState == EGameFeatureTargetState::Active &&
		!StateMachine->IsRunning() && 
		StateMachine->GetCurrentState() == TargetPluginState)
	{
		// TODO: Resolve the activated case here, this is needed because in a PIE environment the plugins
		// are not sandboxed, and we need to do simulate a successful activate call in order run GFP systems 
		// on whichever Role runs second between client and server.

		// Refire the observer for Activated and do nothing else.
		CallbackObservers(EObserverCallback::Activating, PluginURL, &StateMachine->GetPluginName(), StateMachine->GetGameFeatureDataForActivePlugin());
	}
	
	if (ShouldUpdatePluginURLData(PluginURL))
	{
		UpdateGameFeaturePluginURL(PluginURL, FGameFeaturePluginUpdateURLComplete());
	}

	ChangeGameFeatureDestination(StateMachine, FGameFeaturePluginStateRange(TargetPluginState), CompleteDelegate);
}

void UGameFeaturesSubsystem::UpdateGameFeaturePluginURL(const FString& NewPluginURL)
{
	UpdateGameFeaturePluginURL(NewPluginURL, FGameFeaturePluginUpdateURLComplete());
};

void UGameFeaturesSubsystem::UpdateGameFeaturePluginURL(const FString& NewPluginURL, const FGameFeaturePluginUpdateURLComplete& CompleteDelegate)
{
	UGameFeaturePluginStateMachine* StateMachine = nullptr;
	StateMachine = FindGameFeaturePluginStateMachine(NewPluginURL);
	if (!StateMachine)
	{
		CompleteDelegate.ExecuteIfBound(MakeError(TEXT("GameFeaturePlugin.BadURL")));
		return;
	}

	check(StateMachine);

	const bool bUpdated = StateMachine->TryUpdatePluginURLData(NewPluginURL);
	if (!bUpdated)
	{
		CompleteDelegate.ExecuteIfBound(MakeError(TEXT("GameFeaturePlugin.No_URL_Update_Needed")));
	}
	else
	{
		FString* PluginURL = GameFeaturePluginNameToPathMap.Find(StateMachine->GetPluginName());
		if (!ensureAlwaysMsgf(PluginURL, TEXT("Attempt to UpdateGameFeaturePluginURL before GameFeaturePlugin has been added to the GameFeaturePluginNameToPathMap! URL:%s"), *NewPluginURL))
		{
			CompleteDelegate.ExecuteIfBound(MakeError(TEXT("GameFeaturePlugin.UpdateTooEarly")));
			return;
		}

		*PluginURL = NewPluginURL;
		CompleteDelegate.ExecuteIfBound(MakeValue());
	}
}

bool UGameFeaturesSubsystem::ShouldUpdatePluginURLData(const FString& NewPluginURL)
{
	UGameFeaturePluginStateMachine* StateMachine = nullptr;
	StateMachine = FindGameFeaturePluginStateMachine(NewPluginURL);
	if (!StateMachine || !StateMachine->IsStatusKnown())
	{
		return false;
	}

	//Should always be valid at this point
	check(StateMachine);
	
	//Make sure our StateMachine isn't in terminal, don't want to update Terminal plugins
	if (TerminalGameFeaturePluginStateMachines.Contains(StateMachine) || (StateMachine->GetCurrentState() == EGameFeaturePluginState::Terminal))
	{
		return false;
	}

	if (StateMachine->GetPluginURL().Equals(NewPluginURL, ESearchCase::IgnoreCase))
	{
		return false;
	}

	return true;
}

bool UGameFeaturesSubsystem::GetGameFeaturePluginInstallPercent(const FString& PluginURL, float& Install_Percent) const
{
	if (const UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		if (StateMachine->IsStatusKnown() && StateMachine->IsAvailable())
		{
			const FGameFeaturePluginStateInfo& StateInfo = StateMachine->GetCurrentStateInfo();
			if (StateInfo.State == EGameFeaturePluginState::Downloading)
			{
				Install_Percent = StateInfo.Progress;
			}
			else if (StateInfo.State >= EGameFeaturePluginState::Installed)
			{
				Install_Percent = 1.0f;
			}
			else
			{
				Install_Percent = 0.0f;
			}
			return true;
		}
	}
	return false;
}

bool UGameFeaturesSubsystem::IsGameFeaturePluginActive(const FString& PluginURL, bool bCheckForActivating /*= false*/) const
{
	if (const UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		const EGameFeaturePluginState CurrentState = StateMachine->GetCurrentState();

		return CurrentState == EGameFeaturePluginState::Active || (bCheckForActivating && CurrentState == EGameFeaturePluginState::Activating);
	}

	return false;
}

void UGameFeaturesSubsystem::DeactivateGameFeaturePlugin(const FString& PluginURL)
{
	DeactivateGameFeaturePlugin(PluginURL, FGameFeaturePluginDeactivateComplete());
}

void UGameFeaturesSubsystem::DeactivateGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginDeactivateComplete& CompleteDelegate)
{
	if (UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		ChangeGameFeatureDestination(StateMachine, FGameFeaturePluginStateRange(EGameFeaturePluginState::Terminal, EGameFeaturePluginState::Loaded), CompleteDelegate);
	}
	else
	{
		CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::BadURL)));
	}
}

void UGameFeaturesSubsystem::UnloadGameFeaturePlugin(const FString& PluginURL, bool bKeepRegistered /*= false*/)
{
	UnloadGameFeaturePlugin(PluginURL, FGameFeaturePluginUnloadComplete(), bKeepRegistered);
}

void UGameFeaturesSubsystem::UnloadGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginUnloadComplete& CompleteDelegate, bool bKeepRegistered /*= false*/)
{
	if (UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		EGameFeaturePluginState TargetPluginState = bKeepRegistered ? EGameFeaturePluginState::Registered : EGameFeaturePluginState::Installed;
		ChangeGameFeatureDestination(StateMachine, FGameFeaturePluginStateRange(EGameFeaturePluginState::Terminal, TargetPluginState), CompleteDelegate);
	}
	else
	{
		CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::BadURL)));
	}
}

void UGameFeaturesSubsystem::ReleaseGameFeaturePlugin(const FString& PluginURL)
{
	ReleaseGameFeaturePlugin(PluginURL, FGameFeaturePluginReleaseComplete());
}

void UGameFeaturesSubsystem::ReleaseGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginReleaseComplete& CompleteDelegate)
{
	if (UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		ChangeGameFeatureDestination(StateMachine, FGameFeaturePluginStateRange(EGameFeaturePluginState::Terminal, EGameFeaturePluginState::StatusKnown), CompleteDelegate);
	}
	else
	{
		CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::BadURL)));
	}
}

void UGameFeaturesSubsystem::UninstallGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginUninstallComplete& CompleteDelegate)
{
	//FindOrCreate so that we can make sure we uninstall data for plugins that were installed on a previous application run
	//but have not yet been requested on this application run and so are not yet in the plugin list but might have data on disk
	//to uninstall
	UGameFeaturePluginStateMachine* StateMachine = FindOrCreateGameFeaturePluginStateMachine(PluginURL);
	check(StateMachine);

	//We may need to update our PluginURL to force certain metadata changes to facilitate this uninstall
	//This tracks what URL we actually will pass in to the terminate as we can rely on the fact that Terminate
	//will update any important Metadata from this new URL before beginning it's terminate
	FString PluginURLForTerminate = PluginURL;

	//InstallBundle Protocol GameFeatures may need to change their metadata to force this uninstall
	if (UGameFeaturesSubsystem::IsPluginURLProtocol(PluginURL, EGameFeaturePluginProtocol::InstallBundle))
	{
		// Parse a duplicate version of our current Metadata from the URL
		FInstallBundlePluginProtocolMetaData ProtocolMetadata;
		if (!FInstallBundlePluginProtocolMetaData::FromString(PluginURL, ProtocolMetadata))
		{
			CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::BadURL)));
		}

		// Need to force on bUninstallBeforeTerminate if it wasn't already set to on in our Metadata
		if (!ProtocolMetadata.bUninstallBeforeTerminate)
		{
			ProtocolMetadata.bUninstallBeforeTerminate = true;
			
			FString PluginFilename;

			//Try and pull PluginFilename from the StateMachine first, but this may not be set yet
			//as this may be running too early before we have parsed the URL
			StateMachine->GetPluginFilename(PluginFilename);
			if (PluginFilename.IsEmpty())
			{
				//The PluginIdentifyingString is currently just the PluginFilename so we can fallback to that
				PluginFilename = StateMachine->GetPluginIdentifier().GetIdentifyingString();
			}
			check(!PluginFilename.IsEmpty());

			PluginURLForTerminate = GetPluginURL_InstallBundleProtocol(PluginFilename, ProtocolMetadata);
		}
	}

	//Weird flow here because we need to do a few tasks asynchronously
	// Update URL   -->    Call to set destination to Uninstall --> After we get to Uninstall go to Terminate
	// (Called Directly)	   (UninstallTransitionLambda)              (StartTerminateLambda)

	// THIRD:
	//Lambda that will kick off the actual Terminate after we successfully transition to Uninstalled state
	const FGameFeaturePluginTerminateComplete StartTerminateLambda = FGameFeaturePluginTerminateComplete::CreateWeakLambda(this,
		[this, PluginURLForTerminate, CompleteDelegate](const UE::GameFeatures::FResult& Result)
		{
			if (Result.HasValue())
			{
				TerminateGameFeaturePlugin(PluginURLForTerminate, CompleteDelegate);
			}
			//If we failed just bubble error up
			else
			{
				CompleteDelegate.ExecuteIfBound(Result);
			}
		});

	// SECOND:
	//Lambda that will kick off the Uninstall destination after updating our URL if necessary
	const FGameFeaturePluginUpdateURLComplete UninstallTransitionLambda = FGameFeaturePluginUpdateURLComplete::CreateWeakLambda(this,
		[this, PluginURLForTerminate, StartTerminateLambda](const UE::GameFeatures::FResult& Result)
		{
			UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURLForTerminate);
			check(StateMachine);
			ChangeGameFeatureDestination(StateMachine, FGameFeaturePluginStateRange(EGameFeaturePluginState::Uninstalled), StartTerminateLambda);
		});


	// FIRST:
	//If we need to update our URLData, try to do that first before starting the Uninstall. This allows us to update
	//URL Metadata flags that might be important on the way to Terminal if they are changed. EX: FInstallBundlePluginProtocolMetaData::bUninstallBeforeTerminate
	if (ShouldUpdatePluginURLData(PluginURLForTerminate))
	{
		UpdateGameFeaturePluginURL(PluginURLForTerminate, UninstallTransitionLambda);
	}
	else
	{
		UninstallTransitionLambda.Execute(MakeValue());
	}
}

void UGameFeaturesSubsystem::TerminateGameFeaturePlugin(const FString& PluginURL)
{
	TerminateGameFeaturePlugin(PluginURL, FGameFeaturePluginTerminateComplete());
}

void UGameFeaturesSubsystem::TerminateGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginTerminateComplete& CompleteDelegate)
{
	if (UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		//Define a lambda that will kick off the actual Terminate
		const FGameFeaturePluginUpdateURLComplete StartTerminateLambda = FGameFeaturePluginUpdateURLComplete::CreateWeakLambda(this, 
			[this, PluginURL, CompleteDelegate](const UE::GameFeatures::FResult& Result)
			{
				UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL);
				check(StateMachine);
				ChangeGameFeatureDestination(StateMachine, FGameFeaturePluginStateRange(EGameFeaturePluginState::Terminal), CompleteDelegate);
			});

		//If we need to update our URLData, try to do that first before starting the Terminate. This allows us to update
		//URL Metadata flags that might be important on the way to Terminal if they are changed. EX: FInstallBundlePluginProtocolMetaData::bUninstallBeforeTerminate
		if (ShouldUpdatePluginURLData(PluginURL))
		{
			UpdateGameFeaturePluginURL(PluginURL, StartTerminateLambda);
		}
		else
		{
			StartTerminateLambda.Execute(MakeValue());
		}
	}
	else
	{
		CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::BadURL)));
	}
}

void UGameFeaturesSubsystem::CancelGameFeatureStateChange(const FString& PluginURL)
{
	CancelGameFeatureStateChange(PluginURL, FGameFeaturePluginChangeStateComplete());
}

void UGameFeaturesSubsystem::CancelGameFeatureStateChange(const FString& PluginURL, const FGameFeaturePluginChangeStateComplete& CompleteDelegate)
{
	if (UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		const bool bCancelPending = StateMachine->TryCancel(FGameFeatureStateTransitionCanceled::CreateWeakLambda(this, [CompleteDelegate](UGameFeaturePluginStateMachine* Machine)
		{
			CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeValue()));
		}));

		if (!bCancelPending)
		{
			CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeValue()));
		}
	}
	else
	{
		CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::BadURL)));
	}
}

void UGameFeaturesSubsystem::LoadBuiltInGameFeaturePlugin(const TSharedRef<IPlugin>& Plugin, FBuiltInPluginAdditionalFilters AdditionalFilter, const FGameFeaturePluginLoadComplete& CompleteDelegate /*= FGameFeaturePluginLoadComplete()*/)
{
	UE_SCOPED_ENGINE_ACTIVITY(TEXT("Loading GameFeaturePlugin %s"), *Plugin->GetName());

	UAssetManager::Get().PushBulkScanning();

	FString PluginURL;
	FGameFeaturePluginDetails PluginDetails;
	if (GetGameFeaturePluginDetails(Plugin, PluginURL, PluginDetails))
	{
		if (GameSpecificPolicies->IsPluginAllowed(PluginURL))
		{
			FBuiltInGameFeaturePluginBehaviorOptions BehaviorOptions;
			const bool bShouldProcess = AdditionalFilter(Plugin->GetDescriptorFileName(), PluginDetails, BehaviorOptions);
			if (bShouldProcess)
			{
				UGameFeaturePluginStateMachine* StateMachine = FindOrCreateGameFeaturePluginStateMachine(PluginURL);

				const EBuiltInAutoState InitialAutoState = (BehaviorOptions.AutoStateOverride != EBuiltInAutoState::Invalid) ? BehaviorOptions.AutoStateOverride : PluginDetails.BuiltInAutoState;
				
				const EGameFeaturePluginState DestinationState = ConvertInitialFeatureStateToTargetState(InitialAutoState);

				// If we're already at the destination or beyond, don't transition back
				FGameFeaturePluginStateRange Destination(DestinationState, EGameFeaturePluginState::Active);
				ChangeGameFeatureDestination(StateMachine, Destination, 
					FGameFeaturePluginChangeStateComplete::CreateWeakLambda(this, [this, StateMachine, Destination, CompleteDelegate](const UE::GameFeatures::FResult& Result)
					{
						LoadBuiltInGameFeaturePluginComplete(Result, StateMachine, Destination);
						CompleteDelegate.ExecuteIfBound(Result);
					}));
			}
			else
			{
				CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::PluginFiltered)));
			}
		}
		else
		{
			CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::PluginNotAllowed)));
		}
	}
	else
	{
		CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::PluginDetailsNotFound)));
	}

	UAssetManager::Get().PopBulkScanning();
}

#if UE_BUILD_SHIPPING
class FBuiltInPluginLoadTimeTracker {};
class FBuiltInPluginLoadTimeTrackerScope
{
public:
	FORCEINLINE FBuiltInPluginLoadTimeTrackerScope(FBuiltInPluginLoadTimeTracker& InTracker, const TSharedRef<IPlugin>& Plugin) {};
};
#else // !UE_BUILD_SHIPPING
class FBuiltInPluginLoadTimeTracker
{
public:
	~FBuiltInPluginLoadTimeTracker()
	{
		if (PluginLoadTimes.Num() > 0)
		{
			UE_LOG(LogGameFeatures, Display, TEXT("There were %d built in plugins that took longer than %.4fs to load. Listing worst offenders."), PluginLoadTimes.Num(), UE::GameFeatures::GBuiltInPluginLoadTimeReportThreshold);
			const int32 NumToReport = FMath::Min(PluginLoadTimes.Num(), UE::GameFeatures::GBuiltInPluginLoadTimeMaxReportCount);
			Algo::Sort(PluginLoadTimes, [](const TPair<FString, double>& A, TPair<FString, double>& B) { return A.Value > B.Value; });
			for (int32 PluginIdx = 0; PluginIdx < NumToReport; ++PluginIdx)
			{
				const TPair<FString, double>& Plugin = PluginLoadTimes[PluginIdx];
				double LoadTime = Plugin.Value;
				if (LoadTime >= UE::GameFeatures::GBuiltInPluginLoadTimeErrorThreshold)
				{
					UE_LOG(LogGameFeatures, Warning, TEXT("%s took %.4f seconds to load. Something was done to significantly increase the load time of this plugin and it is now well outside what is acceptable. Reduce the load time to much less than %.4f seconds. Ideally, reduce the load time to less than %.4f seconds."),
						*Plugin.Key, Plugin.Value, UE::GameFeatures::GBuiltInPluginLoadTimeErrorThreshold, UE::GameFeatures::GBuiltInPluginLoadTimeReportThreshold);
				}
				else
				{
					UE_LOG(LogGameFeatures, Display, TEXT("  %.4fs\t%s"), Plugin.Value, *Plugin.Key);
				}
			}
		}
	}

	void ReportPlugin(const FString& PluginName, double LoadTime)
	{
		PluginLoadTimes.Emplace(PluginName, LoadTime);
	}

private:
	TArray<TPair<FString, double>> PluginLoadTimes;
	
};

class FBuiltInPluginLoadTimeTrackerScope
{
public:
	FBuiltInPluginLoadTimeTrackerScope(FBuiltInPluginLoadTimeTracker& InTracker, const TSharedRef<IPlugin>& Plugin)
		: Tracker(InTracker), PluginName(Plugin->GetName()), StartTime(FPlatformTime::Seconds())
	{}

	~FBuiltInPluginLoadTimeTrackerScope()
	{
		double LoadTime = FPlatformTime::Seconds() - StartTime;
		if (LoadTime >= UE::GameFeatures::GBuiltInPluginLoadTimeReportThreshold)
		{
			Tracker.ReportPlugin(PluginName, LoadTime);
		}
	}

private:
	FBuiltInPluginLoadTimeTracker& Tracker;
	FString PluginName;
	double StartTime;
};

#endif // !UE_BUILD_SHIPPING

// @TODO: Need to make sure all code paths wait for this properly
// For example, after login, UFortGlobalUIContext::SetSubGame can call ChangeBundleStateForPrimaryAssets which may cancel pending loads
void UGameFeaturesSubsystem::LoadBuiltInGameFeaturePlugins(FBuiltInPluginAdditionalFilters AdditionalFilter, const FBuiltInGameFeaturePluginsLoaded& InCompleteDelegate /*= FBuiltInGameFeaturePluginsLoaded()*/)
{
	struct FLoadContext
	{
		FScopeLogTime ScopeLogTime{TEXT("BuiltInGameFeaturePlugins loaded."), nullptr, FConditionalScopeLogTime::ScopeLog_Seconds};

		TMap<FString, UE::GameFeatures::FResult> Results;
		FBuiltInGameFeaturePluginsLoaded CompleteDelegate;

		int32 NumPluginsLoaded = 0;

		~FLoadContext()
		{
			UGameplayTagsManager::Get().PopDeferOnGameplayTagTreeChangedBroadcast();
			UAssetManager::Get().PopBulkScanning();

			CompleteDelegate.ExecuteIfBound(Results);
		}
	};
	TSharedRef LoadContext = MakeShared<FLoadContext>();
	LoadContext->CompleteDelegate = InCompleteDelegate;

	UAssetManager::Get().PushBulkScanning();
	UGameplayTagsManager::Get().PushDeferOnGameplayTagTreeChangedBroadcast();

	FBuiltInPluginLoadTimeTracker PluginLoadTimeTracker;
	TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPlugins();

	LoadContext->Results.Reserve(EnabledPlugins.Num());
	for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins)
	{
		LoadContext->Results.Add(Plugin->GetName(), MakeError("Pending"));
	}

	const int32 NumPluginsToLoad = EnabledPlugins.Num();
	UE_LOG(LogGameFeatures, Log, TEXT("Loading %i builtins"), NumPluginsToLoad);

	for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins)
	{
		FBuiltInPluginLoadTimeTrackerScope TrackerScope(PluginLoadTimeTracker, Plugin);
		LoadBuiltInGameFeaturePlugin(Plugin, AdditionalFilter, FGameFeaturePluginLoadComplete::CreateLambda([LoadContext, Plugin, NumPluginsToLoad](const UE::GameFeatures::FResult& Result)
		{
			LoadContext->Results.Add(Plugin->GetName(), Result);
			++LoadContext->NumPluginsLoaded;
			UE_LOG(LogGameFeatures, VeryVerbose, TEXT("Finished Loading %i builtins"), LoadContext->NumPluginsLoaded);
		}));
	}
}

bool UGameFeaturesSubsystem::GetPluginURLByName(const FString& PluginName, FString& OutPluginURL) const
{
	if (const FString* PluginURL = GameFeaturePluginNameToPathMap.Find(PluginName))
	{
		OutPluginURL = *PluginURL;
		return true;
	}

	return false;
}

bool UGameFeaturesSubsystem::GetPluginURLForBuiltInPluginByName(const FString& PluginName, FString& OutPluginURL) const
{
	return GetPluginURLByName(PluginName, OutPluginURL);
}

FString UGameFeaturesSubsystem::GetPluginFilenameFromPluginURL(const FString& PluginURL) const
{
	FString PluginFilename;
	const UGameFeaturePluginStateMachine* GFSM = FindGameFeaturePluginStateMachine(PluginURL);
	if (GFSM == nullptr || !GFSM->GetPluginFilename(PluginFilename))
	{
		UE_LOG(LogGameFeatures, Error, TEXT("UGameFeaturesSubsystem could not get the plugin path from the plugin URL. URL:%s "), *PluginURL);
	}
	return PluginFilename;
}

void UGameFeaturesSubsystem::FixPluginPackagePath(FString& PathToFix, const FString& PluginRootPath, bool bMakeRelativeToPluginRoot)
{
	if (bMakeRelativeToPluginRoot)
	{
		// This only modifies paths starting with the root
		PathToFix.RemoveFromStart(PluginRootPath);
	}
	else
	{
		if (!FPackageName::IsValidLongPackageName(PathToFix))
		{
			PathToFix = PluginRootPath / PathToFix;
		}
	}
}

void UGameFeaturesSubsystem::GetLoadedGameFeaturePluginFilenamesForCooking(TArray<FString>& OutLoadedPluginFilenames) const
{
	for (auto StateMachineIt = GameFeaturePluginStateMachines.CreateConstIterator(); StateMachineIt; ++StateMachineIt)
	{
		UGameFeaturePluginStateMachine* GFSM = StateMachineIt.Value();
		if (GFSM && GFSM->GetCurrentState() > EGameFeaturePluginState::Installed)
		{
			FString PluginFilename;
			if (GFSM->GetPluginFilename(PluginFilename))
			{
				OutLoadedPluginFilenames.Add(PluginFilename);
			}
		}
	}
}

EGameFeaturePluginState UGameFeaturesSubsystem::GetPluginState(const FString& PluginURL) const
{
	FGameFeaturePluginIdentifier PluginIdentifier(PluginURL);
	return GetPluginState(PluginIdentifier);
}

EGameFeaturePluginState UGameFeaturesSubsystem::GetPluginState(FGameFeaturePluginIdentifier PluginIdentifier) const
{
	if (UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginIdentifier))
	{
		return StateMachine->GetCurrentState();
	}
	else
	{
		return EGameFeaturePluginState::UnknownStatus;
	}
}

bool UGameFeaturesSubsystem::GetGameFeaturePluginDetails(const TSharedRef<IPlugin>& Plugin, FString& OutPluginURL, FGameFeaturePluginDetails& OutPluginDetails) const
{
	// @TODO: this problematic because it assumes file protocol.
	// Ideally this would work with any protocol, but for current uses cases the exact protocol doesn't seem to matter.

	const FString& PluginDescriptorFilename = Plugin->GetDescriptorFileName();

	// Make sure you are in a game feature plugins folder. All GameFeaturePlugins are rooted in a GameFeatures folder.
	if (!PluginDescriptorFilename.IsEmpty() && GetDefault<UGameFeaturesSubsystemSettings>()->IsValidGameFeaturePlugin(FPaths::ConvertRelativePathToFull(PluginDescriptorFilename)) && FPaths::FileExists(PluginDescriptorFilename))
	{
		bool bIsFileProtocol = true;
		if (GetPluginURLByName(Plugin->GetName(), OutPluginURL))
		{
			bIsFileProtocol = UGameFeaturesSubsystem::IsPluginURLProtocol(OutPluginURL, EGameFeaturePluginProtocol::File);
		}
		else
		{
			OutPluginURL = GetPluginURL_FileProtocol(PluginDescriptorFilename);
		}

		if (bIsFileProtocol)
		{
			return GetGameFeaturePluginDetails(OutPluginURL, PluginDescriptorFilename, OutPluginDetails);
		}
	}

	return false;
}

bool UGameFeaturesSubsystem::GetGameFeaturePluginDetails(const FString& PluginURL, const FString& PluginDescriptorFilename, FGameFeaturePluginDetails& OutPluginDetails) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GFP_GetPluginDetails);

	// GFPs are implemented with a plugin so FPluginReferenceDescriptor doesn't know anything about them.
	// Need a better way of storing GFP specific plugin data...

	FDateTime FileTimeStamp;
	if (UE::GameFeatures::GCachePluginDetails)
	{
		// Note: On file systems that don't support timestamps (current time is returned instead), 
		// the pak file layer will end up caching the mount time, so this stamp will still be valid as long as the uplugin
		// is in a pak and the pak is mounted.
		FileTimeStamp = IFileManager::Get().GetTimeStamp(*PluginDescriptorFilename);
		if (FCachedGameFeaturePluginDetails* ExistingDetails = CachedPluginDetailsByFilename.Find(PluginDescriptorFilename))
		{
			if (ExistingDetails->TimeStamp == FileTimeStamp)
			{
				OutPluginDetails = ExistingDetails->Details;
				return true;
			}
			else
			{
				CachedPluginDetailsByFilename.Remove(PluginDescriptorFilename);
			}
		}
	}

	TSharedPtr<FJsonObject> ObjectPtr;
	{
#if WITH_EDITOR
		// In the editor we already have the plugin JSON cached
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(FPathViews::GetBaseFilename(PluginDescriptorFilename));
		if (Plugin)
		{
			ObjectPtr = Plugin->GetDescriptorJson();
		}
		else
#endif // WITH_EDITOR
		{
			// Read the file to a string
			FString FileContents;
			if (!FFileHelper::LoadFileToString(FileContents, *PluginDescriptorFilename))
			{
				UE_LOG(LogGameFeatures, Error, TEXT("UGameFeaturesSubsystem could not determine if feature was hotfixable. Failed to read file. File:%s Error:%d"), *PluginDescriptorFilename, FPlatformMisc::GetLastError());
				return false;
			}

			// Deserialize a JSON object from the string	
			TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(FileContents);
			if (!FJsonSerializer::Deserialize(Reader, ObjectPtr) || !ObjectPtr.IsValid())
			{
				UE_LOG(LogGameFeatures, Error, TEXT("UGameFeaturesSubsystem could not determine if feature was hotfixable. Json invalid. File:%s. Error:%s"), *PluginDescriptorFilename, *Reader->GetErrorMessage());
				return false;
			}
		}
	}

	// Read the properties

	// Hotfixable. If it is not specified, then we assume it is
	OutPluginDetails.bHotfixable = true;
	ObjectPtr->TryGetBoolField(TEXT("Hotfixable"), OutPluginDetails.bHotfixable);

	// Determine the initial plugin state
	OutPluginDetails.BuiltInAutoState = DetermineBuiltInInitialFeatureState(ObjectPtr, PluginDescriptorFilename);

	// Read any additional metadata the policy might want to consume (e.g., a release version number)
	for (const FString& ExtraKey : GetDefault<UGameFeaturesSubsystemSettings>()->AdditionalPluginMetadataKeys)
	{
		FString ExtraValue;
		ObjectPtr->TryGetStringField(ExtraKey, ExtraValue);
		OutPluginDetails.AdditionalMetadata.Add(ExtraKey, ExtraValue);
	}

	// Parse plugin dependencies
	const TArray<TSharedPtr<FJsonValue>>* PluginsArray = nullptr;
	ObjectPtr->TryGetArrayField(TEXT("Plugins"), PluginsArray);
	if (PluginsArray)
	{
		FString NameField = TEXT("Name");
		FString EnabledField = TEXT("Enabled");
		for (const TSharedPtr<FJsonValue>& PluginElement : *PluginsArray)
		{
			if (PluginElement.IsValid())
			{
				const TSharedPtr<FJsonObject>* ElementObjectPtr = nullptr;
				PluginElement->TryGetObject(ElementObjectPtr);
				if (ElementObjectPtr && ElementObjectPtr->IsValid())
				{
					const TSharedPtr<FJsonObject>& ElementObject = *ElementObjectPtr;
					bool bElementEnabled = false;
					ElementObject->TryGetBoolField(EnabledField, bElementEnabled);

					if (bElementEnabled)
					{
						FString DependencyName;
						ElementObject->TryGetStringField(NameField, DependencyName);
						if (!DependencyName.IsEmpty())
						{
							TValueOrError<FString, FString> ResolvedDepResult = GameSpecificPolicies->ReslovePluginDependency(PluginURL, DependencyName);
							if (ResolvedDepResult.HasError())
							{
								UE_LOG(LogGameFeatures, Error, TEXT("Game feature plugin '%s' has unknown dependency '%s' [%s]."), *PluginDescriptorFilename, *DependencyName, *ResolvedDepResult.GetError());
							}
							else if (ResolvedDepResult.HasValue() && !ResolvedDepResult.GetValue().IsEmpty()) // Dependency may not be a GFP
							{
								OutPluginDetails.PluginDependencies.Add(ResolvedDepResult.StealValue());
							}
						}
					}
				}
			}
		}
	}

	if (UE::GameFeatures::GCachePluginDetails)
	{
		CachedPluginDetailsByFilename.Add(PluginDescriptorFilename, FCachedGameFeaturePluginDetails(OutPluginDetails, FileTimeStamp));
	}
	return true;
}

void UGameFeaturesSubsystem::PruneCachedGameFeaturePluginDetails(const FString& PluginURL, const FString& PluginDescriptorFilename) const
{
	CachedPluginDetailsByFilename.Remove(PluginDescriptorFilename);
}

UGameFeaturePluginStateMachine* UGameFeaturesSubsystem::FindGameFeaturePluginStateMachineByPluginName(const FString& PluginName) const
{
	for (auto StateMachineIt = GameFeaturePluginStateMachines.CreateConstIterator(); StateMachineIt; ++StateMachineIt)
	{
		if (UGameFeaturePluginStateMachine* GFSM = StateMachineIt.Value())
		{
			if (GFSM->GetGameFeatureName() == PluginName)
			{
				return GFSM;
			}
		}
	}

	return nullptr;
}

UGameFeaturePluginStateMachine* UGameFeaturesSubsystem::FindGameFeaturePluginStateMachine(const FString& PluginURL) const
{
	FGameFeaturePluginIdentifier FindPluginIdentifier(PluginURL);
	return FindGameFeaturePluginStateMachine(FindPluginIdentifier);
}

UGameFeaturePluginStateMachine* UGameFeaturesSubsystem::FindGameFeaturePluginStateMachine(const FGameFeaturePluginIdentifier& PluginIdentifier) const
{
	TObjectPtr<UGameFeaturePluginStateMachine> const* ExistingStateMachine = 
		GameFeaturePluginStateMachines.FindByHash(GetTypeHash(PluginIdentifier.GetIdentifyingString()), PluginIdentifier.GetIdentifyingString());
	if (ExistingStateMachine)
	{
		UE_LOG(LogGameFeatures, VeryVerbose, TEXT("FOUND GameFeaturePlugin using PluginIdentifier:%.*s for PluginURL:%s"), PluginIdentifier.GetIdentifyingString().Len(), PluginIdentifier.GetIdentifyingString().GetData(), *PluginIdentifier.GetFullPluginURL());
		return *ExistingStateMachine;
	}
	UE_LOG(LogGameFeatures, VeryVerbose, TEXT("NOT FOUND GameFeaturePlugin using PluginIdentifier:%.*s for PluginURL:%s"), PluginIdentifier.GetIdentifyingString().Len(), PluginIdentifier.GetIdentifyingString().GetData(), *PluginIdentifier.GetFullPluginURL());

	return nullptr;
}

UGameFeaturePluginStateMachine* UGameFeaturesSubsystem::FindOrCreateGameFeaturePluginStateMachine(const FString& PluginURL)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GFP_FindOrCreateStateMachine);
	FGameFeaturePluginIdentifier PluginIdentifier(PluginURL);
	if (UGameFeaturePluginStateMachine* ExistingStateMachine = FindGameFeaturePluginStateMachine(PluginIdentifier))
	{
		UE_LOG(LogGameFeatures, VeryVerbose, TEXT("Found GameFeaturePlugin StateMachine using Identifier:%.*s from PluginURL:%s"), PluginIdentifier.GetIdentifyingString().Len(), PluginIdentifier.GetIdentifyingString().GetData(), *PluginURL);
		return ExistingStateMachine;
	}

	UE_LOG(LogGameFeatures, Display, TEXT("Creating GameFeaturePlugin StateMachine using Identifier:%.*s from PluginURL:%s"), PluginIdentifier.GetIdentifyingString().Len(), PluginIdentifier.GetIdentifyingString().GetData(), *PluginURL);

	UGameFeaturePluginStateMachine* NewStateMachine = NewObject<UGameFeaturePluginStateMachine>(this);
	GameFeaturePluginStateMachines.Add(FString(PluginIdentifier.GetIdentifyingString()), NewStateMachine);
	NewStateMachine->InitStateMachine(MoveTemp(PluginIdentifier));

	return NewStateMachine;
}

void UGameFeaturesSubsystem::LoadBuiltInGameFeaturePluginComplete(const UE::GameFeatures::FResult& Result, UGameFeaturePluginStateMachine* Machine, FGameFeaturePluginStateRange RequestedDestination)
{
	check(Machine);
	if (Result.HasValue())
	{
		//@note It's possible for the machine to still be tranitioning at this point as long as it's withing the requested destination range
		UE_LOG(LogGameFeatures, Display, TEXT("Game feature '%s' loaded successfully. Ending state: %s, [%s, %s]"), 
			*Machine->GetGameFeatureName(), 
			*UE::GameFeatures::ToString(Machine->GetCurrentState()),
			*UE::GameFeatures::ToString(Machine->GetDestination().MinState),
			*UE::GameFeatures::ToString(Machine->GetDestination().MaxState));

		checkf(RequestedDestination.Contains(Machine->GetCurrentState()), TEXT("Game feature '%s': Ending state %s is not in expected range [%s, %s]"), 
			*Machine->GetGameFeatureName(), 
			*UE::GameFeatures::ToString(Machine->GetCurrentState()), 
			*UE::GameFeatures::ToString(RequestedDestination.MinState), 
			*UE::GameFeatures::ToString(RequestedDestination.MaxState));
	}
	else
	{
		const FString ErrorMessage = UE::GameFeatures::ToString(Result);
		UE_LOG(LogGameFeatures, Error, TEXT("Game feature '%s' load failed. Ending state: %s, [%s, %s]. Result: %s"),
			*Machine->GetGameFeatureName(),
			*UE::GameFeatures::ToString(Machine->GetCurrentState()),
			*UE::GameFeatures::ToString(Machine->GetDestination().MinState),
			*UE::GameFeatures::ToString(Machine->GetDestination().MaxState),
			*ErrorMessage);
	}
}

void UGameFeaturesSubsystem::ChangeGameFeatureDestination(UGameFeaturePluginStateMachine* Machine, const FGameFeaturePluginStateRange& StateRange, FGameFeaturePluginChangeStateComplete CompleteDelegate)
{
	const bool bSetDestination = Machine->SetDestination(StateRange,
		FGameFeatureStateTransitionComplete::CreateUObject(this, &ThisClass::ChangeGameFeatureTargetStateComplete, CompleteDelegate));

	if (bSetDestination)
	{
		UE_LOG(LogGameFeatures, Verbose, TEXT("ChangeGameFeatureDestination: Set Game Feature %s Destination State to [%s, %s]"), *Machine->GetGameFeatureName(), *UE::GameFeatures::ToString(StateRange.MinState), *UE::GameFeatures::ToString(StateRange.MaxState));
	}
	else
	{
		FGameFeaturePluginStateRange CurrDesitination = Machine->GetDestination();
		UE_LOG(LogGameFeatures, Display, TEXT("ChangeGameFeatureDestination: Attempting to cancel transition for Game Feature %s. Desired [%s, %s]. Current [%s, %s]"), 
			*Machine->GetGameFeatureName(), 
			*UE::GameFeatures::ToString(StateRange.MinState), *UE::GameFeatures::ToString(StateRange.MaxState),
			*UE::GameFeatures::ToString(CurrDesitination.MinState), *UE::GameFeatures::ToString(CurrDesitination.MaxState));

		// Try canceling any current transition, then retry
		auto OnCanceled = [this, StateRange, CompleteDelegate](UGameFeaturePluginStateMachine* Machine) mutable
		{
			// Special case for terminal state since it cannot be exited, we need to make a new machine
			if (Machine->GetCurrentState() == EGameFeaturePluginState::Terminal)
			{
				UGameFeaturePluginStateMachine* NewMachine = FindOrCreateGameFeaturePluginStateMachine(Machine->GetPluginURL());
				checkf(NewMachine != Machine, TEXT("Game Feature Plugin %s should have already been removed from subsystem!"), *Machine->GetPluginURL());
				Machine = NewMachine;
			}

			// Now that the transition has been canceled, retry reaching the desired destination
			const bool bSetDestination = Machine->SetDestination(StateRange,
				FGameFeatureStateTransitionComplete::CreateUObject(this, &ThisClass::ChangeGameFeatureTargetStateComplete, CompleteDelegate));

			if (!ensure(bSetDestination))
			{
				UE_LOG(LogGameFeatures, Warning, TEXT("ChangeGameFeatureDestination: Failed to set Game Feature %s Destination State to [%s, %s]"), *Machine->GetGameFeatureName(), *UE::GameFeatures::ToString(StateRange.MinState), *UE::GameFeatures::ToString(StateRange.MaxState));

				CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::UnreachableState)));
			}
			else
			{
				UE_LOG(LogGameFeatures, Display, TEXT("ChangeGameFeatureDestination: OnCanceled, set Game Feature %s Destination State to [%s, %s]"), *Machine->GetGameFeatureName(), *UE::GameFeatures::ToString(StateRange.MinState), *UE::GameFeatures::ToString(StateRange.MaxState));
			}
		};

		const bool bCancelPending = Machine->TryCancel(FGameFeatureStateTransitionCanceled::CreateWeakLambda(this, MoveTemp(OnCanceled)));
		if (!ensure(bCancelPending))
		{
			UE_LOG(LogGameFeatures, Warning, TEXT("ChangeGameFeatureDestination: Failed to cancel Game Feature %s"), *Machine->GetGameFeatureName());

			CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::UnreachableState + UE::GameFeatures::CommonErrorCodes::CancelAddonCode)));
		}
	}
}

void UGameFeaturesSubsystem::ChangeGameFeatureTargetStateComplete(UGameFeaturePluginStateMachine* Machine, const UE::GameFeatures::FResult& Result, FGameFeaturePluginChangeStateComplete CompleteDelegate)
{
	CompleteDelegate.ExecuteIfBound(Result);
}

void UGameFeaturesSubsystem::BeginTermination(UGameFeaturePluginStateMachine* Machine)
{
	check(IsValid(Machine));
	check(Machine->GetCurrentState() == EGameFeaturePluginState::Terminal);

	FStringView Identifer = Machine->GetPluginIdentifier().GetIdentifyingString();

	UE_LOG(LogGameFeatures, Verbose, TEXT("BeginTermination of GameFeaturePlugin. Identifier:%.*s URL:%s"), Identifer.Len(), Identifer.GetData(), *(Machine->GetPluginURL()));
	GameFeaturePluginStateMachines.RemoveByHash(GetTypeHash(Identifer), Identifer);
	TerminalGameFeaturePluginStateMachines.Add(Machine);
}

void UGameFeaturesSubsystem::FinishTermination(UGameFeaturePluginStateMachine* Machine)
{
	UE_LOG(LogGameFeatures, Display, TEXT("FinishTermination of GameFeaturePlugin. Identifier:%.*s URL:%s"), Machine->GetPluginIdentifier().GetIdentifyingString().Len(), Machine->GetPluginIdentifier().GetIdentifyingString().GetData(), *(Machine->GetPluginURL()));
	TerminalGameFeaturePluginStateMachines.RemoveSwap(Machine);
}

bool UGameFeaturesSubsystem::FindOrCreatePluginDependencyStateMachines(const FString& PluginURL, const FString& PluginFilename, TArray<UGameFeaturePluginStateMachine*>& OutDependencyMachines)
{
	FGameFeaturePluginDetails Details;
	if (GetGameFeaturePluginDetails(PluginURL, PluginFilename, Details))
	{
		for (const FString& DependencyURL : Details.PluginDependencies)
		{
			UGameFeaturePluginStateMachine* Dependency = FindOrCreateGameFeaturePluginStateMachine(DependencyURL);
			check(Dependency);
			OutDependencyMachines.Add(Dependency);
		}

		return true;
	}

	return false;
}

void UGameFeaturesSubsystem::ListGameFeaturePlugins(const TArray<FString>& Args, UWorld* InWorld, FOutputDevice& Ar)
{
	const bool bAlphaSort = Args.ContainsByPredicate([](const FString& Arg) { return Arg.Compare(TEXT("-ALPHASORT"), ESearchCase::IgnoreCase) == 0; });
	const bool bActiveOnly = Args.ContainsByPredicate([](const FString& Arg) { return Arg.Compare(TEXT("-ACTIVEONLY"), ESearchCase::IgnoreCase) == 0; });
	const bool bCsv = Args.ContainsByPredicate([](const FString& Arg) { return Arg.Compare(TEXT("-CSV"), ESearchCase::IgnoreCase) == 0; });

	FString PlatformName = FPlatformMisc::GetCPUBrand().TrimStartAndEnd();
	Ar.Logf(TEXT("Listing Game Feature Plugins...(%s)"), *PlatformName);
	if (bCsv)
	{
		Ar.Logf(TEXT(",Plugin,State"));
	}

	// create a copy for sorting
	TArray<typename decltype(GameFeaturePluginStateMachines)::ValueType> StateMachines;
	GameFeaturePluginStateMachines.GenerateValueArray(StateMachines);

	if (bAlphaSort)
	{
		StateMachines.Sort([](const UGameFeaturePluginStateMachine& A, const UGameFeaturePluginStateMachine& B) { return A.GetGameFeatureName().Compare(B.GetGameFeatureName()) < 0; });
	}

	int32 PluginCount = 0;
	for (UGameFeaturePluginStateMachine* GFSM : StateMachines)
	{
		if (!GFSM)
		{
			continue;
		}

		if (bActiveOnly && GFSM->GetCurrentState() != EGameFeaturePluginState::Active)
		{
			continue;
		}

		if (bCsv)
		{
			Ar.Logf(TEXT(",%s,%s"), *GFSM->GetGameFeatureName(), *UE::GameFeatures::ToString(GFSM->GetCurrentState()));
		}
		else
		{
			Ar.Logf(TEXT("%s (%s)"), *GFSM->GetGameFeatureName(), *UE::GameFeatures::ToString(GFSM->GetCurrentState()));
		}
		++PluginCount;
	}

	Ar.Logf(TEXT("Total Game Feature Plugins: %d"), PluginCount);
}

void UGameFeaturesSubsystem::CallbackObservers(EObserverCallback CallbackType, const FString& PluginURL, 
	const FString* PluginName /*= nullptr*/, 
	const UGameFeatureData* GameFeatureData /*= nullptr*/, 
	FGameFeatureStateChangeContext* StateChangeContext /*= nullptr*/)
{
	static_assert(std::underlying_type<EObserverCallback>::type(EObserverCallback::Count) == 8, "Update UGameFeaturesSubsystem::CallbackObservers to handle added EObserverCallback");

	// Protect against modifying the observer list during iteration
	TArray<UObject*> LocalObservers(Observers);

	switch (CallbackType)
	{
	case EObserverCallback::CheckingStatus:
	{
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureCheckingStatus(PluginURL);
		}
		break;
	}
	case EObserverCallback::Terminating:
	{
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureTerminating(PluginURL);
		}
		break;
	}
	case EObserverCallback::Registering:
	{
		check(PluginName);
		check(GameFeatureData);
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureRegistering(GameFeatureData, *PluginName, PluginURL);
		}
		break;
	}
	case EObserverCallback::Unregistering:
	{
		check(PluginName);
		check(GameFeatureData);
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureUnregistering(GameFeatureData, *PluginName, PluginURL);
		}
		break;
	}
	case EObserverCallback::Loading:
	{
		check(GameFeatureData);
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureLoading(GameFeatureData, PluginURL);
		}
		break;
	}
	case EObserverCallback::Activating:
	{
		check(GameFeatureData);
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureActivating(GameFeatureData, PluginURL);
		}
		break;
	}
	case EObserverCallback::Deactivating:
	{
		check(GameFeatureData);
		check(StateChangeContext);
		FGameFeatureDeactivatingContext* DeactivatingContext = static_cast<FGameFeatureDeactivatingContext*>(StateChangeContext);
		if (ensureAlwaysMsgf(DeactivatingContext, TEXT("Invalid StateChangeContext supplied! Could not cast to FGameFeaturePauseStateChangeContext*!")))
		{
			for (UObject* Observer : LocalObservers)
			{
				CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureDeactivating(GameFeatureData, *DeactivatingContext, PluginURL);
			}
		}
		break;
	}
	case EObserverCallback::PauseChanged:
	{
		check(PluginName);
		check(StateChangeContext);
		FGameFeaturePauseStateChangeContext* PauseChangeContext = static_cast<FGameFeaturePauseStateChangeContext*>(StateChangeContext);
		if (ensureAlwaysMsgf(PauseChangeContext, TEXT("Invalid StateChangeContext supplied! Could not cast to FGameFeaturePauseStateChangeContext*!")))
		{
			for (UObject* Observer : LocalObservers)
			{
				CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeaturePauseChange(PluginURL, *PluginName, *PauseChangeContext);
			}
		}
		break;
	}
	default:
		UE_LOG(LogGameFeatures, Fatal, TEXT("Unkown EObserverCallback!"));
	}
}

TSet<FString> UGameFeaturesSubsystem::GetActivePluginNames() const
{
	TSet<FString> ActivePluginNames;

	for (const TPair<FString, TObjectPtr<UGameFeaturePluginStateMachine>>& Pair : GameFeaturePluginStateMachines)
	{
		UGameFeaturePluginStateMachine* StateMachine = Pair.Value;
		if (StateMachine->GetCurrentState() == EGameFeaturePluginState::Active &&
			StateMachine->GetDestination().Contains(EGameFeaturePluginState::Active))
		{
			ActivePluginNames.Add(StateMachine->GetPluginName());
		}
	}

	return ActivePluginNames;
}

namespace GameFeaturesSubsystem
{ 
	bool IsContentWithinActivePlugin(const FString& InObjectOrPackagePath, const TSet<FString>& ActivePluginNames)
	{
		// Look for the first slash beyond the first one we start with.
		const int32 RootEndIndex = InObjectOrPackagePath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 1);

		const FString ObjectPathRootName = InObjectOrPackagePath.Mid(1, RootEndIndex - 1);

		if (ActivePluginNames.Contains(ObjectPathRootName))
		{
			return true;
		}

		return false;
	}
}

void UGameFeaturesSubsystem::FilterInactivePluginAssets(TArray<FAssetIdentifier>& AssetsToFilter) const
{
	AssetsToFilter.RemoveAllSwap([ActivePluginNames = GetActivePluginNames()](const FAssetIdentifier& Asset) 
	{
		return !GameFeaturesSubsystem::IsContentWithinActivePlugin(Asset.PackageName.ToString(), ActivePluginNames);
	});
}

void UGameFeaturesSubsystem::FilterInactivePluginAssets(TArray<FAssetData>& AssetsToFilter) const
{
	AssetsToFilter.RemoveAllSwap([ActivePluginNames = GetActivePluginNames()](const FAssetData& Asset) 
	{
		return !GameFeaturesSubsystem::IsContentWithinActivePlugin(Asset.GetObjectPathString(), ActivePluginNames);
	});
}
EBuiltInAutoState UGameFeaturesSubsystem::DetermineBuiltInInitialFeatureState(TSharedPtr<FJsonObject> Descriptor, const FString& ErrorContext)
{
	EBuiltInAutoState InitialState = EBuiltInAutoState::Invalid;

	FString InitialFeatureStateStr;
	if (Descriptor->TryGetStringField(TEXT("BuiltInInitialFeatureState"), InitialFeatureStateStr))
	{
		if (InitialFeatureStateStr == TEXT("Installed"))
		{
			InitialState = EBuiltInAutoState::Installed;
		}
		else if (InitialFeatureStateStr == TEXT("Registered"))
		{
			InitialState = EBuiltInAutoState::Registered;
		}
		else if (InitialFeatureStateStr == TEXT("Loaded"))
		{
			InitialState = EBuiltInAutoState::Loaded;
		}
		else if (InitialFeatureStateStr == TEXT("Active"))
		{
			InitialState = EBuiltInAutoState::Active;
		}
		else
		{
			if (!ErrorContext.IsEmpty())
			{
				UE_LOG(LogGameFeatures, Error, TEXT("Game feature '%s' has an unknown value '%s' for BuiltInInitialFeatureState (expected Installed, Registered, Loaded, or Active); defaulting to Active."), *ErrorContext, *InitialFeatureStateStr);
			}
			InitialState = EBuiltInAutoState::Active;
		}
	}
	else
	{
		// BuiltInAutoRegister. Default to true. If this is a built in plugin, should it be registered automatically (set to false if you intent to load late with LoadAndActivateGameFeaturePlugin)
		bool bBuiltInAutoRegister = true;
		Descriptor->TryGetBoolField(TEXT("BuiltInAutoRegister"), bBuiltInAutoRegister);

		// BuiltInAutoLoad. Default to true. If this is a built in plugin, should it be loaded automatically (set to false if you intent to load late with LoadAndActivateGameFeaturePlugin)
		bool bBuiltInAutoLoad = true;
		Descriptor->TryGetBoolField(TEXT("BuiltInAutoLoad"), bBuiltInAutoLoad);

		// The cooker will need to activate the plugin so that assets can be scanned properly
		bool bBuiltInAutoActivate = true;
		Descriptor->TryGetBoolField(TEXT("BuiltInAutoActivate"), bBuiltInAutoActivate);

		InitialState = EBuiltInAutoState::Installed;
		if (bBuiltInAutoRegister)
		{
			InitialState = EBuiltInAutoState::Registered;
			if (bBuiltInAutoLoad)
			{
				InitialState = EBuiltInAutoState::Loaded;
				if (bBuiltInAutoActivate)
				{
					InitialState = EBuiltInAutoState::Active;
				}
			}
		}

		if (!ErrorContext.IsEmpty())
		{
			//@TODO: Increase severity to a warning after changing existing features
			UE_LOG(LogGameFeatures, Log, TEXT("Game feature '%s' has no BuiltInInitialFeatureState key, using legacy BuiltInAutoRegister(%d)/BuiltInAutoLoad(%d)/BuiltInAutoActivate(%d) values to arrive at initial state."),
				*ErrorContext,
				bBuiltInAutoRegister ? 1 : 0,
				bBuiltInAutoLoad ? 1 : 0,
				bBuiltInAutoActivate ? 1 : 0);
		}
	}

	return InitialState;
}

EGameFeaturePluginState UGameFeaturesSubsystem::ConvertInitialFeatureStateToTargetState(EBuiltInAutoState AutoState)
{
	EGameFeaturePluginState InitialState;
	switch (AutoState)
	{
	default:
	case EBuiltInAutoState::Invalid:
		InitialState = EGameFeaturePluginState::UnknownStatus;
		break;
	case EBuiltInAutoState::Installed:
		InitialState = EGameFeaturePluginState::Installed;
		break;
	case EBuiltInAutoState::Registered:
		InitialState = EGameFeaturePluginState::Registered;
		break;
	case EBuiltInAutoState::Loaded:
		InitialState = EGameFeaturePluginState::Loaded;
		break;
	case EBuiltInAutoState::Active:
		InitialState = EGameFeaturePluginState::Active;
		break;
	}
	return InitialState;
}


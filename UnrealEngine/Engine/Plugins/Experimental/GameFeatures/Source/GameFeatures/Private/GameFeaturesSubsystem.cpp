// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeaturesSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "GameFeaturesProjectPolicies.h"
#include "GameFeatureData.h"
#include "GameFeaturePluginStateMachine.h"
#include "GameFeatureStateChangeObserver.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Containers/Ticker.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/Engine.h"
#include "InstallBundleTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeaturesSubsystem)

DEFINE_LOG_CATEGORY(LogGameFeatures);

const uint32 FInstallBundlePluginProtocolMetaData::FDefaultValues::CurrentVersionNum = 1;
//Missing InstallBundles on purpose as the default is just an empty TArray and should always be encoded
const bool FInstallBundlePluginProtocolMetaData::FDefaultValues::Default_bUninstallBeforeTerminate = true;
const bool FInstallBundlePluginProtocolMetaData::FDefaultValues::Default_bUserPauseDownload = false;
const EInstallBundleRequestFlags FInstallBundlePluginProtocolMetaData::FDefaultValues::Default_InstallBundleFlags = EInstallBundleRequestFlags::Defaults;
const EInstallBundleReleaseRequestFlags FInstallBundlePluginProtocolMetaData::FDefaultValues::Default_ReleaseInstallBundleFlags = EInstallBundleReleaseRequestFlags::None;

namespace UE::GameFeatures
{
	static const FString SubsystemErrorNamespace(TEXT("GameFeaturePlugin.Subsystem."));

	namespace PluginURLStructureInfo
	{
		const TCHAR* OptionAssignOperator = TEXT("=");
		const TCHAR* OptionSeperator = TEXT("?");
	}

	namespace CommonErrorCodes
	{
		const FString PluginNotAllowed = TEXT("Plugin_Denied_By_GameSpecificPolicy");
		const FString DependencyFailedRegister = TEXT("Failed_Dependency_Register");
		const FString BadURL = TEXT("Bad_URL");
		const FString UnreachableState = TEXT("State_Currently_Unreachable");
		const FString NoURLUpdateNeeded = TEXT("URL_Not_Updated");

		const FString CancelAddonCode = TEXT("_Cancel");

		const FText GenericError = NSLOCTEXT("GameFeatures", "CommonErrors.Generic", "An error has occurred. Please try again later.");
	}
}

#define GAME_FEATURE_PLUGIN_STATE_LEX_TO_STRING(inEnum, inText)  \
    case(EGameFeaturePluginState::inEnum):                       \
    {                                                            \
        return TEXT("#inEnum");                                  \
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

FGameFeaturePluginIdentifier::FGameFeaturePluginIdentifier(const FString& PluginURL)
{
	FromPluginURL(PluginURL);
}

void FGameFeaturePluginIdentifier::FromPluginURL(const FString& PluginURLIn)
{
	//Make sure we have no stale data
	IdentifyingURLSubset.Reset();
	PluginURL = PluginURLIn;

	PluginProtocol = UGameFeaturesSubsystem::GetPluginURLProtocol(PluginURL);

	if (ensureAlwaysMsgf( ((PluginProtocol != EGameFeaturePluginProtocol::Unknown) 
						&& (PluginProtocol != EGameFeaturePluginProtocol::Count)),
						TEXT("Invalid PluginProtocol in PluginURL %s"), *PluginURL))
	{

		int32 PluginProtocolEndIndex = FCString::Strlen(UE::GameFeatures::GameFeaturePluginProtocolPrefix(PluginProtocol));
		int32 FirstOptionIndex = PluginURLIn.Find(UE::GameFeatures::PluginURLStructureInfo::OptionSeperator, ESearchCase::IgnoreCase, ESearchDir::FromStart, PluginProtocolEndIndex);
		
		//If we don't have any options, then the IdentifyingURLSubset is just our entire URL except the protocol string
		if (FirstOptionIndex == INDEX_NONE)
		{
			IdentifyingURLSubset = PluginURL.RightChop(PluginProtocolEndIndex);
		}
		//The IdentifyingURLSubset will be the string between the end of the protocol string and before the first option
		else
		{
			const int32 IdentifierCharCount = (FirstOptionIndex - PluginProtocolEndIndex);
			IdentifyingURLSubset = PluginURL.Mid(PluginProtocolEndIndex, IdentifierCharCount);
		}
	}
}

bool FGameFeaturePluginIdentifier::operator==(const FGameFeaturePluginIdentifier& Other) const
{
	return ((PluginProtocol == Other.PluginProtocol) &&
			(IdentifyingURLSubset.Equals(Other.IdentifyingURLSubset, ESearchCase::IgnoreCase)));
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
		TEXT("Loads and activates a game feature plugin by URL"),
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
		TEXT("Deactivates a game feature plugin by URL"),
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
		TEXT("Unloads a game feature plugin by URL"),
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
		TEXT("Unloads a game feature plugin by URL but keeps it registered"),
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
		TEXT("Releases a game feature plugin's InstallBundle data by URL"),
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
		TEXT("Cancel any state changes for a game feature plugin by URL"),
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
	check(GameFeatureToAdd);
	FString PluginRootPath = TEXT("/") + PluginName + TEXT("/");
	UAssetManager& LocalAssetManager = UAssetManager::Get();
	IAssetRegistry& LocalAssetRegistry = LocalAssetManager.GetAssetRegistry();

	// Add the GameFeatureData itself to the primary asset list
#if WITH_EDITOR
	// In the editor, we may not have scanned the FAssetData yet if during startup, but that is fine because we can gather bundles from the object itself, so just create the FAssetData from the object
	LocalAssetManager.RegisterSpecificPrimaryAsset(GameFeatureToAdd->GetPrimaryAssetId(), FAssetData(GameFeatureToAdd));
#else
	// In non-editor, the asset bundle data is compiled out, so it must be gathered from the asset registry instead
	LocalAssetManager.RegisterSpecificPrimaryAsset(GameFeatureToAdd->GetPrimaryAssetId(), LocalAssetRegistry.GetAssetByObjectPath(FSoftObjectPath(GameFeatureToAdd), true));
#endif // WITH_EDITOR

	// @TODO: HACK - There is no guarantee that the plugin mount point was added before inte initial asset scan.
	// If not, ScanPathsForPrimaryAssets will fail to find primary assets without a syncronous scan.
	// A proper fix for this would be to handle all the primary asset discovery internally ins the asset manager 
	// instead of doing it here.
	// We just mounted the folder that contains these primary assets and the editor background scan may not
	// not be finished by the time this is called, but a rescan will happen later in OnAssetRegistryFilesLoaded 
	// as long as LocalAssetRegistry.IsLoadingAssets() is true.
	const bool bForceSynchronousScan = !LocalAssetRegistry.IsLoadingAssets();

	LocalAssetManager.PushBulkScanning();

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
			Path.Path = TEXT("/") + PluginName + TEXT("/") + Path.Path;
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
	UGameFeaturePluginStateMachine* StateMachine = FindOrCreateGameFeaturePluginStateMachine(PluginURL);
	check(StateMachine);

	//Define a lambda wrapper to return the FResult of the Terminate for our FGameFeaturePluginUninstallComplete Delegate
	const FGameFeaturePluginTerminateComplete ReportTerminateResultLambda = FGameFeaturePluginTerminateComplete::CreateLambda([=](const UE::GameFeatures::FResult& Result)
		{
			CompleteDelegate.Execute(Result);
		});

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
			PluginURLForTerminate = GetPluginURL_InstallBundleProtocol(StateMachine->GetPluginName(), ProtocolMetadata);
		}
	}

	TerminateGameFeaturePlugin(PluginURLForTerminate, ReportTerminateResultLambda);
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
		const FGameFeaturePluginUpdateURLComplete StartTerminateLambda = FGameFeaturePluginUpdateURLComplete::CreateLambda([=](const UE::GameFeatures::FResult& Result)
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

void UGameFeaturesSubsystem::LoadBuiltInGameFeaturePlugin(const TSharedRef<IPlugin>& Plugin, FBuiltInPluginAdditionalFilters AdditionalFilter)
{
	UE_SCOPED_ENGINE_ACTIVITY(TEXT("Loading GameFeaturePlugin %s"), *Plugin->GetName());

	UAssetManager::Get().PushBulkScanning();

	const FString& PluginDescriptorFilename = Plugin->GetDescriptorFileName();

	// Make sure you are in a game feature plugins folder. All GameFeaturePlugins are rooted in a GameFeatures folder.
	if (!PluginDescriptorFilename.IsEmpty() && GetDefault<UGameFeaturesSubsystemSettings>()->IsValidGameFeaturePlugin(FPaths::ConvertRelativePathToFull(PluginDescriptorFilename)) && FPaths::FileExists(PluginDescriptorFilename))
	{
		FString PluginURL;
		bool bIsFileProtocol = true;
		if (GetPluginURLByName(Plugin->GetName(), PluginURL))
		{
			bIsFileProtocol = UGameFeaturesSubsystem::IsPluginURLProtocol(PluginURL, EGameFeaturePluginProtocol::File);
		}
		else
		{
			PluginURL = GetPluginURL_FileProtocol(PluginDescriptorFilename);
		}

		if (bIsFileProtocol && GameSpecificPolicies->IsPluginAllowed(PluginURL))
		{
			FGameFeaturePluginDetails PluginDetails;
			if (GetGameFeaturePluginDetails(PluginDescriptorFilename, PluginDetails))
			{
				FBuiltInGameFeaturePluginBehaviorOptions BehaviorOptions;
				bool bShouldProcess = AdditionalFilter(PluginDescriptorFilename, PluginDetails, BehaviorOptions);
				if (bShouldProcess)
				{
					UGameFeaturePluginStateMachine* StateMachine = FindOrCreateGameFeaturePluginStateMachine(PluginURL);

					const EBuiltInAutoState InitialAutoState = (BehaviorOptions.AutoStateOverride != EBuiltInAutoState::Invalid) ? BehaviorOptions.AutoStateOverride : PluginDetails.BuiltInAutoState;
						
					const EGameFeaturePluginState DestinationState = ConvertInitialFeatureStateToTargetState(InitialAutoState);

					// If we're already at the destination or beyond, don't transition back
					FGameFeaturePluginStateRange Destination(DestinationState, EGameFeaturePluginState::Active);
					ChangeGameFeatureDestination(StateMachine, Destination, 
						FGameFeaturePluginChangeStateComplete::CreateUObject(this, &ThisClass::LoadBuiltInGameFeaturePluginComplete, StateMachine, Destination));
				}
			}
		}
	}

	UAssetManager::Get().PopBulkScanning();
}

void UGameFeaturesSubsystem::LoadBuiltInGameFeaturePlugins(FBuiltInPluginAdditionalFilters AdditionalFilter)
{
	UAssetManager::Get().PushBulkScanning();
	UGameplayTagsManager::Get().PushDeferOnGameplayTagTreeChangedBroadcast();

	TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPlugins();
	for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins)
	{
		LoadBuiltInGameFeaturePlugin(Plugin, AdditionalFilter);
	}

	UGameplayTagsManager::Get().PopDeferOnGameplayTagTreeChangedBroadcast();
	UAssetManager::Get().PopBulkScanning();
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

EGameFeaturePluginState UGameFeaturesSubsystem::GetPluginState(const FString& PluginURL)
{
	if (UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		return StateMachine->GetCurrentState();
	}
	else
	{
		return EGameFeaturePluginState::UnknownStatus;
	}
}

bool UGameFeaturesSubsystem::GetGameFeaturePluginDetails(const FString& PluginDescriptorFilename, FGameFeaturePluginDetails& OutPluginDetails) const
{
	// @TODO: We load the descriptor 2-3 per plugin because FPluginReferenceDescriptor doesn't cache any of this info.
	// GFPs are implemented with a plugin so FPluginReferenceDescriptor doesn't know anything about them.
	// Need a better way of storing GFP specific plugin data...

	// Read the file to a string
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *PluginDescriptorFilename))
	{
		UE_LOG(LogGameFeatures, Error, TEXT("UGameFeaturesSubsystem could not determine if feature was hotfixable. Failed to read file. File:%s Error:%d"), *PluginDescriptorFilename, FPlatformMisc::GetLastError());
		return false;
	}

	// Deserialize a JSON object from the string
	TSharedPtr< FJsonObject > ObjectPtr;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(FileContents);
	if (!FJsonSerializer::Deserialize(Reader, ObjectPtr) || !ObjectPtr.IsValid())
	{
		UE_LOG(LogGameFeatures, Error, TEXT("UGameFeaturesSubsystem could not determine if feature was hotfixable. Json invalid. File:%s. Error:%s"), *PluginDescriptorFilename, *Reader->GetErrorMessage());
		return false;
	}

	//@TODO: When we properly support downloaded plugins, will need to determine this
	const bool bIsBuiltInPlugin = true;

	// Read the properties

	// Hotfixable. If it is not specified, then we assume it is
	OutPluginDetails.bHotfixable = true;
	ObjectPtr->TryGetBoolField(TEXT("Hotfixable"), OutPluginDetails.bHotfixable);

	// Determine the initial plugin state
	OutPluginDetails.BuiltInAutoState = bIsBuiltInPlugin ? DetermineBuiltInInitialFeatureState(ObjectPtr, PluginDescriptorFilename) : EBuiltInAutoState::Installed;

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
							TSharedPtr<IPlugin> DependencyPlugin = IPluginManager::Get().FindPlugin(DependencyName);
							if (DependencyPlugin.IsValid())
							{
								FString DependencyURL;
								if (!GetPluginURLByName(DependencyPlugin->GetName(), DependencyURL))
								{
									if (!DependencyPlugin->GetDescriptorFileName().IsEmpty() &&
										GetDefault<UGameFeaturesSubsystemSettings>()->IsValidGameFeaturePlugin(FPaths::ConvertRelativePathToFull(DependencyPlugin->GetDescriptorFileName())) &&
										FPaths::FileExists(DependencyPlugin->GetDescriptorFileName()))
									{
										DependencyURL = GetPluginURL_FileProtocol(DependencyPlugin->GetDescriptorFileName());
									}
								}

								if (!DependencyURL.IsEmpty())
								{
									OutPluginDetails.PluginDependencies.Add(DependencyURL);
								}
							}
							else
							{
								UE_LOG(LogGameFeatures, Display, TEXT("Game feature plugin '%s' has unknown dependency '%s'."), *PluginDescriptorFilename, *DependencyName);
							}
						}
					}
				}
			}
		}
	}

	return true;
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
	TObjectPtr<UGameFeaturePluginStateMachine> const* ExistingStateMachine = GameFeaturePluginStateMachines.Find(PluginURL);
	if (ExistingStateMachine)
	{
		return *ExistingStateMachine;
	}

	return nullptr;
}

UGameFeaturePluginStateMachine* UGameFeaturesSubsystem::FindOrCreateGameFeaturePluginStateMachine(const FString& PluginURL)
{
	if (UGameFeaturePluginStateMachine* ExistingStateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		return ExistingStateMachine;
	}

	UGameFeaturePluginStateMachine* NewStateMachine = NewObject<UGameFeaturePluginStateMachine>(this);
	GameFeaturePluginStateMachines.Add(PluginURL, NewStateMachine);
	NewStateMachine->InitStateMachine(PluginURL);

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
	GameFeaturePluginStateMachines.Remove(Machine->GetPluginURL());
	TerminalGameFeaturePluginStateMachines.Add(Machine);
}

void UGameFeaturesSubsystem::FinishTermination(UGameFeaturePluginStateMachine* Machine)
{
	TerminalGameFeaturePluginStateMachines.RemoveSwap(Machine);
}

bool UGameFeaturesSubsystem::FindOrCreatePluginDependencyStateMachines(const FString& PluginFilename, TArray<UGameFeaturePluginStateMachine*>& OutDependencyMachines)
{
	FGameFeaturePluginDetails Details;
	if (GetGameFeaturePluginDetails(PluginFilename, Details))
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

	for (const TPair<FGameFeaturePluginIdentifier, TObjectPtr<UGameFeaturePluginStateMachine>>& Pair : GameFeaturePluginStateMachines)
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


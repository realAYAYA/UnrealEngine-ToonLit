// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeaturesSubsystem.h"
#include "Algo/TopologicalSort.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "BundlePrereqCombinedStatusHelper.h"
#include "Dom/JsonValue.h"
#include "GameFeaturesSubsystemSettings.h"
#include "GameFeaturesProjectPolicies.h"
#include "GameFeatureData.h"
#include "GameFeaturePluginStateMachine.h"
#include "GameFeatureStateChangeObserver.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Logging/StructuredLog.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Stats/StatsMisc.h"
#include "String/ParseTokens.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManagerSettings.h"
#include "InstallBundleTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeaturesSubsystem)

DEFINE_LOG_CATEGORY(LogGameFeatures);

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

#define GAME_FEATURE_PLUGIN_PROTOCOL_PREFIX(inEnum, inString) case EGameFeaturePluginProtocol::inEnum: return inString;
	static const TCHAR* GameFeaturePluginProtocolPrefix(EGameFeaturePluginProtocol Protocol)
	{
		switch (Protocol)
		{
			GAME_FEATURE_PLUGIN_PROTOCOL_LIST(GAME_FEATURE_PLUGIN_PROTOCOL_PREFIX)
		}

		check(false);
		return nullptr;
	}
#undef GAME_FEATURE_PLUGIN_PROTOCOL_PREFIX

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

	TOptional<FString> GetPluginUrlForConsoleCommand(const TArray<FString>& Args, FOutputDevice& Ar)
	{
		TOptional<FString> PluginURL;
		if (Args.Num() > 0)
		{
			EGameFeaturePluginProtocol Protocol = UGameFeaturesSubsystem::GetPluginURLProtocol(Args[0]);
			if (Protocol != EGameFeaturePluginProtocol::Unknown)
			{
				PluginURL.Emplace(Args[0]);
			}
			else
			{
				FString PluginURLStr;
				if (UGameFeaturesSubsystem::Get().GetPluginURLByName(Args[0], /*out*/ PluginURLStr))
				{
					PluginURL.Emplace(MoveTemp(PluginURLStr));
				}
			}
		}

		if (PluginURL)
		{
			Ar.Logf(TEXT("Using URL %s for console command"), *PluginURL.GetValue());
		}
		else
		{
			Ar.Logf(TEXT("Expected a game feature plugin URL or name as an argument"));
		}

		return PluginURL;
	}
}

const FString LexToString(const EBuiltInAutoState BuiltInAutoState)
{
	switch (BuiltInAutoState)
	{
		case EBuiltInAutoState::Invalid:
			return TEXT("Invalid");
		case EBuiltInAutoState::Installed:
			return TEXT("Installed");
		case EBuiltInAutoState::Registered:
			return TEXT("Registered");
		case EBuiltInAutoState::Loaded:
			return TEXT("Loaded");
		case EBuiltInAutoState::Active:
			return TEXT("Active");
		default:
			check(false);
			return TEXT("Unknown");
	}
}

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

	if (UGameFeaturesSubsystem::ParsePluginURL(PluginURL, &PluginProtocol, &IdentifyingURLSubset))
	{
		if (!IdentifyingURLSubset.IsEmpty())
		{
			// Plugins must be unique so just use the name as the identifier. This avoids issues with normalizing paths.
			IdentifyingURLSubset = FPathViews::GetCleanFilename(IdentifyingURLSubset);
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

FStringView FGameFeaturePluginIdentifier::GetPluginName() const
{ 
	return FPathViews::GetBaseFilename(IdentifyingURLSubset); 
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
		[CompletionCallback = CompletionCallback, PauserTag = MoveTemp(InPauserTag)]() { CompletionCallback(PauserTag); }
	);
}

FSimpleDelegate FGameFeaturePostMountingContext::PauseUntilComplete(FString InPauserTag)
{
	UE_LOG(LogGameFeatures, Display, TEXT("Post-mount of %.*s paused by %s"), PluginName.Len(), PluginName.GetData(), *InPauserTag);

	++NumPausers;
	return FSimpleDelegate::CreateLambda(
		[CompletionCallback = CompletionCallback, PauserTag = MoveTemp(InPauserTag)]() { CompletionCallback(PauserTag); }
	);
}

FInstallBundlePluginProtocolOptions::FInstallBundlePluginProtocolOptions()
	: InstallBundleFlags(EInstallBundleRequestFlags::Defaults)
	, ReleaseInstallBundleFlags(EInstallBundleReleaseRequestFlags::None)
	, bUninstallBeforeTerminate(false)
	, bUserPauseDownload(false)
	, bAllowIniLoading(false)
	, bDoNotDownload(false)
{}

bool FInstallBundlePluginProtocolOptions::operator==(const FInstallBundlePluginProtocolOptions& Other) const
{
	return
		InstallBundleFlags == Other.InstallBundleFlags &&
		ReleaseInstallBundleFlags == Other.ReleaseInstallBundleFlags &&
		bUninstallBeforeTerminate == Other.bUninstallBeforeTerminate &&
		bUserPauseDownload == Other.bUserPauseDownload && 
		bAllowIniLoading == Other.bAllowIniLoading &&
		bDoNotDownload == Other.bDoNotDownload;
}

// TODO : FGameFeatureProtocolOptions - C++20 will allow inline init for bitfields
FGameFeatureProtocolOptions::FGameFeatureProtocolOptions()
	: bForceSyncLoading(false)
	, bLogWarningOnForcedDependencyCreation(false)
	, bLogErrorOnForcedDependencyCreation(false)
{ 
	SetSubtype<FNull>(); 
}

FGameFeatureProtocolOptions::FGameFeatureProtocolOptions(const FInstallBundlePluginProtocolOptions& InOptions) 
	: TUnion(InOptions) 
	, bForceSyncLoading(false)
	, bLogWarningOnForcedDependencyCreation(false)
	, bLogErrorOnForcedDependencyCreation(false)
{
}

FGameFeatureProtocolOptions::FGameFeatureProtocolOptions(FNull InOptions)
	: bForceSyncLoading(false)
	, bLogWarningOnForcedDependencyCreation(false)
	, bLogErrorOnForcedDependencyCreation(false)
{
	SetSubtype<FNull>(InOptions);
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
		TEXT("Prints game features plugins and their current state to log. (options: [-activeonly] [-csv])"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateUObject(this, &ThisClass::ListGameFeaturePlugins),
		ECVF_Default);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("LoadGameFeaturePlugin"),
		TEXT("Loads and activates a game feature plugin by PluginName or URL"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
		{
			if (TOptional<FString> PluginURL = UE::GameFeatures::GetPluginUrlForConsoleCommand(Args, Ar))
			{
				UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(PluginURL.GetValue(), FGameFeaturePluginLoadComplete());
			}
		}),
		ECVF_Cheat);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("DeactivateGameFeaturePlugin"),
		TEXT("Deactivates a game feature plugin by PluginName or URL"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
		{
			if (TOptional<FString> PluginURL = UE::GameFeatures::GetPluginUrlForConsoleCommand(Args, Ar))
			{
				UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(PluginURL.GetValue(), FGameFeaturePluginLoadComplete());
			}
		}),
		ECVF_Cheat);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("UnloadGameFeaturePlugin"),
		TEXT("Unloads a game feature plugin by PluginName or URL"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
		{
			if (TOptional<FString> PluginURL = UE::GameFeatures::GetPluginUrlForConsoleCommand(Args, Ar))
			{
				UGameFeaturesSubsystem::Get().UnloadGameFeaturePlugin(PluginURL.GetValue());
			}
		}),
		ECVF_Cheat);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("UnloadAndKeepRegisteredGameFeaturePlugin"),
		TEXT("Unloads a game feature plugin by PluginName or URL but keeps it registered"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
		{
			if (TOptional<FString> PluginURL = UE::GameFeatures::GetPluginUrlForConsoleCommand(Args, Ar))
			{
				UGameFeaturesSubsystem::Get().UnloadGameFeaturePlugin(PluginURL.GetValue(), true);
			}
		}),
		ECVF_Cheat);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ReleaseGameFeaturePlugin"),
		TEXT("Releases a game feature plugin's InstallBundle data by PluginName or URL"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
		{
			if (TOptional<FString> PluginURL = UE::GameFeatures::GetPluginUrlForConsoleCommand(Args, Ar))
			{
				UGameFeaturesSubsystem::Get().ReleaseGameFeaturePlugin(PluginURL.GetValue());
			}
		}),
		ECVF_Cheat);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("CancelGameFeaturePlugin"),
		TEXT("Cancel any state changes for a game feature plugin by PluginName or URL"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
		{
			if (TOptional<FString> PluginURL = UE::GameFeatures::GetPluginUrlForConsoleCommand(Args, Ar))
			{
				UGameFeaturesSubsystem::Get().CancelGameFeatureStateChange(PluginURL.GetValue());
			}
		}),
		ECVF_Cheat);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("TerminateGameFeaturePlugin"),
		TEXT("Terminates a game feature plugin by PluginName or URL"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
		{
			if (TOptional<FString> PluginURL = UE::GameFeatures::GetPluginUrlForConsoleCommand(Args, Ar))
			{
				UGameFeaturesSubsystem::Get().TerminateGameFeaturePlugin(PluginURL.GetValue());
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
		const bool bHasProject = FApp::HasProjectName(); // Only error when we have a UE project loaded, as otherwise there won't be any GFPs to load
		UE_CLOG(bHasProject, LogGameFeatures, Error, TEXT("Asset manager settings do not include a rule for assets of type %s, which is required for game feature plugins to function"), *UGameFeatureData::StaticClass()->GetName());
	}

	// Create the game-specific policy
	UE_LOG(LogGameFeatures, Verbose, TEXT("Initializing game features policy (type %s)"), *GameSpecificPolicies->GetClass()->GetName());
	GameSpecificPolicies->InitGameFeatureManager();
	bInitializedPolicyManager = true;
}

TSharedPtr<FStreamableHandle> UGameFeaturesSubsystem::LoadGameFeatureData(const FString& GameFeatureToLoad, bool bStartStalled /*= false*/)
{
	return UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		FSoftObjectPath(GameFeatureToLoad), 
		FStreamableDelegate(), 
		FStreamableManager::DefaultAsyncLoadPriority, 
		false, 
		bStartStalled, 
		TEXT("LoadGameFeatureData"));
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
		for (FDirectoryPath& Path : TypeInfo.GetDirectories())
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

	const UAssetManagerSettings& Settings = LocalAssetManager.GetSettings();
	for (const FPrimaryAssetRulesCustomOverride& Override : Settings.CustomPrimaryAssetRules)
	{
		if (Override.FilterDirectory.Path.StartsWith(PluginRootPath))
		{
			LocalAssetManager.ApplyCustomPrimaryAssetRulesOverride(Override);
		}
	}
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

		for (FDirectoryPath& Path : TypeInfo.GetDirectories())
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

void UGameFeaturesSubsystem::ForEachGameFeature(TFunctionRef<void(FGameFeatureInfo&&)> Visitor) const
{
	for (auto StateMachineIt = GameFeaturePluginStateMachines.CreateConstIterator(); StateMachineIt; ++StateMachineIt)
	{
		if (UGameFeaturePluginStateMachine* GFSM = StateMachineIt.Value())
		{
			FGameFeatureInfo GameFeatureInfo = { GFSM->GetPluginName(), GFSM->GetPluginURL(), GFSM->WasLoadedAsBuiltIn(), GFSM->GetCurrentState() };
			Visitor(MoveTemp(GameFeatureInfo));
		}
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
	return UE::GameFeatures::GameFeaturePluginProtocolPrefix(EGameFeaturePluginProtocol::File) + PluginDescriptorPath;
}

FString UGameFeaturesSubsystem::GetPluginURL_FileProtocol(const FString& PluginDescriptorPath, TArrayView<const TPair<FString, FString>> AdditionalOptions)
{
	FString Path;
	Path += UE::GameFeatures::GameFeaturePluginProtocolPrefix(EGameFeaturePluginProtocol::File);
	Path += PluginDescriptorPath;
	if (AdditionalOptions.Num() > 0)
	{
		Path += UE::GameFeatures::PluginURLStructureInfo::OptionSeperator;
		Path += FString::JoinBy(AdditionalOptions, UE::GameFeatures::PluginURLStructureInfo::OptionSeperator,
			[](const TPair<FString, FString>& OptionPair)
			{
				return OptionPair.Key + UE::GameFeatures::PluginURLStructureInfo::OptionAssignOperator + OptionPair.Value;
			});
	}
	return Path;
}

FString GetPluginURL_InstallBundleProtocol(const FString& PluginName, const FInstallBundlePluginProtocolMetaData& ProtocolMetadata, TArrayView<const TPair<FString, FString>> AdditionalOptions = {})
{
	ensure(ProtocolMetadata.InstallBundles.Num() > 0);
	FString Path;
	Path += UE::GameFeatures::GameFeaturePluginProtocolPrefix(EGameFeaturePluginProtocol::InstallBundle);
	Path += PluginName;
	Path += ProtocolMetadata.ToString();
	if (AdditionalOptions.Num() > 0)
	{
		Path += UE::GameFeatures::PluginURLStructureInfo::OptionSeperator;
		Path += FString::JoinBy(AdditionalOptions, UE::GameFeatures::PluginURLStructureInfo::OptionSeperator,
			[](const TPair<FString, FString>& OptionPair)
			{
				return OptionPair.Key + UE::GameFeatures::PluginURLStructureInfo::OptionAssignOperator + OptionPair.Value;
			});
	}

	return Path;
}

FString UGameFeaturesSubsystem::GetPluginURL_InstallBundleProtocol(const FString& PluginName, TArrayView<const FString> BundleNames)
{
	FInstallBundlePluginProtocolMetaData ProtocolMetadata;
	for (const FString& BundleName : BundleNames)
	{
		ProtocolMetadata.InstallBundles.Emplace(BundleName);
	}
	return ::GetPluginURL_InstallBundleProtocol(PluginName, ProtocolMetadata);
}

FString UGameFeaturesSubsystem::GetPluginURL_InstallBundleProtocol(const FString& PluginName, const FString& BundleName)
{
	return GetPluginURL_InstallBundleProtocol(PluginName, MakeArrayView(&BundleName, 1));
}

FString UGameFeaturesSubsystem::GetPluginURL_InstallBundleProtocol(const FString& PluginName, const TArrayView<const FName> BundleNames)
{
	return GetPluginURL_InstallBundleProtocol(PluginName, BundleNames, TArrayView<const TPair<FString, FString>>());
}

FString UGameFeaturesSubsystem::GetPluginURL_InstallBundleProtocol(const FString& PluginName, FName BundleName)
{
	return GetPluginURL_InstallBundleProtocol(PluginName, MakeArrayView(&BundleName, 1));
}

FString UGameFeaturesSubsystem::GetPluginURL_InstallBundleProtocol(const FString& PluginName, TArrayView<const FName> BundleNames, TArrayView<const TPair<FString, FString>> AdditionalOptions)
{
	FInstallBundlePluginProtocolMetaData ProtocolMetadata;
	ProtocolMetadata.InstallBundles.Append(BundleNames.GetData(), BundleNames.Num());
	return ::GetPluginURL_InstallBundleProtocol(PluginName, ProtocolMetadata, AdditionalOptions);
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

bool UGameFeaturesSubsystem::ParsePluginURL(FStringView PluginURL, EGameFeaturePluginProtocol* OutProtocol /*= nullptr*/, FStringView* OutPath /*= nullptr*/, FStringView* OutOptions /*= nullptr*/)
{
	FStringView Path;
	FStringView Options;
	EGameFeaturePluginProtocol PluginProtocol = UGameFeaturesSubsystem::GetPluginURLProtocol(PluginURL);

	if (ensureAlwaysMsgf(PluginProtocol != EGameFeaturePluginProtocol::Unknown && PluginProtocol != EGameFeaturePluginProtocol::Count,
		TEXT("Invalid PluginProtocol in PluginURL %.*s"), PluginURL.Len(), PluginURL.GetData()))
	{
		int32 PluginProtocolLen = FCString::Strlen(UE::GameFeatures::GameFeaturePluginProtocolPrefix(PluginProtocol));
		int32 FirstOptionIndex = UE::String::FindFirst(PluginURL, UE::GameFeatures::PluginURLStructureInfo::OptionSeperator, ESearchCase::IgnoreCase);

		//If we don't have any options, then the Path is just our entire URL except the protocol string
		if (FirstOptionIndex == INDEX_NONE)
		{
			Path = PluginURL.RightChop(PluginProtocolLen);
		}
		//The Path will be the string between the end of the protocol string and before the first option
		else
		{
			const int32 IdentifierCharCount = (FirstOptionIndex - PluginProtocolLen);
			Path = PluginURL.Mid(PluginProtocolLen, IdentifierCharCount);
			Options = PluginURL.RightChop(FirstOptionIndex);
		}

		if (ensureAlwaysMsgf(Path.EndsWith(TEXTVIEW(".uplugin")), TEXT("Invalid path in PluginURL %.*s"), PluginURL.Len(), PluginURL.GetData()))
		{
			if (OutProtocol)
			{
				*OutProtocol = PluginProtocol;
			}

			if (OutPath)
			{
				*OutPath = Path;
			}

			if (OutOptions)
			{
				*OutOptions = Options;
			}

			return true;
		}
	}

	return false;
}

namespace GameFeaturesSubsystem
{
	static bool SplitOption(FStringView OptionPair, FStringView& OutOptionName, FStringView& OutOptionValue)
	{
		int32 TokenCount = 0;
		FStringView OptionName;
		FStringView OptionValue;
		UE::String::ParseTokens(OptionPair, UE::GameFeatures::PluginURLStructureInfo::OptionAssignOperator, 
		[&TokenCount, &OptionName, &OptionValue](FStringView Token)
		{
			++TokenCount;
			switch (TokenCount)
			{
			case 1:
				OptionName = Token;
				break;
			case 2:
				OptionValue = Token;
				break;
			}
		});

		const bool bSuccess = TokenCount == 2;
		if (bSuccess)
		{
			OutOptionName = OptionName;
			OutOptionValue = OptionValue;
		}

		return bSuccess;
	}

	struct FParsePluginURLOptionsFilter
	{
		EGameFeatureURLOptions OptionsFlags;
		TConstArrayView<FStringView> AdditionalOptions;
	};

	static bool ParsePluginURLOptions(FStringView URLOptionsString, const FParsePluginURLOptionsFilter* OptionsFilter,
		TFunctionRef<void(EGameFeatureURLOptions Option, FStringView OptionString, FStringView OptionValue)> Output)
	{
		enum class EParseState : uint8
		{
			First,
			OK,
			Error
		};

		//Parse through our URLOptions. The first option won't appear until after the first seperator.
		//We don't care what comes before the first seperator
		EParseState ParseState = EParseState::First;
		UE::String::ParseTokens(URLOptionsString, UE::GameFeatures::PluginURLStructureInfo::OptionSeperator, 
		[OptionsFilter, Output, &ParseState](FStringView Token)
		{
			if (ParseState == EParseState::Error)
			{
				return;
			}

			if (ParseState == EParseState::First)
			{
				ParseState = EParseState::OK;
				return;
			}

			FStringView OptionName;
			FStringView OptionValue;
			if (!GameFeaturesSubsystem::SplitOption(Token, OptionName, OptionValue))
			{
				ParseState = EParseState::Error;
				return;
			}

			EGameFeatureURLOptions OptionEnum = EGameFeatureURLOptions::None;

			if (!OptionsFilter || OptionsFilter->OptionsFlags != EGameFeatureURLOptions::None)
			{
				LexFromString(OptionEnum, OptionName);
			}

			if (!OptionsFilter || EnumHasAnyFlags(OptionsFilter->OptionsFlags, OptionEnum) || OptionsFilter->AdditionalOptions.Contains(OptionName))
			{
				UE::String::ParseTokens(OptionValue, UE::GameFeatures::PluginURLStructureInfo::OptionListSeperator,
				[Output, OptionEnum, OptionName](FStringView ListToken)
				{
					Output(OptionEnum, OptionName, ListToken);
				});
			}
		});

		return ParseState == EParseState::OK;
	}
}

bool UGameFeaturesSubsystem::ParsePluginURLOptions(FStringView URLOptionsString,
	TFunctionRef<void(EGameFeatureURLOptions Option, FStringView OptionString, FStringView OptionValue)> Output)
{
	return GameFeaturesSubsystem::ParsePluginURLOptions(URLOptionsString, nullptr, Output);
}

bool UGameFeaturesSubsystem::ParsePluginURLOptions(FStringView URLOptionsString, EGameFeatureURLOptions OptionsFlags,
	TFunctionRef<void(EGameFeatureURLOptions Option, FStringView OptionString, FStringView OptionValue)> Output)
{
	const GameFeaturesSubsystem::FParsePluginURLOptionsFilter OptionsFilter{ OptionsFlags, {} };
	return GameFeaturesSubsystem::ParsePluginURLOptions(URLOptionsString, &OptionsFilter, Output);
}

bool UGameFeaturesSubsystem::ParsePluginURLOptions(FStringView URLOptionsString, TConstArrayView<FStringView> AdditionalOptions,
	TFunctionRef<void(EGameFeatureURLOptions Option, FStringView OptionString, FStringView OptionValue)> Output)
{
	const GameFeaturesSubsystem::FParsePluginURLOptionsFilter OptionsFilter{ EGameFeatureURLOptions::None, AdditionalOptions };
	return GameFeaturesSubsystem::ParsePluginURLOptions(URLOptionsString, &OptionsFilter, Output);
}

bool UGameFeaturesSubsystem::ParsePluginURLOptions(FStringView URLOptionsString, EGameFeatureURLOptions OptionsFlags, TConstArrayView<FStringView> AdditionalOptions,
	TFunctionRef<void(EGameFeatureURLOptions Option, FStringView OptionString, FStringView OptionValue)> Output)
{
	const GameFeaturesSubsystem::FParsePluginURLOptionsFilter OptionsFilter{ OptionsFlags, AdditionalOptions };
	return GameFeaturesSubsystem::ParsePluginURLOptions(URLOptionsString, &OptionsFilter, Output);
}

void UGameFeaturesSubsystem::OnGameFeatureTerminating(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	CallbackObservers(EObserverCallback::Terminating, PluginIdentifier, &PluginName);

	if (!PluginName.IsEmpty())
	{
		// Unmap plugin name to plugin URL
		GameFeaturePluginNameToPathMap.Remove(PluginName);
	}
}

void UGameFeaturesSubsystem::OnGameFeatureCheckingStatus(const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	CallbackObservers(EObserverCallback::CheckingStatus, PluginIdentifier);
}

void UGameFeaturesSubsystem::OnGameFeatureStatusKnown(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	// Map plugin name to plugin URL
	if (ensure(!GameFeaturePluginNameToPathMap.Contains(PluginName)))
	{
		GameFeaturePluginNameToPathMap.Add(PluginName, PluginIdentifier.GetFullPluginURL());
	}
}

void UGameFeaturesSubsystem::OnGameFeaturePredownloading(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	CallbackObservers(EObserverCallback::Predownloading, PluginIdentifier, &PluginName);
}

void UGameFeaturesSubsystem::OnGameFeatureDownloading(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	CallbackObservers(EObserverCallback::Downloading, PluginIdentifier, &PluginName);
}

void UGameFeaturesSubsystem::OnGameFeatureReleasing(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	CallbackObservers(EObserverCallback::Releasing, PluginIdentifier, &PluginName);
}

void UGameFeaturesSubsystem::OnGameFeaturePreMounting(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier, FGameFeaturePreMountingContext& Context)
{
	CallbackObservers(EObserverCallback::PreMounting, PluginIdentifier, &PluginName, /*GameFeatureData=*/nullptr, &Context);
}

void UGameFeaturesSubsystem::OnGameFeaturePostMounting(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier, FGameFeaturePostMountingContext& Context)
{
	CallbackObservers(EObserverCallback::PostMounting, PluginIdentifier, &PluginName, /*GameFeatureData=*/nullptr, &Context);
}

void UGameFeaturesSubsystem::OnGameFeatureRegistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	CallbackObservers(EObserverCallback::Registering, PluginIdentifier, &PluginName, GameFeatureData);

	for (UGameFeatureAction* Action : GameFeatureData->GetActions())
	{
		if (Action != nullptr)
		{
			Action->OnGameFeatureRegistering();
		}
	}
}

void UGameFeaturesSubsystem::OnGameFeatureUnregistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	CallbackObservers(EObserverCallback::Unregistering, PluginIdentifier, &PluginName, GameFeatureData);

#if !WITH_EDITOR
	check(GameFeatureData);
#else
	if (GameFeatureData) // In the editor the GameFeatureData asset can be force deleted, otherwise it should exist
#endif
	{
		for (UGameFeatureAction* Action : GameFeatureData->GetActions())
		{
			if (Action != nullptr)
			{
				Action->OnGameFeatureUnregistering();
			}
		}
	}
}

void UGameFeaturesSubsystem::OnGameFeatureLoading(const UGameFeatureData* GameFeatureData, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	CallbackObservers(EObserverCallback::Loading, PluginIdentifier, nullptr, GameFeatureData);

	for (UGameFeatureAction* Action : GameFeatureData->GetActions())
	{
		if (Action != nullptr)
		{
			Action->OnGameFeatureLoading();
		}
	}
}
void UGameFeaturesSubsystem::OnGameFeatureUnloading(const UGameFeatureData* GameFeatureData, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	CallbackObservers(EObserverCallback::Unloading, PluginIdentifier, nullptr, GameFeatureData);

#if !WITH_EDITOR
	check(GameFeatureData);
#else
	if (GameFeatureData) // In the editor the GameFeatureData asset can be force deleted, otherwise it should exist
#endif
	{
		for (UGameFeatureAction* Action : GameFeatureData->GetActions())
		{
			if (Action != nullptr)
			{
				Action->OnGameFeatureUnloading();
			}
		}
	}
}

void UGameFeaturesSubsystem::OnGameFeatureActivating(const UGameFeatureData* GameFeatureData, const FString& PluginName, FGameFeatureActivatingContext& Context, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GFP_OnActivating_CallbackObservers);
		CallbackObservers(EObserverCallback::Activating, PluginIdentifier, &PluginName, GameFeatureData);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GFP_OnActivating_CallbackActions);
		for (UGameFeatureAction* Action : GameFeatureData->GetActions())
		{
			if (Action != nullptr)
			{
				Action->OnGameFeatureActivating(Context);
			}
		}
	}
}

void UGameFeaturesSubsystem::OnGameFeatureDeactivating(const UGameFeatureData* GameFeatureData, const FString& PluginName, FGameFeatureDeactivatingContext& Context, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
#if !WITH_EDITOR
	check(GameFeatureData);
#else
	if (GameFeatureData) // In the editor the GameFeatureData asset can be force deleted, otherwise it should exist
#endif
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GFP_OnDeactivating_CallbackObservers);
		CallbackObservers(EObserverCallback::Deactivating, PluginIdentifier, &PluginName, GameFeatureData, &Context);
	}
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GFP_OnDeactivating_OnGameFeatureDeactivating);
		for (UGameFeatureAction* Action : GameFeatureData->GetActions())
		{
			if (Action != nullptr)
			{
				Action->OnGameFeatureDeactivating(Context);
			}
		}
	}
}

void UGameFeaturesSubsystem::OnGameFeaturePauseChange(const FGameFeaturePluginIdentifier& PluginIdentifier, const FString& PluginName, FGameFeaturePauseStateChangeContext& Context)
{
	CallbackObservers(EObserverCallback::PauseChanged, PluginIdentifier, &PluginName, nullptr, &Context);
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

bool UGameFeaturesSubsystem::IsGameFeaturePluginMounted(const FString& PluginURL) const
{
	if (const UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		return StateMachine->GetCurrentState() > EGameFeaturePluginState::Mounting;
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

bool UGameFeaturesSubsystem::WasGameFeaturePluginLoadedAsBuiltIn(const FString& PluginURL) const
{
	if (const UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		return StateMachine->WasLoadedAsBuiltIn();
	}
	return false;
}

void UGameFeaturesSubsystem::LoadGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginLoadComplete& CompleteDelegate)
{
	LoadGameFeaturePlugin(PluginURL, FGameFeatureProtocolOptions(), CompleteDelegate);
}

void UGameFeaturesSubsystem::LoadGameFeaturePlugin(const FString& PluginURL, const FGameFeatureProtocolOptions& ProtocolOptions, const FGameFeaturePluginLoadComplete& CompleteDelegate)
{
	const bool bIsPluginAllowed = GameSpecificPolicies->IsPluginAllowed(PluginURL);
	if (!bIsPluginAllowed)
	{
		CompleteDelegate.ExecuteIfBound(UE::GameFeatures::FResult(MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::PluginNotAllowed)));
		return;
	}

	UGameFeaturePluginStateMachine* StateMachine = FindOrCreateGameFeaturePluginStateMachine(PluginURL, ProtocolOptions);

	if (!StateMachine->IsRunning() && StateMachine->GetCurrentState() == EGameFeaturePluginState::Active)
	{
		// TODO: Resolve the activated case here, this is needed because in a PIE environment the plugins
		// are not sandboxed, and we need to do simulate a successful activate call in order run GFP systems 
		// on whichever Role runs second between client and server.

		// Refire the observer for Activated and do nothing else.
		CallbackObservers(EObserverCallback::Activating, StateMachine->GetPluginIdentifier(), &StateMachine->GetPluginName(), StateMachine->GetGameFeatureDataForActivePlugin());
	}

	if (ShouldUpdatePluginProtocolOptions(StateMachine, ProtocolOptions))
	{
		const UE::GameFeatures::FResult Result = UpdateGameFeatureProtocolOptions(StateMachine, ProtocolOptions);
		if (Result.HasError())
		{
			CompleteDelegate.ExecuteIfBound(Result);
			return;
		}
	}

	ChangeGameFeatureDestination(StateMachine, ProtocolOptions, FGameFeaturePluginStateRange(EGameFeaturePluginState::Loaded, EGameFeaturePluginState::Active), CompleteDelegate);
}

void UGameFeaturesSubsystem::LoadGameFeaturePlugin(TConstArrayView<FString> PluginURLs, const FGameFeatureProtocolOptions& ProtocolOptions, const FMultipleGameFeaturePluginsLoaded& CompleteDelegate)
{
	struct FLoadContext
	{
		TMap<FString, UE::GameFeatures::FResult> Results;
		FMultipleGameFeaturePluginsLoaded CompleteDelegate;

		int32 NumPluginsLoaded = 0;
		bool bPushedTagsBroadcast = false;

		FLoadContext()
		{
			if (!IsEngineExitRequested())
			{
				UGameplayTagsManager::Get().PushDeferOnGameplayTagTreeChangedBroadcast();
				bPushedTagsBroadcast = true;
			}
			else if(UGameplayTagsManager* TagsManager = UGameplayTagsManager::GetIfAllocated())
			{
				TagsManager->PushDeferOnGameplayTagTreeChangedBroadcast();
				bPushedTagsBroadcast = true;
			}
		}

		~FLoadContext()
		{
			if (bPushedTagsBroadcast)
			{
				if (UGameplayTagsManager* TagsManager = UGameplayTagsManager::GetIfAllocated())
				{
					TagsManager->PopDeferOnGameplayTagTreeChangedBroadcast();
				}
			}

			CompleteDelegate.ExecuteIfBound(Results);
		}
	};
	TSharedRef<FLoadContext> LoadContext = MakeShared<FLoadContext>();
	LoadContext->CompleteDelegate = CompleteDelegate;

	LoadContext->Results.Reserve(PluginURLs.Num());
	for (const FString& PluginURL : PluginURLs)
	{
		LoadContext->Results.Add(PluginURL, MakeError("Pending"));
	}

	const int32 NumPluginsToLoad = PluginURLs.Num();
	UE_LOG(LogGameFeatures, Log, TEXT("Loading %i GFPs"), NumPluginsToLoad);

	for (const FString& PluginURL : PluginURLs)
	{
		LoadGameFeaturePlugin(PluginURL, ProtocolOptions, FGameFeaturePluginChangeStateComplete::CreateLambda([LoadContext, PluginURL](const UE::GameFeatures::FResult& Result)
		{
			LoadContext->Results.Add(PluginURL, Result);
			++LoadContext->NumPluginsLoaded;
			UE_LOG(LogGameFeatures, VeryVerbose, TEXT("Finished Loading %i GFPs"), LoadContext->NumPluginsLoaded);
		}));
	}
}

void UGameFeaturesSubsystem::LoadAndActivateGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginLoadComplete& CompleteDelegate)
{
	ChangeGameFeatureTargetState(PluginURL, EGameFeatureTargetState::Active, CompleteDelegate);
}

void UGameFeaturesSubsystem::LoadAndActivateGameFeaturePlugin(const FString& PluginURL, const FGameFeatureProtocolOptions& ProtocolOptions, const FGameFeaturePluginLoadComplete& CompleteDelegate)
{
	ChangeGameFeatureTargetState(PluginURL, ProtocolOptions, EGameFeatureTargetState::Active, CompleteDelegate);
}

void UGameFeaturesSubsystem::LoadAndActivateGameFeaturePlugin(TConstArrayView<FString> PluginURLs, const FGameFeatureProtocolOptions& ProtocolOptions, const FMultipleGameFeaturePluginsLoaded& CompleteDelegate)
{
	ChangeGameFeatureTargetState(PluginURLs, ProtocolOptions, EGameFeatureTargetState::Active, CompleteDelegate);
}

void UGameFeaturesSubsystem::ChangeGameFeatureTargetState(const FString& PluginURL, EGameFeatureTargetState TargetState, const FGameFeaturePluginChangeStateComplete& CompleteDelegate)
{
	ChangeGameFeatureTargetState(PluginURL, FGameFeatureProtocolOptions(), TargetState, CompleteDelegate);
}

void UGameFeaturesSubsystem::ChangeGameFeatureTargetState(const FString& PluginURL, const FGameFeatureProtocolOptions& ProtocolOptions, EGameFeatureTargetState TargetState, const FGameFeaturePluginChangeStateComplete& CompleteDelegate)
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
		StateMachine = FindOrCreateGameFeaturePluginStateMachine(PluginURL, ProtocolOptions);
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
		CallbackObservers(EObserverCallback::Activating, StateMachine->GetPluginIdentifier(), &StateMachine->GetPluginName(), StateMachine->GetGameFeatureDataForActivePlugin());
	}
	
	if (ShouldUpdatePluginProtocolOptions(StateMachine, ProtocolOptions))
	{
		const UE::GameFeatures::FResult Result = UpdateGameFeatureProtocolOptions(StateMachine, ProtocolOptions);
		if (Result.HasError())
		{
			CompleteDelegate.ExecuteIfBound(Result);
			return;
		}
	}
	
	ChangeGameFeatureDestination(StateMachine, ProtocolOptions, FGameFeaturePluginStateRange(TargetPluginState), CompleteDelegate);
}

void UGameFeaturesSubsystem::ChangeGameFeatureTargetState(TConstArrayView<FString> PluginURLs, const FGameFeatureProtocolOptions& ProtocolOptions, EGameFeatureTargetState TargetState, const FMultipleGameFeaturePluginsLoaded& CompleteDelegate)
{
	struct FLoadContext
	{
		TMap<FString, UE::GameFeatures::FResult> Results;
		FMultipleGameFeaturePluginsLoaded CompleteDelegate;

		int32 NumPluginsLoaded = 0;
		bool bPushedTagsBroadcast = false;

		FLoadContext()
		{
			if (!IsEngineExitRequested())
			{
				UGameplayTagsManager::Get().PushDeferOnGameplayTagTreeChangedBroadcast();
				bPushedTagsBroadcast = true;
			}
			else if(UGameplayTagsManager* TagsManager = UGameplayTagsManager::GetIfAllocated())
			{
				TagsManager->PushDeferOnGameplayTagTreeChangedBroadcast();
				bPushedTagsBroadcast = true;
			}
		}

		~FLoadContext()
		{
			if (bPushedTagsBroadcast)
			{
				if (UGameplayTagsManager* TagsManager = UGameplayTagsManager::GetIfAllocated())
				{
					TagsManager->PopDeferOnGameplayTagTreeChangedBroadcast();
				}
			}

			CompleteDelegate.ExecuteIfBound(Results);
		}
	};
	TSharedRef<FLoadContext> LoadContext = MakeShared<FLoadContext>();
	LoadContext->CompleteDelegate = CompleteDelegate;

	LoadContext->Results.Reserve(PluginURLs.Num());
	for (const FString& PluginURL : PluginURLs)
	{
		LoadContext->Results.Add(PluginURL, MakeError("Pending"));
	}

	const int32 NumPluginsToLoad = PluginURLs.Num();
	UE_LOG(LogGameFeatures, Log, TEXT("Transitioning (%s) %i GFPs"), *LexToString(TargetState), NumPluginsToLoad);

	for (const FString& PluginURL : PluginURLs)
	{
		ChangeGameFeatureTargetState(PluginURL, ProtocolOptions, TargetState, FGameFeaturePluginChangeStateComplete::CreateLambda([LoadContext, PluginURL, TargetState](const UE::GameFeatures::FResult& Result)
		{
			LoadContext->Results.Add(PluginURL, Result);
			++LoadContext->NumPluginsLoaded;
			UE_LOG(LogGameFeatures, VeryVerbose, TEXT("Finished Transitioning (%s) %i GFPs"), *LexToString(TargetState), LoadContext->NumPluginsLoaded);
		}));
	}
}

UE::GameFeatures::FResult UGameFeaturesSubsystem::UpdateGameFeatureProtocolOptions(const FString& PluginURL, const FGameFeatureProtocolOptions& NewOptions, bool* bOutDidUpdate /*= nullptr*/)
{
	UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL);
	return UpdateGameFeatureProtocolOptions(StateMachine, NewOptions, bOutDidUpdate);
}

UE::GameFeatures::FResult UGameFeaturesSubsystem::UpdateGameFeatureProtocolOptions(UGameFeaturePluginStateMachine* StateMachine, const FGameFeatureProtocolOptions& NewOptions, bool* bOutDidUpdate /*= nullptr*/)
{
	if (bOutDidUpdate)
	{
		*bOutDidUpdate = false;
	}

	if (!StateMachine)
	{
		return MakeError(UE::GameFeatures::SubsystemErrorNamespace + UE::GameFeatures::CommonErrorCodes::BadURL);
	}

	bool bUpdated = false;
	UE::GameFeatures::FResult Result = StateMachine->TryUpdatePluginProtocolOptions(NewOptions, bUpdated);
	if (bOutDidUpdate)
	{
		*bOutDidUpdate = bUpdated;
	}

	return Result;
}

bool UGameFeaturesSubsystem::ShouldUpdatePluginProtocolOptions(const UGameFeaturePluginStateMachine* StateMachine, const FGameFeatureProtocolOptions& NewOptions)
{
	if (NewOptions.HasSubtype<FNull>())
	{
		return false;
	}

	if (!StateMachine)
	{
		return false;
	}
	
	//Make sure our StateMachine isn't in terminal, don't want to update Terminal plugins
	if (TerminalGameFeaturePluginStateMachines.Contains(StateMachine) || (StateMachine->GetCurrentState() == EGameFeaturePluginState::Terminal))
	{
		return false;
	}

	if (StateMachine->GetProtocolOptions() == NewOptions)
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

bool UGameFeaturesSubsystem::GetGameFeaturePluginInstallPercent(TConstArrayView<FString> PluginURLs, float& Install_Percent) const
{
	float TotalInstallPercent = 0;
	int32 NumFound = 0;

	for (const FString& URL : PluginURLs)
	{
		float SingleInstallPercent = 0;
		if (GetGameFeaturePluginInstallPercent(URL, SingleInstallPercent))
		{
			TotalInstallPercent += SingleInstallPercent;
			++NumFound;
		}
	}

	if (NumFound > 0)
	{
		Install_Percent = TotalInstallPercent / NumFound;
		return true;
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

void UGameFeaturesSubsystem::UninstallGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginUninstallComplete& CompleteDelegate /*= FGameFeaturePluginUninstallComplete()*/)
{
	UninstallGameFeaturePlugin(PluginURL, FGameFeatureProtocolOptions(), CompleteDelegate);
}

void UGameFeaturesSubsystem::UninstallGameFeaturePlugin(const FString& PluginURL, const FGameFeatureProtocolOptions& InProtocolOptions, const FGameFeaturePluginUninstallComplete& CompleteDelegate /*= FGameFeaturePluginUninstallComplete()*/)
{
	// FindOrCreate so that we can make sure we uninstall data for plugins that were installed on a previous application run
	// but have not yet been requested on this application run and so are not yet in the plugin list but might have data on disk
	// to uninstall
	UGameFeaturePluginStateMachine* StateMachine = FindOrCreateGameFeaturePluginStateMachine(PluginURL, InProtocolOptions);
	check(StateMachine);

	// We may need to update our ProtocolOptions to force certain metadata changes to facilitate this uninstall
	FGameFeatureProtocolOptions ProtocolOptions = StateMachine->GetProtocolOptions();

	// InstallBundle Protocol GameFeatures may need to change their metadata to force this uninstall
	if (StateMachine->GetPluginIdentifier().GetPluginProtocol() == EGameFeaturePluginProtocol::InstallBundle)
	{
		// It's possible that ParseURL hasn't been called yet so setup options here if needed.
		if (!ProtocolOptions.HasSubtype<FInstallBundlePluginProtocolOptions>())
		{
			ensureMsgf(ProtocolOptions.HasSubtype<FNull>(), TEXT("Protocol options type is incorrect for URL %s"), *PluginURL);
			ProtocolOptions.SetSubtype<FInstallBundlePluginProtocolOptions>();
		}

		// Need to force on bUninstallBeforeTerminate if it wasn't already set to on in our Metadata
		FInstallBundlePluginProtocolOptions& InstallBundleOptions = ProtocolOptions.GetSubtype<FInstallBundlePluginProtocolOptions>();
		if (!InstallBundleOptions.bUninstallBeforeTerminate)
		{
			InstallBundleOptions.bUninstallBeforeTerminate = true;
		}
	}

	// Weird flow here because we need to do a few tasks asynchronously
	// 1) Update Protocol Options   -->   2) Call to set destination to Uninstall --> 3) After we get to Uninstall go to Terminate

	// FIRST:
	// If we need to update our ProtocolOptions, do that first before starting the Uninstall. This allows us to update
	// options that might be important on the way to Terminal if they are changed. EX: FInstallBundlePluginProtocolMetaData::bUninstallBeforeTerminate
	if (ShouldUpdatePluginProtocolOptions(StateMachine, ProtocolOptions))
	{
		const UE::GameFeatures::FResult Result = UpdateGameFeatureProtocolOptions(StateMachine, ProtocolOptions);
		if (Result.HasError())
		{
			CompleteDelegate.ExecuteIfBound(Result);
			return;
		}
	}

	// SECOND:
	// Kick off the Uninstall destination after updating our options if necessary
	ChangeGameFeatureDestination(StateMachine, ProtocolOptions, FGameFeaturePluginStateRange(EGameFeaturePluginState::Uninstalled),
		FGameFeaturePluginTerminateComplete::CreateWeakLambda(this, [this, PluginURL, CompleteDelegate](const UE::GameFeatures::FResult& Result)
		{
			// THIRD:
			// Kick off the actual Terminate after we successfully transition to Uninstalled state
			if (Result.HasValue())
			{
				TerminateGameFeaturePlugin(PluginURL, CompleteDelegate);
			}
			//If we failed just bubble error up
			else
			{
				CompleteDelegate.ExecuteIfBound(Result);
			}
		}));
}

void UGameFeaturesSubsystem::TerminateGameFeaturePlugin(const FString& PluginURL)
{
	TerminateGameFeaturePlugin(PluginURL, FGameFeaturePluginTerminateComplete());
}

void UGameFeaturesSubsystem::TerminateGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginTerminateComplete& CompleteDelegate)
{
	if (UGameFeaturePluginStateMachine* StateMachine = FindGameFeaturePluginStateMachine(PluginURL))
	{
		ChangeGameFeatureDestination(StateMachine, FGameFeaturePluginStateRange(EGameFeaturePluginState::Terminal), CompleteDelegate);
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

void UGameFeaturesSubsystem::CancelGameFeatureStateChange(TConstArrayView<FString> PluginURLs, const FMultipleGameFeaturePluginChangeStateComplete& CompleteDelegate)
{
	struct FContext
	{
		TMap<FString, UE::GameFeatures::FResult> Results;
		FMultipleGameFeaturePluginsLoaded CompleteDelegate;

		int32 NumPluginsCanceled = 0;

		~FContext()
		{
			CompleteDelegate.ExecuteIfBound(Results);
		}
	};
	TSharedRef<FContext> CancelContext = MakeShared<FContext>();
	CancelContext->CompleteDelegate = CompleteDelegate;

	CancelContext->Results.Reserve(PluginURLs.Num());
	for (const FString& PluginURL : PluginURLs)
	{
		CancelContext->Results.Add(PluginURL, MakeError("Pending"));
	}

	UE_LOG(LogGameFeatures, Log, TEXT("Canceling %i GFP transitions"), PluginURLs.Num());

	for (const FString& PluginURL : PluginURLs)
	{
		CancelGameFeatureStateChange(PluginURL, FGameFeaturePluginChangeStateComplete::CreateLambda([CancelContext, PluginURL](const UE::GameFeatures::FResult& Result)
		{
			CancelContext->Results.Add(PluginURL, Result);
			++CancelContext->NumPluginsCanceled;
			UE_LOG(LogGameFeatures, VeryVerbose, TEXT("Finished canceling %i GFP transitions"), CancelContext->NumPluginsCanceled);
		}));
	}
}

void UGameFeaturesSubsystem::LoadBuiltInGameFeaturePlugin(const TSharedRef<IPlugin>& Plugin, FBuiltInPluginAdditionalFilters AdditionalFilter, const FGameFeaturePluginLoadComplete& CompleteDelegate /*= FGameFeaturePluginLoadComplete()*/)
{
	UE_SCOPED_ENGINE_ACTIVITY(TEXT("Loading GameFeaturePlugin %s"), *Plugin->GetName());

	UAssetManager::Get().PushBulkScanning();

	FString PluginURL;
	FGameFeaturePluginDetails PluginDetails;
	if (GetBuiltInGameFeaturePluginDetails(Plugin, PluginURL, PluginDetails))
	{
		if (GameSpecificPolicies->IsPluginAllowed(PluginURL))
		{
			FBuiltInGameFeaturePluginBehaviorOptions BehaviorOptions;
			const bool bShouldProcess = AdditionalFilter(Plugin->GetDescriptorFileName(), PluginDetails, BehaviorOptions);
			if (bShouldProcess)
			{
				FGameFeatureProtocolOptions ProtocolOptions;
				ProtocolOptions.bForceSyncLoading = BehaviorOptions.bForceSyncLoading;
				ProtocolOptions.bLogWarningOnForcedDependencyCreation = BehaviorOptions.bLogWarningOnForcedDependencyCreation;
				ProtocolOptions.bLogErrorOnForcedDependencyCreation = BehaviorOptions.bLogErrorOnForcedDependencyCreation;
				UGameFeaturePluginStateMachine* StateMachine = FindOrCreateGameFeaturePluginStateMachine(PluginURL, ProtocolOptions);

				EBuiltInAutoState InitialAutoState = (BehaviorOptions.AutoStateOverride != EBuiltInAutoState::Invalid) ? 
					BehaviorOptions.AutoStateOverride : PluginDetails.BuiltInAutoState;
				if (InitialAutoState < EBuiltInAutoState::Registered && IsRunningCookCommandlet())
				{
					UE_LOG(LogGameFeatures, Display, TEXT("%s will be set to Registerd for cooking"), *Plugin->GetName());
					InitialAutoState = EBuiltInAutoState::Registered;
				}
				const EGameFeaturePluginState DestinationState = ConvertInitialFeatureStateToTargetState(InitialAutoState);

				StateMachine->SetWasLoadedAsBuiltIn();

				// If we're already at the destination or beyond, don't transition back
				FGameFeaturePluginStateRange Destination(DestinationState, EGameFeaturePluginState::Active);
				ChangeGameFeatureDestination(StateMachine, ProtocolOptions, Destination,
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
	FBuiltInPluginLoadTimeTracker()
	{
		StartTime = FPlatformTime::Seconds();
	}

	~FBuiltInPluginLoadTimeTracker()
	{
		double TotalLoadTime = FPlatformTime::Seconds() - StartTime;
		UE_LOG(LogGameFeatures, Display, TEXT("Total built in plugin load time %.4fs"), TotalLoadTime);

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
	double StartTime;
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

void UGameFeaturesSubsystem::LoadBuiltInGameFeaturePlugins(FBuiltInPluginAdditionalFilters AdditionalFilter, const FBuiltInGameFeaturePluginsLoaded& InCompleteDelegate /*= FBuiltInGameFeaturePluginsLoaded()*/)
{
	struct FLoadContext
	{
		FScopeLogTime ScopeLogTime{TEXT("BuiltInGameFeaturePlugins loaded."), nullptr, FConditionalScopeLogTime::ScopeLog_Seconds};

		TMap<FString, UE::GameFeatures::FResult> Results;
		FBuiltInGameFeaturePluginsLoaded CompleteDelegate;

		int32 NumPluginsLoaded = 0;
		bool bPushedTagsBroadcast = false;
		bool bPushedAssetBulkScanning = false;

		FLoadContext()
		{
			if (!IsEngineExitRequested())
			{
				UAssetManager::Get().PushBulkScanning();
				bPushedAssetBulkScanning = true;
				UGameplayTagsManager::Get().PushDeferOnGameplayTagTreeChangedBroadcast();
				bPushedTagsBroadcast = true;
			}
			else
			{
				if(UAssetManager* AssetManager =  UAssetManager::GetIfInitialized())
				{
					AssetManager->PushBulkScanning();
					bPushedAssetBulkScanning = true;
				}
				if(UGameplayTagsManager* TagsManager = UGameplayTagsManager::GetIfAllocated())
				{
					TagsManager->PushDeferOnGameplayTagTreeChangedBroadcast();
					bPushedTagsBroadcast = true;
				}
			}
		}

		~FLoadContext()
		{
			if (bPushedTagsBroadcast)
			{
				if (UGameplayTagsManager* TagsManager = UGameplayTagsManager::GetIfAllocated())
				{
					TagsManager->PopDeferOnGameplayTagTreeChangedBroadcast();
				}
			}
			if (bPushedAssetBulkScanning)
			{
				if(UAssetManager* AssetManager =  UAssetManager::GetIfInitialized())
				{
					AssetManager->PopBulkScanning();
				}
			}

			CompleteDelegate.ExecuteIfBound(Results);
		}
	};
	TSharedRef<FLoadContext> LoadContext = MakeShared<FLoadContext>();
	LoadContext->CompleteDelegate = InCompleteDelegate;

	FBuiltInPluginLoadTimeTracker PluginLoadTimeTracker;
	TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPlugins();

	LoadContext->Results.Reserve(EnabledPlugins.Num());
	for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins)
	{
		LoadContext->Results.Add(Plugin->GetName(), MakeError("Pending"));
	}

	const int32 NumPluginsToLoad = EnabledPlugins.Num();
	UE_LOG(LogGameFeatures, Log, TEXT("Loading %i builtins"), NumPluginsToLoad);

	// Sort the plugins so we can more accurately track how long it takes to load rather than have inconsistent dependency timings.
	TArray<TSharedRef<IPlugin>> Dependencies;
	auto GetPluginDependencies =
		[&Dependencies](TSharedRef<IPlugin> CurrentPlugin)
	{
		IPluginManager& PluginManager = IPluginManager::Get();
		Dependencies.Reset();
		
		const FPluginDescriptor& Desc = CurrentPlugin->GetDescriptor();
		for (const FPluginReferenceDescriptor& Dependency : Desc.Plugins)
		{
			if (Dependency.bEnabled)
			{
				if (TSharedPtr<IPlugin> FoundPlugin = PluginManager.FindEnabledPlugin(Dependency.Name))
				{
					Dependencies.Add(FoundPlugin.ToSharedRef());
				}
			}
		}
		return Dependencies;
	};
	Algo::TopologicalSort(EnabledPlugins, GetPluginDependencies);

	for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins)
	{
		FBuiltInPluginLoadTimeTrackerScope TrackerScope(PluginLoadTimeTracker, Plugin);
		LoadBuiltInGameFeaturePlugin(Plugin, AdditionalFilter, FGameFeaturePluginLoadComplete::CreateLambda([LoadContext, Plugin](const UE::GameFeatures::FResult& Result)
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
	return GetBuiltInGameFeaturePluginDetails(Plugin, OutPluginURL, OutPluginDetails);
}

bool UGameFeaturesSubsystem::GetBuiltInGameFeaturePluginDetails(const TSharedRef<IPlugin>& Plugin, FString& OutPluginURL, FGameFeaturePluginDetails& OutPluginDetails) const
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
			return GetGameFeaturePluginDetailsInternal(PluginDescriptorFilename, OutPluginDetails);
		}
	}

	return false;
}

bool UGameFeaturesSubsystem::GetGameFeaturePluginDetails(FString PluginURL, FGameFeaturePluginDetails& OutPluginDetails) const
{
	FStringView PluginPath;
	if (UGameFeaturesSubsystem::ParsePluginURL(PluginURL, nullptr, &PluginPath))
	{
		return GetGameFeaturePluginDetailsInternal(FString(PluginPath), OutPluginDetails);
	}

	return false;
}

bool UGameFeaturesSubsystem::GetGameFeaturePluginDetailsInternal(const FString& PluginDescriptorFilename, FGameFeaturePluginDetails& OutPluginDetails) const
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
		TSharedPtr<FJsonValue> Field = ObjectPtr->TryGetField(ExtraKey);
		if (Field.IsValid())
		{
			OutPluginDetails.AdditionalMetadata.Add(ExtraKey, Field);
		}
		else
		{
			OutPluginDetails.AdditionalMetadata.Add(ExtraKey, MakeShared<FJsonValueString>(TEXT("")));
		}
	}

	// Parse plugin dependencies
	const TArray<TSharedPtr<FJsonValue>>* PluginsArray = nullptr;
	ObjectPtr->TryGetArrayField(TEXT("Plugins"), PluginsArray);
	if (PluginsArray)
	{
		FString NameField = TEXT("Name");
		FString EnabledField = TEXT("Enabled");
		FString ActivateField = TEXT("Activate");
		for (const TSharedPtr<FJsonValue>& PluginElement : *PluginsArray)
		{
			if (PluginElement.IsValid())
			{
				const TSharedPtr<FJsonObject>* ElementObjectPtr = nullptr;
				PluginElement->TryGetObject(ElementObjectPtr);
				if (ElementObjectPtr && ElementObjectPtr->IsValid())
				{
					const TSharedPtr<FJsonObject>& ElementObject = *ElementObjectPtr;

					FString DependencyName;
					ElementObject->TryGetStringField(NameField, DependencyName);
					if (!DependencyName.IsEmpty())
					{
						bool bElementEnabled = false;
						ElementObject->TryGetBoolField(EnabledField, bElementEnabled);
						if (bElementEnabled)
						{
							//Have to get Activate from JSON as it's unique to GFP and not in the PluginManager
							bool bElementActivate = false;
							ElementObject->TryGetBoolField(ActivateField, bElementActivate);

							OutPluginDetails.PluginDependencies.Emplace(FGameFeaturePluginReferenceDetails(MoveTemp(DependencyName), bElementActivate));
						}
						else
						{
							UE_LOG(LogGameFeatures, VeryVerbose, TEXT("Skipping adding dependency %s in %s. Plugin is disabled."), *DependencyName, *PluginDescriptorFilename);
						}
					}
					else
					{
						UE_LOG(LogGameFeatures, Error, TEXT("Error parsing dependency name in %s! Invalid JSON data!"), *PluginDescriptorFilename);
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

struct FGameFeaturePluginPredownloadContext : public FGameFeaturePluginPredownloadHandle
{
	const FStringView PredownloadErrorNamespace = TEXTVIEW("GameFeaturePlugin.Predownload.");
	TMap<FGameFeaturePluginIdentifier, FInstallBundlePluginProtocolMetaData> GFPs;
	TArray<FName> PendingBundleDownloads;
	TOptional<FInstallBundleCombinedProgressTracker> ProgressTracker;
	TUniqueFunction<void(const UE::GameFeatures::FResult&)> OnComplete;
	TUniqueFunction<void(float)> OnProgress;
	UE::GameFeatures::FResult Result = MakeValue();
	float Progress = 0.0f;
	bool bIsComplete = false;
	bool bCanceled = false;

	virtual ~FGameFeaturePluginPredownloadContext() override
	{
		Cleanup();
	}

	virtual bool IsComplete() const override
	{
		return bIsComplete;
	}

	virtual const UE::GameFeatures::FResult& GetResult() const override
	{
		return Result;
	}

	virtual float GetProgress() const override
	{
		return Progress;
	}

	virtual void Cancel() override
	{
		bCanceled = true;

		if (PendingBundleDownloads.Num() > 0)
		{
			TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
			if (BundleManager)
			{
				BundleManager->CancelUpdateContent(PendingBundleDownloads);
			}
		}
	}

	void Cleanup()
	{
		ProgressTracker.Reset();
		IInstallBundleManager::InstallBundleCompleteDelegate.RemoveAll(this);
		IInstallBundleManager::PausedBundleDelegate.RemoveAll(this);
	}

	void SetComplete()
	{
		bIsComplete = true;
		if (OnComplete)
		{
			OnComplete(Result);
		}
	}

	void SetComplete(UE::GameFeatures::FResult&& InResult)
	{
		Result = MoveTemp(InResult);
		bIsComplete = true;
		if (OnComplete)
		{
			OnComplete(Result);
		}
	}

	void SetCompleteCanceled()
	{
		Result = MakeError(FString::Printf(TEXT("%.*s%s"),
			PredownloadErrorNamespace.Len(), PredownloadErrorNamespace.GetData(),
			TEXT("Canceled")));
		bIsComplete = true;
		if (OnComplete)
		{
			OnComplete(Result);
		}
	}

	void Start(TConstArrayView<FString> PluginURLs)
	{
		if (bCanceled)
		{
			SetCompleteCanceled();
			return;
		}

		for (const FString& URL : PluginURLs)
		{
			if (UGameFeaturesSubsystem::GetPluginURLProtocol(URL) != EGameFeaturePluginProtocol::InstallBundle)
			{
				// Only support install bundle protocol for downloading right now
				continue;
			}

			TValueOrError<FInstallBundlePluginProtocolMetaData, void> MaybeInstallBundleOptions = FInstallBundlePluginProtocolMetaData::FromString(URL);
			if (MaybeInstallBundleOptions.HasError())
			{
				UE_LOGFMT(LogGameFeatures, Error, "GFP Predownload failed to parse URL {URL}", ("URL", URL));
				UE::GameFeatures::FResult ErrorResult = MakeError(FString::Printf(TEXT("%.*s%s"),
					PredownloadErrorNamespace.Len(), PredownloadErrorNamespace.GetData(),
					TEXT("BadUrl")));
				SetComplete(MoveTemp(ErrorResult));
				return;
			}

			GFPs.Emplace(URL, MaybeInstallBundleOptions.StealValue());
		}

		if (GFPs.Num() == 0)
		{
			SetComplete(MakeValue());
			return;
		}

		UGameFeaturesSubsystem& GFPSubSys = UGameFeaturesSubsystem::Get();

		TArray<FName> BundlesToInstall;
		for (const TPair<FGameFeaturePluginIdentifier, FInstallBundlePluginProtocolMetaData>& Pair : GFPs)
		{
			UGameFeaturePluginStateMachine* Machine = GFPSubSys.FindGameFeaturePluginStateMachine(Pair.Key);
			if (Machine && Machine->GetDestination() < EGameFeaturePluginState::Installed)
			{
				// Existing machine exists and wants to be uninstalled, can't precache
				UE_LOGFMT(LogGameFeatures, Error, "GFP Predownload failed because a GFP is unloading, GFP: {GFP}", ("GFP", Machine->GetPluginName()));
				UE::GameFeatures::FResult ErrorResult = MakeError(FString::Printf(TEXT("%.*s%s"),
					PredownloadErrorNamespace.Len(), PredownloadErrorNamespace.GetData(),
					TEXT("GFPUnloading")));
				SetComplete(MoveTemp(ErrorResult));
				return;
			}

			BundlesToInstall.Append(Pair.Value.InstallBundles);

			GFPSubSys.OnGameFeaturePredownloading(FString(Pair.Key.GetPluginName()), Pair.Key);
		}

		TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
		if (!BundleManager)
		{
			UE_LOGFMT(LogGameFeatures, Error, "GFP Predownload failed, no Install Bundle Manager found.");
			UE::GameFeatures::FResult ErrorResult = MakeError(FString::Printf(TEXT("%.*s%s"),
				PredownloadErrorNamespace.Len(), PredownloadErrorNamespace.GetData(),
				TEXT("BundleManager_Null")));
			SetComplete(MoveTemp(ErrorResult));
			return;
		}

		// Early out if everything is up to date already. This helps avoid enquing UI dialogs for content that doesn't actually need to be downloaded
		TValueOrError<FInstallBundleCombinedInstallState, EInstallBundleResult> MaybeInstallState = BundleManager->GetInstallStateSynchronous(BundlesToInstall, false);
		if (MaybeInstallState.HasValue() && MaybeInstallState.GetValue().GetAllBundlesHaveState(EInstallBundleInstallState::UpToDate))
		{
			SetComplete(MakeValue());
			return;
		}

		BundleManager->GetContentState(BundlesToInstall, EInstallBundleGetContentStateFlags::None, false,
			FInstallBundleGetContentStateDelegate::CreateLambda([Context = SharedThis(this)](FInstallBundleCombinedContentState BundleContentState)
			{ Context->OnGotContentState(MoveTemp(BundleContentState)); }));
	}

	void OnGotContentState(FInstallBundleCombinedContentState BundleContentState)
	{
		TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();

		if (BundleContentState.GetAllBundlesHaveState(EInstallBundleInstallState::UpToDate))
		{
			SetComplete(MakeValue());
			return;
		}

		if (bCanceled)
		{
			SetCompleteCanceled();
			return;
		}

		TArray<FName> BundlesToInstall;
		for (const TPair<FGameFeaturePluginIdentifier, FInstallBundlePluginProtocolMetaData>& Pair : GFPs)
		{
			BundlesToInstall.Append(Pair.Value.InstallBundles);
		}

		EInstallBundleRequestFlags InstallFlags = EInstallBundleRequestFlags::Defaults | EInstallBundleRequestFlags::SkipMount;
		TValueOrError<FInstallBundleRequestInfo, EInstallBundleResult> MaybeRequestInfo = BundleManager->RequestUpdateContent(BundlesToInstall, InstallFlags);

		if (MaybeRequestInfo.HasError())
		{
			UE_LOGFMT(LogGameFeatures, Error, "GFP Predownload failed to request content, Error: {Error}", ("Error", LexToString(MaybeRequestInfo.GetError())));
			UE::GameFeatures::FResult ErrorResult = MakeError(FString::Printf(TEXT("%.*s%s"),
				PredownloadErrorNamespace.Len(), PredownloadErrorNamespace.GetData(),
				LexToString(MaybeRequestInfo.GetError())));
			SetComplete(MoveTemp(ErrorResult));
			return;
		}

		FInstallBundleRequestInfo RequestInfo = MaybeRequestInfo.StealValue();
		if (RequestInfo.BundlesEnqueued.Num() == 0)
		{
			SetComplete(MakeValue());
			return;
		}

		PendingBundleDownloads = MoveTemp(RequestInfo.BundlesEnqueued);

		ProgressTracker.Emplace(true, [this](const FInstallBundleCombinedProgressTracker::FCombinedProgress& InProgress)
		{
			Progress = InProgress.ProgressPercent;
			if (OnProgress)
			{
				OnProgress(InProgress.ProgressPercent);
			}
		});
		ProgressTracker->SetBundlesToTrackFromContentState(BundleContentState, PendingBundleDownloads);

		IInstallBundleManager::InstallBundleCompleteDelegate.AddLambda(
			[Context = SharedThis(this)](FInstallBundleRequestResultInfo BundleResult)
			{ Context->OnInstallBundleCompleted(MoveTemp(BundleResult)); });
		// TODO: handle pause?  Just cancel? This should only be relevent for cell connections
		// IInstallBundleManager::PausedBundleDelegate.AddRaw(this, &FGameFeaturePluginState_Downloading::OnInstallBundlePaused);
	}

	void OnInstallBundleCompleted(FInstallBundleRequestResultInfo BundleResult)
	{
		if (!PendingBundleDownloads.Contains(BundleResult.BundleName))
		{
			return;
		}

		PendingBundleDownloads.Remove(BundleResult.BundleName);

		if (Result.HasValue() && BundleResult.Result != EInstallBundleResult::OK)
		{
			if (BundleResult.OptionalErrorCode.IsEmpty())
			{
				UE_LOGFMT(LogGameFeatures, Error, "GFP Predownload failed to install {Bundle}, Error: {Error}",
					("Bundle", BundleResult.BundleName), ("Error", LexToString(BundleResult.Result)));
			}
			else
			{
				UE_LOGFMT(LogGameFeatures, Error, "GFP Predownload failed to install {Bundle}, Error: {Error}",
					("Bundle", BundleResult.BundleName), ("Error", BundleResult.OptionalErrorCode));
			}

			//Use OptionalErrorCode and/or OptionalErrorText if available
			const FString ErrorCodeEnding = (BundleResult.OptionalErrorCode.IsEmpty()) ? LexToString(BundleResult.Result) : BundleResult.OptionalErrorCode;
			FText ErrorText = BundleResult.OptionalErrorCode.IsEmpty() ? UE::GameFeatures::CommonErrorCodes::GetErrorTextForBundleResult(BundleResult.Result) : BundleResult.OptionalErrorText;
			Result = UE::GameFeatures::FResult(
				MakeError(FString::Printf(TEXT("%.*s%s"), PredownloadErrorNamespace.Len(), PredownloadErrorNamespace.GetData(), *ErrorCodeEnding)),
				MoveTemp(ErrorText)
			);

			// Cancel remaining downloads
			TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
			BundleManager->CancelUpdateContent(PendingBundleDownloads);
		}

		if (PendingBundleDownloads.Num() > 0)
		{
			return;
		}

		// Delay call to ReleaseBundlesIfPossible. We don't want to release them from within the complete callback.
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([Context = SharedThis(this)](float)
			{
				Context->ReleaseBundlesIfPossible();

				// Done
				Context->SetComplete();

				Context->Cleanup();
				return false;
			})
		);
	}

	// Predownload shouldn't pin any cached bundles so release them now
	void ReleaseBundlesIfPossible()
	{
		UGameFeaturesSubsystem& GFPSubSys = UGameFeaturesSubsystem::Get();
		TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();

		TArray<FName> ReleaseList;
		TArray<FName> KeepList;
		for (const TPair<FGameFeaturePluginIdentifier, FInstallBundlePluginProtocolMetaData>& Pair : GFPs)
		{
			UGameFeaturePluginStateMachine* Machine = GFPSubSys.FindGameFeaturePluginStateMachine(Pair.Key);
			if (Machine &&
				Machine->GetCurrentState() > EGameFeaturePluginState::StatusKnown &&
				Machine->GetCurrentState() != EGameFeaturePluginState::Releasing)
			{
				// A machine is using the bundles, don't release
				KeepList.Append(Pair.Value.InstallBundles);
				continue;
			}

			ReleaseList.Append(Pair.Value.InstallBundles);
		}

		BundleManager->RequestReleaseContent(ReleaseList, EInstallBundleReleaseRequestFlags::None, KeepList);
	}
};

TSharedRef<FGameFeaturePluginPredownloadHandle> UGameFeaturesSubsystem::PredownloadGameFeaturePlugins(TConstArrayView<FString> PluginURLs, TUniqueFunction<void(const UE::GameFeatures::FResult&)> OnComplete /*= nullptr*/, TUniqueFunction<void(float)> OnProgress /*= nullptr*/)
{
	TSharedRef<FGameFeaturePluginPredownloadContext> Context = MakeShared<FGameFeaturePluginPredownloadContext>();
	Context->OnComplete = MoveTemp(OnComplete);
	Context->OnProgress = MoveTemp(OnProgress);
	Context->Start(PluginURLs);

	return Context;
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
	const FStringView ShortUrl = PluginIdentifier.GetIdentifyingString();

	TObjectPtr<UGameFeaturePluginStateMachine> const* ExistingStateMachine = 
		GameFeaturePluginStateMachines.FindByHash(GetTypeHash(PluginIdentifier.GetIdentifyingString()), PluginIdentifier.GetIdentifyingString());
	if (ExistingStateMachine)
	{
		EGameFeaturePluginProtocol ExpectedProtocol = (*ExistingStateMachine)->GetPluginIdentifier().GetPluginProtocol();
		if (ensureMsgf(ExpectedProtocol == PluginIdentifier.GetPluginProtocol(), TEXT("Expected protocol %s for %.*s"), UE::GameFeatures::GameFeaturePluginProtocolPrefix(ExpectedProtocol), ShortUrl.Len(), ShortUrl.GetData()))
		{
			UE_LOG(LogGameFeatures, VeryVerbose, TEXT("FOUND GameFeaturePlugin using PluginIdentifier:%.*s for PluginURL:%s"), ShortUrl.Len(), ShortUrl.GetData(), *PluginIdentifier.GetFullPluginURL());
			return *ExistingStateMachine;
		}
	}
	UE_LOG(LogGameFeatures, VeryVerbose, TEXT("NOT FOUND GameFeaturePlugin using PluginIdentifier:%.*s for PluginURL:%s"), ShortUrl.Len(), ShortUrl.GetData(), *PluginIdentifier.GetFullPluginURL());

	return nullptr;
}

// Note: ProtocolOptions is not defaulted here. Any API call that could create a state machine should allow the user to pass ProtocolOptions to initialize the machine.
// It is acceptable that user passes null options. 
UGameFeaturePluginStateMachine* UGameFeaturesSubsystem::FindOrCreateGameFeaturePluginStateMachine(const FString& PluginURL, const FGameFeatureProtocolOptions& ProtocolOptions, bool* bOutFoundExisting /*= nullptr*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GFP_FindOrCreateStateMachine);
	FGameFeaturePluginIdentifier PluginIdentifier(PluginURL);
	TObjectPtr<UGameFeaturePluginStateMachine> const* ExistingStateMachine =
		GameFeaturePluginStateMachines.FindByHash(GetTypeHash(PluginIdentifier.GetIdentifyingString()), PluginIdentifier.GetIdentifyingString());

	if (bOutFoundExisting)
	{
		*bOutFoundExisting = !!ExistingStateMachine;
	}

	if (ExistingStateMachine)
	{
		// In this case, still return the existing machine, even if the protocol doesn't match. This function should never return null.
		// There can only be one active instance of any machine.
		EGameFeaturePluginProtocol ExpectedProtocol = (*ExistingStateMachine)->GetPluginIdentifier().GetPluginProtocol();
		ensureAlwaysMsgf(ExpectedProtocol == PluginIdentifier.GetPluginProtocol(), TEXT("Expected protocol %s for %.*s"), UE::GameFeatures::GameFeaturePluginProtocolPrefix(ExpectedProtocol), PluginIdentifier.GetIdentifyingString().Len(), PluginIdentifier.GetIdentifyingString().GetData());

		UE_LOG(LogGameFeatures, VeryVerbose, TEXT("Found GameFeaturePlugin StateMachine using Identifier:%.*s from PluginURL:%s"), PluginIdentifier.GetIdentifyingString().Len(), PluginIdentifier.GetIdentifyingString().GetData(), *PluginURL);
		return *ExistingStateMachine;
	}

	UE_LOG(LogGameFeatures, Display, TEXT("Creating GameFeaturePlugin StateMachine using Identifier:%.*s from PluginURL:%s"), PluginIdentifier.GetIdentifyingString().Len(), PluginIdentifier.GetIdentifyingString().GetData(), *PluginURL);

	UGameFeaturePluginStateMachine* NewStateMachine = NewObject<UGameFeaturePluginStateMachine>(this);
	GameFeaturePluginStateMachines.Add(FString(PluginIdentifier.GetIdentifyingString()), NewStateMachine);
	NewStateMachine->InitStateMachine(MoveTemp(PluginIdentifier), ProtocolOptions);

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
	ChangeGameFeatureDestination(Machine, FGameFeatureProtocolOptions(), StateRange, CompleteDelegate);
}

void UGameFeaturesSubsystem::ChangeGameFeatureDestination(UGameFeaturePluginStateMachine* Machine, const FGameFeatureProtocolOptions& InProtocolOptions, const FGameFeaturePluginStateRange& StateRange, FGameFeaturePluginChangeStateComplete CompleteDelegate)
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
		auto OnCanceled = [this, InProtocolOptions, StateRange, CompleteDelegate](UGameFeaturePluginStateMachine* Machine) mutable
		{
			// Special case for terminal state since it cannot be exited, we need to make a new machine
			if (Machine->GetCurrentState() == EGameFeaturePluginState::Terminal)
			{
				UGameFeaturePluginStateMachine* NewMachine = FindOrCreateGameFeaturePluginStateMachine(Machine->GetPluginURL(), InProtocolOptions);
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

bool UGameFeaturesSubsystem::FindOrCreatePluginDependencyStateMachines(const FString& PluginURL, const FGameFeaturePluginStateMachineProperties& InStateProperties, TArray<UGameFeaturePluginStateMachine*>& OutDependencyMachines)
{
	const FString& PluginFilename = InStateProperties.PluginInstalledFilename;
	const FGameFeatureProtocolOptions InDepProtocolOptions = InStateProperties.RecycleProtocolOptions();

	const bool bWarnOnDepCreation = InStateProperties.ProtocolOptions.bLogWarningOnForcedDependencyCreation;
	const bool bErrorOnDepCreation = InStateProperties.ProtocolOptions.bLogErrorOnForcedDependencyCreation;

	FGameFeaturePluginDetails Details;
	if (GetGameFeaturePluginDetailsInternal(PluginFilename, Details))
	{
		for (const FGameFeaturePluginReferenceDetails& PluginDependency : Details.PluginDependencies)
		{
			const FString& DependencyName = PluginDependency.PluginName;
			TValueOrError<FString, FString> DependencyURLInfo = GameSpecificPolicies->ResolvePluginDependency(PluginURL, DependencyName);
			if (DependencyURLInfo.HasError())
			{
				UE_LOG(LogGameFeatures, Error, TEXT("Game feature plugin '%s' has unknown dependency '%s' [%s]."), *PluginFilename, *DependencyName, *DependencyURLInfo.GetError());

				//Don't actually return false here as we want to still be able to progress in the case of 
				//things like an editor plugin being included as a dependency in the client or a dynamic dependency that
				//hasn't correctly loaded yet
				continue;
			}

			const FString& DependencyURL = DependencyURLInfo.GetValue();

			// Dependency may not be a GFP and so will have an empty URL but not have an error
			if (DependencyURL.IsEmpty())
			{
				continue;
			}

			// Inherit dep protocol options if possible
			FGameFeatureProtocolOptions DepProtocolOptions;
			EGameFeaturePluginProtocol DepProtocol = UGameFeaturesSubsystem::GetPluginURLProtocol(DependencyURL);
			if (DepProtocol == EGameFeaturePluginProtocol::InstallBundle && InDepProtocolOptions.HasSubtype<FInstallBundlePluginProtocolOptions>())
			{
				DepProtocolOptions = InDepProtocolOptions;
			}
			else
			{
				// Always propogate non-protocol specific flags
				DepProtocolOptions.bForceSyncLoading = InDepProtocolOptions.bForceSyncLoading;
				DepProtocolOptions.bLogWarningOnForcedDependencyCreation = InDepProtocolOptions.bLogWarningOnForcedDependencyCreation;
				DepProtocolOptions.bLogErrorOnForcedDependencyCreation = InDepProtocolOptions.bLogErrorOnForcedDependencyCreation;
			}

			bool bFoundExisting = false;
			UGameFeaturePluginStateMachine* ResolvedDependency = FindOrCreateGameFeaturePluginStateMachine(DependencyURL, DepProtocolOptions, &bFoundExisting);
			check(ResolvedDependency);

			if (!bFoundExisting)
			{
				// Propogate bWasLoadedAsBuiltInGameFeaturePlugin
				if (InStateProperties.bWasLoadedAsBuiltInGameFeaturePlugin)
				{
					ResolvedDependency->SetWasLoadedAsBuiltIn();
				}

				// Note: Given that LoadBuiltInGameFeaturePlugins does a topological sort, we don't expect to hit this path for built-ins
				if (bWarnOnDepCreation)
				{
					if (InStateProperties.bWasLoadedAsBuiltInGameFeaturePlugin)
					{
						UE_LOGFMT(LogGameFeatures, Warning, "GFP dependency {Dep} was forcibly created by {Parent}, Game specific policies may be incorrectly filtering this dependency.",
							("Dep", ResolvedDependency->GetPluginIdentifier().GetIdentifyingString()), ("Parent", InStateProperties.PluginIdentifier.GetIdentifyingString()));
					}
					else
					{
						UE_LOGFMT(LogGameFeatures, Warning, "GFP dependency {Dep} was unexpectedly forcibly created by {Parent}",
							("Dep", ResolvedDependency->GetPluginIdentifier().GetIdentifyingString()), ("Parent", InStateProperties.PluginIdentifier.GetIdentifyingString()));
					}
				}
				else if (bErrorOnDepCreation)
				{
					if (InStateProperties.bWasLoadedAsBuiltInGameFeaturePlugin)
					{
						UE_LOGFMT(LogGameFeatures, Error, "GFP dependency {Dep} was forcibly created by {Parent}, Game specific policies may be incorrectly filtering this dependency.",
							("Dep", ResolvedDependency->GetPluginIdentifier().GetIdentifyingString()), ("Parent", InStateProperties.PluginIdentifier.GetIdentifyingString()));
					}
					else
					{
						UE_LOGFMT(LogGameFeatures, Error, "GFP dependency {Dep} was unexpectedly forcibly created by {Parent}",
							("Dep", ResolvedDependency->GetPluginIdentifier().GetIdentifyingString()), ("Parent", InStateProperties.PluginIdentifier.GetIdentifyingString()));
					}
				}
			}

			OutDependencyMachines.Add(ResolvedDependency);
		}

		return true;
	}

	return false;
}

bool UGameFeaturesSubsystem::FindPluginDependencyStateMachinesToActivate(const FString& PluginURL, const FString& PluginFilename, TArray<UGameFeaturePluginStateMachine*>& OutDependencyMachines) const
{
	return EnumeratePluginDependenciesWithShouldActivate(PluginURL, PluginFilename, [this, &OutDependencyMachines](const FString& DependencyName, const FString& DependencyURL) {
		UGameFeaturePluginStateMachine* Dependency = FindGameFeaturePluginStateMachine(DependencyURL);
		if (Dependency)
		{
			OutDependencyMachines.Add(Dependency);
			return true;
		}
		//Expect to find all valid dependencies and activate them, so error if not found
		else
		{
			UE_LOG(LogGameFeatures, Error, TEXT("FindPluginDependencyStateMachinesToActivate failed to find plugin state machine for %s using URL %s"), *DependencyName, *DependencyURL);
			return false;
		}
	});
}

bool UGameFeaturesSubsystem::FindPluginDependencyStateMachinesToDeactivate(const FString& PluginURL, const FString& PluginFilename, TArray<UGameFeaturePluginStateMachine*>& OutDependencyMachines) const
{
	return EnumeratePluginDependenciesWithShouldActivate(PluginURL, PluginFilename, [this, &OutDependencyMachines](const FString& DependencyName, const FString& DependencyURL) {
		UGameFeaturePluginStateMachine* Dependency = FindGameFeaturePluginStateMachine(DependencyURL);
		if (Dependency)
		{
			OutDependencyMachines.Add(Dependency);
		}
		else
		{
			// Depenedency may have been fully terminated which is considered deactivated already.
			UE_LOG(LogGameFeatures, Log, TEXT("FindPluginDependencyStateMachinesToDeactivate unable to find plugin state machine for %s using URL %s"), *DependencyName, *DependencyURL);
		}
		return true;
	});
}

template <typename CallableT>
bool UGameFeaturesSubsystem::EnumeratePluginDependenciesWithShouldActivate(const FString& PluginURL, const FString& PluginFilename, CallableT Callable) const
{
	FGameFeaturePluginDetails Details;
	if (GetGameFeaturePluginDetailsInternal(PluginFilename, Details))
	{
		for (const FGameFeaturePluginReferenceDetails& PluginDependency : Details.PluginDependencies)
		{
			if (PluginDependency.bShouldActivate)
			{
				const FString& DependencyName = PluginDependency.PluginName;
				TValueOrError<FString, FString> DependencyURLInfo = GameSpecificPolicies->ResolvePluginDependency(PluginURL, DependencyName);
				if (DependencyURLInfo.HasError())
				{
					UE_LOG(LogGameFeatures, Error, TEXT("Failure to resolve dependency %s [%s] for parent plugin url: %s"), *DependencyName, *DependencyURLInfo.GetError(), *PluginURL);
					return false;
				}

				const FString& DependencyURL = DependencyURLInfo.GetValue();

				// Dependency may not be a GFP and so will have an empty URL but not have an error
				if (DependencyURL.IsEmpty())
				{
					continue;
				}

				if (!Callable(DependencyName, DependencyURL))
				{
					return false;
				}
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}

void UGameFeaturesSubsystem::ListGameFeaturePlugins(const TArray<FString>& Args, UWorld* InWorld, FOutputDevice& Ar)
{
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

	// Alphasort
	StateMachines.Sort([](const UGameFeaturePluginStateMachine& A, const UGameFeaturePluginStateMachine& B) { return A.GetGameFeatureName().Compare(B.GetGameFeatureName()) < 0; });

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

void UGameFeaturesSubsystem::CallbackObservers(EObserverCallback CallbackType, const FGameFeaturePluginIdentifier& PluginIdentifier,
	const FString* PluginName /*= nullptr*/, 
	const UGameFeatureData* GameFeatureData /*= nullptr*/, 
	FGameFeatureStateChangeContext* StateChangeContext /*= nullptr*/)
{
	static_assert(std::underlying_type<EObserverCallback>::type(EObserverCallback::Count) == 14, "Update UGameFeaturesSubsystem::CallbackObservers to handle added EObserverCallback");

	// Protect against modifying the observer list during iteration
	TArray<UObject*> LocalObservers(Observers);

	switch (CallbackType)
	{
	case EObserverCallback::CheckingStatus:
	{
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureCheckingStatus(PluginIdentifier.GetFullPluginURL());
		}
		break;
	}
	case EObserverCallback::Terminating:
	{
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureTerminating(PluginIdentifier.GetFullPluginURL());
		}
		break;
	}
	case EObserverCallback::Predownloading:
	{
		check(PluginName);
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeaturePredownloading(*PluginName, PluginIdentifier);
		}
		break;
	}
	case EObserverCallback::Downloading:
	{
		check(PluginName);
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureDownloading(*PluginName, PluginIdentifier);
		}
		break;
	}
	case EObserverCallback::Releasing:
	{
		check(PluginName);
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureReleasing(*PluginName, PluginIdentifier);
		}
		break;
	}
	case EObserverCallback::PreMounting:
	{
		check(PluginName);
		FGameFeaturePreMountingContext* PreMountingContext = static_cast<FGameFeaturePreMountingContext*>(StateChangeContext);
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeaturePreMounting(*PluginName, PluginIdentifier, *PreMountingContext);
		}
		break;
	}
	case EObserverCallback::PostMounting:
	{
		check(PluginName);
		FGameFeaturePostMountingContext* PostMountingContext = static_cast<FGameFeaturePostMountingContext*>(StateChangeContext);
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeaturePostMounting(*PluginName, PluginIdentifier, *PostMountingContext);
		}
		break;
	}
	case EObserverCallback::Registering:
	{
		check(PluginName);
		check(GameFeatureData);
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureRegistering(GameFeatureData, *PluginName, PluginIdentifier.GetFullPluginURL());
		}
		break;
	}
	case EObserverCallback::Unregistering:
	{
		check(PluginName);
#if !WITH_EDITOR
		// In the editor the GameFeatureData asset can be force deleted, otherwise it should exist
		check(GameFeatureData);
#endif
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureUnregistering(GameFeatureData, *PluginName, PluginIdentifier.GetFullPluginURL());
		}
		break;
	}
	case EObserverCallback::Loading:
	{
		check(GameFeatureData);
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureLoading(GameFeatureData, PluginIdentifier.GetFullPluginURL());
		}
		break;
	}
	case EObserverCallback::Unloading:
	{
#if !WITH_EDITOR
		// In the editor the GameFeatureData asset can be force deleted, otherwise it should exist
		check(GameFeatureData);
#endif
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureUnloading(GameFeatureData, PluginIdentifier.GetFullPluginURL());
		}
		break;
	}
	case EObserverCallback::Activating:
	{
		check(GameFeatureData);
		for (UObject* Observer : LocalObservers)
		{
			CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureActivating(GameFeatureData, PluginIdentifier.GetFullPluginURL());
		}
		break;
	}
	case EObserverCallback::Deactivating:
	{
#if !WITH_EDITOR
		// In the editor the GameFeatureData asset can be force deleted, otherwise it should exist
		check(GameFeatureData);
#endif
		check(StateChangeContext);
		FGameFeatureDeactivatingContext* DeactivatingContext = static_cast<FGameFeatureDeactivatingContext*>(StateChangeContext);
		if (ensureAlwaysMsgf(DeactivatingContext, TEXT("Invalid StateChangeContext supplied! Could not cast to FGameFeaturePauseStateChangeContext*!")))
		{
			for (UObject* Observer : LocalObservers)
			{
				CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeatureDeactivating(GameFeatureData, *DeactivatingContext, PluginIdentifier.GetFullPluginURL());
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
				CastChecked<IGameFeatureStateChangeObserver>(Observer)->OnGameFeaturePauseChange(PluginIdentifier.GetFullPluginURL(), *PluginName, *PauseChangeContext);
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
	static bool IsContentWithinActivePlugin(const FString& InObjectOrPackagePath, const TSet<FString>& ActivePluginNames)
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

void UGameFeaturesSubsystem::GetPluginsToCook(TSet<FString>& OutPlugins)
{
	// Command line parameter -CookPlugins.
	static TArray<FString> PluginsList = []()
	{
		TArray<FString> ReturnList;
		FString CookPluginsStr;
		if (FParse::Value(FCommandLine::Get(), TEXT("CookPlugins="), CookPluginsStr, false))
		{
			CookPluginsStr.ParseIntoArray(ReturnList, TEXT(","));
		}

		return ReturnList;
	}();
	
	OutPlugins.Append(PluginsList);
}

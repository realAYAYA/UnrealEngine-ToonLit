// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "GameFeaturePluginOperationResult.h"
#include "Engine/Engine.h"
#include "GameFeatureTypesFwd.h"

#include "GameFeaturesSubsystem.generated.h"

class UGameFeaturePluginStateMachine;
class IGameFeatureStateChangeObserver;
struct FStreamableHandle;
struct FAssetIdentifier;
class UGameFeatureData;
class UGameFeaturesProjectPolicies;
class IPlugin;
class FJsonObject;
struct FWorldContext;
struct FGameFeaturePluginStateRange;
enum class EInstallBundleRequestFlags : uint32;
enum class EInstallBundleReleaseRequestFlags : uint32;

/** Holds static global information about how our PluginURLs are structured */
namespace UE::GameFeatures
{
	namespace PluginURLStructureInfo
	{
		extern const TCHAR* OptionAssignOperator;
		extern const TCHAR* OptionSeperator;
	};

	namespace CommonErrorCodes
	{
		extern const FString PluginNotAllowed;
		extern const FString DependencyFailedRegister;
		extern const FString BadURL;
		extern const FString UnreachableState;
		extern const FString NoURLUpdateNeeded;
		
		extern const FString CancelAddonCode;
	};

	namespace CommonErrorText
	{
		extern const FText GenericError;
	};
};

/** 
 * Struct that determines if game feature action state changes should be applied for cases where there are multiple worlds or contexts.
 * The default value means to apply to all possible objects. This can be safely copied and used for later querying.
 */
struct GAMEFEATURES_API FGameFeatureStateChangeContext
{
public:

	/** Sets a specific world context handle to limit changes to */
	void SetRequiredWorldContextHandle(FName Handle);

	/** Sees if the specific world context matches the application rules */
	bool ShouldApplyToWorldContext(const FWorldContext& WorldContext) const;

	/** True if events bound using this context should apply when using other context */
	bool ShouldApplyUsingOtherContext(const FGameFeatureStateChangeContext& OtherContext) const;

	/** Check if this has the exact same state change application rules */
	FORCEINLINE bool operator==(const FGameFeatureStateChangeContext& OtherContext) const
	{
		if (OtherContext.WorldContextHandle == WorldContextHandle)
		{
			return true;
		}

		return false;
	}

	/** Allow this to be used as a map key */
	FORCEINLINE friend uint32 GetTypeHash(const FGameFeatureStateChangeContext& OtherContext)
	{
		return GetTypeHash(OtherContext.WorldContextHandle);
	}

private:
	/** Specific world context to limit changes to, if none then it will apply to all */
	FName WorldContextHandle;
};

/** Context that provides extra information for activating a game feature */
struct FGameFeatureActivatingContext : public FGameFeatureStateChangeContext
{
public:
	//@TODO: Add rules specific to activation when required

private:

	friend struct FGameFeaturePluginState_Activating;
};

/** Context that provides extra information for deactivating a game feature, will use the same change context rules as the activating context */
struct FGameFeatureDeactivatingContext : public FGameFeatureStateChangeContext
{
public:
	// Call this if your observer has an asynchronous action to complete as part of shutdown, and invoke the returned delegate when you are done (on the game thread!)
	FSimpleDelegate PauseDeactivationUntilComplete()
	{
		++NumPausers;
		return CompletionDelegate;
	}

	FGameFeatureDeactivatingContext(FSimpleDelegate&& InCompletionDelegate)
		: CompletionDelegate(MoveTemp(InCompletionDelegate))
	{
	}

	int32 GetNumPausers() const { return NumPausers; }
private:
	FSimpleDelegate CompletionDelegate;
	int32 NumPausers = 0;

	friend struct FGameFeaturePluginState_Deactivating;
};

/** Context that provides extra information for a game feature changing its pause state */
struct FGameFeaturePauseStateChangeContext : public FGameFeatureStateChangeContext
{
public:
	FGameFeaturePauseStateChangeContext(FString PauseStateNameIn, FString PauseReasonIn, bool bIsPausedIn)
		: PauseStateName(MoveTemp(PauseStateNameIn))
		, PauseReason(MoveTemp(PauseReasonIn))
		, bIsPaused(bIsPausedIn)
	{
	}

	/** Returns true if the State has paused or false if it is resuming */
	bool IsPaused() const { return bIsPaused; }

	/** Returns an FString description of why the state has paused work. */
	const FString& GetPauseReason() const { return PauseReason; }

	/** Returns an FString description of what state has issued the pause change */
	const FString& GetPausingStateName() const { return PauseStateName; }

private:
	FString PauseStateName;
	FString PauseReason;
	bool bIsPaused = false;
};

GAMEFEATURES_API DECLARE_LOG_CATEGORY_EXTERN(LogGameFeatures, Log, All);
/** Notification that a game feature plugin install/register/load/unload has finished */
DECLARE_DELEGATE_OneParam(FGameFeaturePluginChangeStateComplete, const UE::GameFeatures::FResult& /*Result*/);

using FGameFeaturePluginLoadComplete = FGameFeaturePluginChangeStateComplete;
using FGameFeaturePluginDeactivateComplete = FGameFeaturePluginChangeStateComplete;
using FGameFeaturePluginUnloadComplete = FGameFeaturePluginChangeStateComplete;
using FGameFeaturePluginReleaseComplete = FGameFeaturePluginChangeStateComplete;
using FGameFeaturePluginUninstallComplete = FGameFeaturePluginChangeStateComplete;
using FGameFeaturePluginTerminateComplete = FGameFeaturePluginChangeStateComplete;
using FGameFeaturePluginUpdateURLComplete = FGameFeaturePluginChangeStateComplete;

/** Notification delegate to be called by certain states that want to notify when they pause/resume work without leaving their current state.
EX: Downloading can be paused due to cellular or internet connection outages without failing the download,
but bubbling that pause information up to the user can be beneficial for messaging */
DECLARE_DELEGATE_TwoParams(FGameFeaturePluginOnStatePausedChange, bool /*bIsPaused*/, const FString& /*PauseReason*/);

/** Notification that a game feature plugin load has finished successfully and feeds back the GameFeatureData*/
DECLARE_MULTICAST_DELEGATE_TwoParams(FGameFeaturePluginLoadCompleteDataReady, const FString& /*Name*/, const UGameFeatureData* /*Data*/);

/** Notification that a game feature plugin load has deactivated successfully and feeds back the GameFeatureData that was being used*/
DECLARE_MULTICAST_DELEGATE_TwoParams(FGameFeaturePluginDeativated, const FString& /*Name*/, const UGameFeatureData* /*Data*/);

enum class EBuiltInAutoState : uint8
{
	Invalid,
	Installed,
	Registered,
	Loaded,
	Active
};

UENUM(BlueprintType)
enum class EGameFeatureTargetState : uint8
{
	Installed,
	Registered,
	Loaded,
	Active,
	Count	UMETA(Hidden)
};
const FString GAMEFEATURES_API LexToString(const EGameFeatureTargetState GameFeatureTargetState);

struct FGameFeaturePluginDetails
{
	TArray<FString> PluginDependencies;
	TMap<FString, FString> AdditionalMetadata;
	bool bHotfixable;
	EBuiltInAutoState BuiltInAutoState;

	FGameFeaturePluginDetails()
		: bHotfixable(false)
		, BuiltInAutoState(EBuiltInAutoState::Installed)
	{}
};

struct FBuiltInGameFeaturePluginBehaviorOptions
{
	EBuiltInAutoState AutoStateOverride = EBuiltInAutoState::Invalid;
};

/** Struct used to transform a GameFeaturePlugin URL into something that can uniquely identify the GameFeaturePlugin
    without including any transient data being passed in through the URL */
USTRUCT()
struct FGameFeaturePluginIdentifier
{
	GENERATED_BODY()

	/** The protocol used in the URL for this GameFeaturePlugin URL */
	EGameFeaturePluginProtocol PluginProtocol;

	FGameFeaturePluginIdentifier() = default;
	FGameFeaturePluginIdentifier(const FString& PluginURL);

	/** Used to determine if 2 FGameFeaturePluginIdentifiers are referencing the same GameFeaturePlugin.
		Only matching on Identifying information instead of all the optional bundle information */
	bool operator==(const FGameFeaturePluginIdentifier& Other) const;

	/** Function that fills out IdentifyingURLSubset from the given PluginURL */
	void FromPluginURL(const FString& PluginURL);

	/** Returns true if this FGameFeaturePluginIdentifier exactly matches the given PluginURL.
		To match exactly all information in the PluginURL has to match and not just the IdentifyingURLSubset */
	bool ExactMatchesURL(const FString& PluginURL) const;

	/** Get the Full PluginURL used to originally construct this identifier */
	const FString& GetFullPluginURL() const { return PluginURL; }

	friend FORCEINLINE uint32 GetTypeHash(const FGameFeaturePluginIdentifier& PluginIdentifier)
	{
		return GetTypeHash(PluginIdentifier.IdentifyingURLSubset);
	}

private:
	/** The part of the URL that can be used to uniquely identify this plugin without any transient data */
	FString IdentifyingURLSubset;

	/** Full PluginURL used to originally construct this identifier */
	FString PluginURL;

	//Friend class so that it can access parsed URL data from under the hood
	friend struct FGameFeaturePluginStateMachineProperties;
};

USTRUCT()
struct GAMEFEATURES_API FInstallBundlePluginProtocolMetaData
{
	GENERATED_BODY()

	TArray<FName> InstallBundles;

	/** Set to whatever the FDefaultValues::CurrentVersionNum was when this URL was generated.
		Allows us to ensure if we try and load URLs generated from a previous version **/
	uint8 VersionNum;

	/** If we want to attempt to uninstall InstallBundle information installed by this plugin before terminating */
	bool bUninstallBeforeTerminate;

	/** EInstallBundleRequestFlags utilized during the download/install by InstallBundleManager */
	EInstallBundleRequestFlags InstallBundleFlags;

	/** EInstallBundleReleaseRequestFlags utilized during our release and uninstall states */
	EInstallBundleReleaseRequestFlags ReleaseInstallBundleFlags;

	/** If we want to set the Downloading state to pause because of user interaction */
	bool bUserPauseDownload;

	/** Functions to convert to/from the URL FString representation of this metadata **/
	FString ToString() const;
	static bool FromString(const FString& URLString, FInstallBundlePluginProtocolMetaData& OutMetadata);

	/** Resets all our Metadata values to the default values */
	void ResetToDefaults();

	FInstallBundlePluginProtocolMetaData();

private:
	/** Holds default values for the above settings as they are only encoded into a string if they differ from these values */
	struct FDefaultValues
	{
		static const uint32 CurrentVersionNum;
		//Missing InstallBundles on purpose as the default is just an empty TArray and should always be encoded
		static const bool Default_bUninstallBeforeTerminate;
		static const bool Default_bUserPauseDownload;
		static const EInstallBundleRequestFlags Default_InstallBundleFlags;
		static const EInstallBundleReleaseRequestFlags Default_ReleaseInstallBundleFlags;
	};
};

/** The manager subsystem for game features */
UCLASS()
class GAMEFEATURES_API UGameFeaturesSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	//~UEngineSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End of UEngineSubsystem interface

	static UGameFeaturesSubsystem& Get() { return *GEngine->GetEngineSubsystem<UGameFeaturesSubsystem>(); }

public:
	/** Loads the specified game feature data and its bundles */
	static TSharedPtr<FStreamableHandle> LoadGameFeatureData(const FString& GameFeatureToLoad);
	static void UnloadGameFeatureData(const UGameFeatureData* GameFeatureToUnload);

	void AddObserver(UObject* Observer);
	void RemoveObserver(UObject* Observer);

	/**
	 * Calls the compile-time lambda on each active game feature data of the specified type
	 * @param GameFeatureDataType       The kind of data required
	 */
	template<class GameFeatureDataType, typename Func>
	void ForEachActiveGameFeature(Func InFunc) const
	{
		for (auto StateMachineIt = GameFeaturePluginStateMachines.CreateConstIterator(); StateMachineIt; ++StateMachineIt)
		{
			if (UGameFeaturePluginStateMachine* GFSM = StateMachineIt.Value())
			{
				if (const GameFeatureDataType* GameFeatureData = Cast<const GameFeatureDataType>(GetDataForStateMachine(GFSM)))
				{
					InFunc(GameFeatureData);
				}
			}
		}
	}

	/**
	 * Calls the compile-time lambda on each registered game feature data of the specified type
	 * @param GameFeatureDataType       The kind of data required
	 */
	template<class GameFeatureDataType, typename Func>
	void ForEachRegisteredGameFeature(Func InFunc) const
	{
		for (auto StateMachineIt = GameFeaturePluginStateMachines.CreateConstIterator(); StateMachineIt; ++StateMachineIt)
		{
			if (UGameFeaturePluginStateMachine* GFSM = StateMachineIt.Value())
			{
				if (const GameFeatureDataType* GameFeatureData = Cast<const GameFeatureDataType>(GetRegisteredDataForStateMachine(GFSM)))
				{
					InFunc(GameFeatureData);
				}
			}
		}
	}

public:
	/** Construct a 'file:' Plugin URL using from the PluginDescriptorPath */
	static FString GetPluginURL_FileProtocol(const FString& PluginDescriptorPath);

	/** Construct a 'installbundle:' Plugin URL using from the PluginName and required install bundles */
	static FString GetPluginURL_InstallBundleProtocol(const FString& PluginName, TArrayView<const FString> BundleNames);
	static FString GetPluginURL_InstallBundleProtocol(const FString& PluginName, const FString& BundleName);
	static FString GetPluginURL_InstallBundleProtocol(const FString& PluginName, TArrayView<const FName> BundleNames);
	static FString GetPluginURL_InstallBundleProtocol(const FString& PluginName, FName BundleName);
	static FString GetPluginURL_InstallBundleProtocol(const FString& PluginName, const FInstallBundlePluginProtocolMetaData& ProtocolMetadata);

	/** Returns the plugin protocol for the specified URL */
	static EGameFeaturePluginProtocol GetPluginURLProtocol(FStringView PluginURL);

	/** Tests whether the plugin URL is the specified protocol */
	static bool IsPluginURLProtocol(FStringView PluginURL, EGameFeaturePluginProtocol PluginProtocol);

public:
	/** Returns all the active plugins GameFeatureDatas */
	void GetGameFeatureDataForActivePlugins(TArray<const UGameFeatureData*>& OutActivePluginFeatureDatas);

	/** Returns the game feature data for an active plugin specified by PluginURL */
	const UGameFeatureData* GetGameFeatureDataForActivePluginByURL(const FString& PluginURL);

	/** Returns the game feature data for a registered plugin specified by PluginURL */
	const UGameFeatureData* GetGameFeatureDataForRegisteredPluginByURL(const FString& PluginURL, bool bCheckForRegistering = false);

	/** Determines if a plugin is in the Installed state (or beyond) */
	bool IsGameFeaturePluginInstalled(const FString& PluginURL) const;

	/** Determines if a plugin is in the Registered state (or beyond) */
	bool IsGameFeaturePluginRegistered(const FString& PluginURL, bool bCheckForRegistering = false) const;

	/** Determines if a plugin is in the Loaded state (or beyond) */
	bool IsGameFeaturePluginLoaded(const FString& PluginURL) const;

	/** Loads a single game feature plugin. */
	void LoadGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginLoadComplete& CompleteDelegate);

	/** Loads a single game feature plugin and activates it. */
	void LoadAndActivateGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginLoadComplete& CompleteDelegate);

	/** Changes the target state of a game feature plugin */
	void ChangeGameFeatureTargetState(const FString& PluginURL, EGameFeatureTargetState TargetState, const FGameFeaturePluginChangeStateComplete& CompleteDelegate);

	/** Changes the URL data of a game feature plugin. Useful to change any options data that is parsed from the URL such as settings flags */
	void UpdateGameFeaturePluginURL(const FString& NewPluginURL);
	void UpdateGameFeaturePluginURL(const FString& NewPluginURL, const FGameFeaturePluginUpdateURLComplete& CompleteDelegate);

	/** Gets the Install_Percent for single game feature plugin if it is active. */
	bool GetGameFeaturePluginInstallPercent(const FString& PluginURL, float& Install_Percent) const;

	/** Determines if a plugin is in the Active state.*/
	bool IsGameFeaturePluginActive(const FString& PluginURL, bool bCheckForActivating = false) const;

	/** Deactivates the specified plugin */
	void DeactivateGameFeaturePlugin(const FString& PluginURL);
	void DeactivateGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginDeactivateComplete& CompleteDelegate);

	/** Unloads the specified game feature plugin. */
	void UnloadGameFeaturePlugin(const FString& PluginURL, bool bKeepRegistered = false);
	void UnloadGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginUnloadComplete& CompleteDelegate, bool bKeepRegistered = false);

	/** Releases any game data stored for this GameFeaturePlugin. Does not uninstall data and it will remain on disk. */
	void ReleaseGameFeaturePlugin(const FString& PluginURL);
	void ReleaseGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginReleaseComplete& CompleteDelegate);

	/** Uninstalls any game data stored for this GameFeaturePlugin and terminates the GameFeaturePlugin.
		If the given PluginURL is not found this will create a GameFeaturePlugin first and attempt to run it through the uninstall flow.
		This allows for the uninstalling of data that was installed on previous runs of the application where we haven't yet requested the
		GameFeaturePlugin that we would like to uninstall data for on this run. */
	void UninstallGameFeaturePlugin(const FString& PluginURL);
	void UninstallGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginUninstallComplete& CompleteDelegate);

	/** Terminate the GameFeaturePlugin and remove all associated plugin tracking data. */
	void TerminateGameFeaturePlugin(const FString& PluginURL);
	void TerminateGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginTerminateComplete& CompleteDelegate);
	
	/** Attempt to cancel any state change. Calls back when cancelation is complete. Any other pending callbacks will be called with a canceled error. */
	void CancelGameFeatureStateChange(const FString& PluginURL);
	void CancelGameFeatureStateChange(const FString& PluginURL, const FGameFeaturePluginChangeStateComplete& CompleteDelegate);

	/**
	 * If the specified plugin is known by the game feature system, returns the URL used to identify it
	 * @return true if the plugin exists, false if it was not found
	 */
	bool GetPluginURLByName(const FString& PluginName, FString& OutPluginURL) const;

	/** If the specified plugin is a built-in plugin, return the URL used to identify it. Returns true if the plugin exists, false if it was not found */
	UE_DEPRECATED(5.1, "Use GetPluginURLByName instead")
	bool GetPluginURLForBuiltInPluginByName(const FString& PluginName, FString& OutPluginURL) const;

	/** Get the plugin path from the plugin URL */
	FString GetPluginFilenameFromPluginURL(const FString& PluginURL) const;

	/** Fixes a package path/directory to either be relative to plugin root or not. Paths relative to different roots will not be modified */
	static void FixPluginPackagePath(FString& PathToFix, const FString& PluginRootPath, bool bMakeRelativeToPluginRoot);

	/** Returns the game-specific policy for managing game feature plugins */
	template <typename T = UGameFeaturesProjectPolicies>
	T& GetPolicy() const
	{
		return *CastChecked<T>(GameSpecificPolicies, ECastCheckedType::NullChecked);
	}

	typedef TFunctionRef<bool(const FString& PluginFilename, const FGameFeaturePluginDetails& Details, FBuiltInGameFeaturePluginBehaviorOptions& OutOptions)> FBuiltInPluginAdditionalFilters;

	/** Loads a built-in game feature plugin if it passes the specified filter */
	void LoadBuiltInGameFeaturePlugin(const TSharedRef<IPlugin>& Plugin, FBuiltInPluginAdditionalFilters AdditionalFilter);

	/** Loads all built-in game feature plugins that pass the specified filters */
	void LoadBuiltInGameFeaturePlugins(FBuiltInPluginAdditionalFilters AdditionalFilter);

	/** Returns the list of plugin filenames that have progressed beyond installed. Used in cooking to determine which will be cooked. */
	//@TODO: GameFeaturePluginEnginePush: Might not be general enough for engine level, TBD
	void GetLoadedGameFeaturePluginFilenamesForCooking(TArray<FString>& OutLoadedPluginFilenames) const;

	/** Removes assets that are in plugins we know to be inactive.  Order is not maintained. */
	void FilterInactivePluginAssets(TArray<FAssetIdentifier>& AssetsToFilter) const;

	/** Removes assets that are in plugins we know to be inactive.  Order is not maintained. */
	void FilterInactivePluginAssets(TArray<FAssetData>& AssetsToFilter) const;

	/** Returns the current state of the state machine for the specified plugin URL */
	EGameFeaturePluginState GetPluginState(const FString& PluginURL);

	/** Determine the initial feature state for a built-in plugin */
	static EBuiltInAutoState DetermineBuiltInInitialFeatureState(TSharedPtr<FJsonObject> Descriptor, const FString& ErrorContext);

	static EGameFeaturePluginState ConvertInitialFeatureStateToTargetState(EBuiltInAutoState InitialState);

private:
	TSet<FString> GetActivePluginNames() const;

	void OnGameFeatureTerminating(const FString& PluginName, const FString& PluginURL);
	friend struct FGameFeaturePluginState_Terminal;

	void OnGameFeatureCheckingStatus(const FString& PluginURL);
	friend struct FGameFeaturePluginState_UnknownStatus;

	void OnGameFeatureStatusKnown(const FString& PluginName, const FString& PluginURL);
	friend struct FGameFeaturePluginState_CheckingStatus;

	void OnGameFeatureRegistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL);
	friend struct FGameFeaturePluginState_Registering;

	void OnGameFeatureUnregistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL);
	friend struct FGameFeaturePluginState_Unregistering;

	void OnGameFeatureActivating(const UGameFeatureData* GameFeatureData, const FString& PluginName, FGameFeatureActivatingContext& Context, const FString& PluginURL);
	friend struct FGameFeaturePluginState_Activating;

	void OnGameFeatureDeactivating(const UGameFeatureData* GameFeatureData, const FString& PluginName, FGameFeatureDeactivatingContext& Context, const FString& PluginURL);
	friend struct FGameFeaturePluginState_Deactivating;

	void OnGameFeatureLoading(const UGameFeatureData* GameFeatureData, const FString& PluginURL);
	friend struct FGameFeaturePluginState_Loading;

	void OnGameFeaturePauseChange(const FString& PluginURL, const FString& PluginName, FGameFeaturePauseStateChangeContext& Context);
	friend struct FGameFeaturePluginState_Downloading;
	friend struct FGameFeaturePluginState_Deactivating;

	void OnAssetManagerCreated();

	/** Scans for assets specified in the game feature data */
	static void AddGameFeatureToAssetManager(const UGameFeatureData* GameFeatureToAdd, const FString& PluginName, TArray<FName>& OutNewPrimaryAssetTypes);

	static void RemoveGameFeatureFromAssetManager(const UGameFeatureData* GameFeatureToRemove, const FString& PluginName, const TArray<FName>& AddedPrimaryAssetTypes);

private:
	bool ShouldUpdatePluginURLData(const FString& NewPluginURL);

	const UGameFeatureData* GetDataForStateMachine(UGameFeaturePluginStateMachine* GFSM) const;
	const UGameFeatureData* GetRegisteredDataForStateMachine(UGameFeaturePluginStateMachine* GFSM) const;

	/** Gets relevant properties out of a uplugin file */
	bool GetGameFeaturePluginDetails(const FString& PluginDescriptorFilename, struct FGameFeaturePluginDetails& OutPluginDetails) const;

	/** Gets the state machine associated with the specified plugin name */
	UGameFeaturePluginStateMachine* FindGameFeaturePluginStateMachineByPluginName(const FString& PluginName) const;

	/** Gets the state machine associated with the specified URL */
	UGameFeaturePluginStateMachine* FindGameFeaturePluginStateMachine(const FString& PluginURL) const;

	/** Gets the state machine associated with the specified URL, creates it if it doesnt exist */
	UGameFeaturePluginStateMachine* FindOrCreateGameFeaturePluginStateMachine(const FString& PluginURL);

	/** Notification that a game feature has finished loading, and whether it was successful */
	void LoadBuiltInGameFeaturePluginComplete(const UE::GameFeatures::FResult& Result, UGameFeaturePluginStateMachine* Machine, FGameFeaturePluginStateRange RequestedDestination);

	/** Sets a new destination state. Will attempt to cancel the current transition if the new destination is incompatible with the current destination */
	void ChangeGameFeatureDestination(UGameFeaturePluginStateMachine* Machine, const FGameFeaturePluginStateRange& StateRange, FGameFeaturePluginChangeStateComplete CompleteDelegate);

	/** Generic notification that calls the Complete delegate without broadcasting anything else.*/
	void ChangeGameFeatureTargetStateComplete(UGameFeaturePluginStateMachine* Machine, const UE::GameFeatures::FResult& Result, FGameFeaturePluginChangeStateComplete CompleteDelegate);

	void BeginTermination(UGameFeaturePluginStateMachine* Machine);
	void FinishTermination(UGameFeaturePluginStateMachine* Machine);
	friend class UGameFeaturePluginStateMachine;

	/** Handler for when a state machine requests its dependencies. Returns false if the dependencies could not be read */
	bool FindOrCreatePluginDependencyStateMachines(const FString& PluginFilename, TArray<UGameFeaturePluginStateMachine*>& OutDependencyMachines);
	friend struct FGameFeaturePluginState_WaitingForDependencies;

	/** Handle 'ListGameFeaturePlugins' console command */
	void ListGameFeaturePlugins(const TArray<FString>& Args, UWorld* InWorld, FOutputDevice& Ar);

	enum class EObserverCallback
	{
		CheckingStatus,
		Terminating,
		Registering,
		Unregistering,
		Loading,
		Activating,
		Deactivating,
		PauseChanged,
		Count
	};

	void CallbackObservers(EObserverCallback CallbackType, const FString& PluginURL, 
		const FString* PluginName = nullptr, 
		const UGameFeatureData* GameFeatureData = nullptr, 
		FGameFeatureStateChangeContext* StateChangeContext = nullptr);

private:
	/** The list of all game feature plugin state machine objects */
	UPROPERTY(Transient)
	TMap<FGameFeaturePluginIdentifier, TObjectPtr<UGameFeaturePluginStateMachine>> GameFeaturePluginStateMachines;

	/** Game feature plugin state machine objects that are being terminated. Used to prevent GC until termination is complete. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameFeaturePluginStateMachine>> TerminalGameFeaturePluginStateMachines;

	TMap<FString, FString> GameFeaturePluginNameToPathMap;

	UPROPERTY()
	TArray<TObjectPtr<UObject>> Observers;

	UPROPERTY(Transient)
	TObjectPtr<UGameFeaturesProjectPolicies> GameSpecificPolicies;

	bool bInitializedPolicyManager = false;
};

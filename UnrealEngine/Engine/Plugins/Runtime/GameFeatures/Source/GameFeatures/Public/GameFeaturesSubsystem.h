// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Union.h"
#include "Engine/Engine.h"
#include "GameFeatureTypesFwd.h"

#include "GameFeaturesSubsystem.generated.h"

namespace UE::GameFeatures { struct FResult; }

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
struct FGameFeaturePluginStateMachineProperties;
enum class EInstallBundleRequestFlags : uint32;
enum class EInstallBundleReleaseRequestFlags : uint32;

/** Holds static global information about how our PluginURLs are structured */
namespace UE::GameFeatures
{
	namespace PluginURLStructureInfo
	{
		/** Character used to denote what value is being assigned to the option before it */
		extern GAMEFEATURES_API const TCHAR* OptionAssignOperator;

		/** Character used to separate options on the URL. Used between each assigned value and the next Option name. */
		extern GAMEFEATURES_API const TCHAR* OptionSeperator;

		/** Character used to separate lists of values for a single option. Used between each entry in the list. */
		extern GAMEFEATURES_API const TCHAR* OptionListSeperator;
	};

	namespace CommonErrorCodes
	{
		extern const TCHAR* DependencyFailedRegister;
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
	UE_DEPRECATED(5.2, "Use tagged version instead")
	FSimpleDelegate PauseDeactivationUntilComplete()
	{
		return PauseDeactivationUntilComplete(TEXT("Unknown(Deprecated)"));
	}

	// Call this if your observer has an asynchronous action to complete as part of shutdown, and invoke the returned delegate when you are done (on the game thread!)
	GAMEFEATURES_API FSimpleDelegate PauseDeactivationUntilComplete(FString InPauserTag);

	UE_DEPRECATED(5.2, "Use tagged version instead")
	FGameFeatureDeactivatingContext(FSimpleDelegate&& InCompletionDelegate)
		: PluginName(TEXTVIEW("Unknown(Deprecated)"))
		, CompletionCallback([CompletionDelegate = MoveTemp(InCompletionDelegate)](FStringView) { CompletionDelegate.ExecuteIfBound(); })
	{
	}

	FGameFeatureDeactivatingContext(FStringView InPluginName, TFunction<void(FStringView InPauserTag)>&& InCompletionCallback)
		: PluginName(InPluginName)
		, CompletionCallback(MoveTemp(InCompletionCallback))
	{
	}

	int32 GetNumPausers() const { return NumPausers; }
private:
	FStringView PluginName;
	TFunction<void(FStringView InPauserTag)> CompletionCallback;
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


/** Context that provides extra information prior to mounting a plugin */
struct FGameFeaturePreMountingContext : public FGameFeatureStateChangeContext
{
public:
	bool bOpenPluginShaderLibrary = true;

private:

	friend struct FGameFeaturePluginState_Mounting;
};

/** Context that allows pausing prior to transitioning out of the mounting state */
struct FGameFeaturePostMountingContext : public FGameFeatureStateChangeContext
{
public:
	// Call this if your observer has an asynchronous action to complete prior to transitioning out of the mounting state
	// and invoke the returned delegate when you are done (on the game thread!)
	GAMEFEATURES_API FSimpleDelegate PauseUntilComplete(FString InPauserTag);

	FGameFeaturePostMountingContext(FStringView InPluginName, TFunction<void(FStringView InPauserTag)>&& InCompletionCallback)
		: PluginName(InPluginName)
		, CompletionCallback(MoveTemp(InCompletionCallback))
	{}

	int32 GetNumPausers() const { return NumPausers; }

private:
	FStringView PluginName;
	TFunction<void(FStringView InPauserTag)> CompletionCallback;
	int32 NumPausers = 0;

	friend struct FGameFeaturePluginState_Mounting;
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
using FGameFeaturePluginUpdateProtocolComplete = FGameFeaturePluginChangeStateComplete;

DECLARE_DELEGATE_OneParam(FMultipleGameFeaturePluginChangeStateComplete, PREPROCESSOR_COMMA_SEPARATED(const TMap<FString, UE::GameFeatures::FResult>& /*Results*/));

using FBuiltInGameFeaturePluginsLoaded = FMultipleGameFeaturePluginChangeStateComplete;
using FMultipleGameFeaturePluginsLoaded = FMultipleGameFeaturePluginChangeStateComplete;

enum class EBuiltInAutoState : uint8
{
	Invalid,
	Installed,
	Registered,
	Loaded,
	Active
};
const FString GAMEFEATURES_API LexToString(const EBuiltInAutoState BuiltInAutoState);

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
void GAMEFEATURES_API LexFromString(EGameFeatureTargetState& Value, const TCHAR* StringIn);

struct FGameFeaturePluginReferenceDetails
{
	FString PluginName;
	bool bShouldActivate;

	FGameFeaturePluginReferenceDetails(FString InPluginName, bool bInShouldActivate)
		: PluginName(MoveTemp(InPluginName))
		, bShouldActivate(bInShouldActivate)
	{
	}
};

struct FGameFeaturePluginDetails
{
	TArray<FGameFeaturePluginReferenceDetails> PluginDependencies;
	TMap<FString, TSharedPtr<class FJsonValue>> AdditionalMetadata;
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

	/** Force this GFP to load synchronously even if async loading is allowed */
	bool bForceSyncLoading = false;

	/** Log Warning if loading this GFP forces dependencies to be created, useful for catching GFP load filtering bugs */
	bool bLogWarningOnForcedDependencyCreation = false;

	/** Log Error if loading this GFP forces dependencies to be created, useful for catching GFP load filtering bugs */
	bool bLogErrorOnForcedDependencyCreation = false;
};

struct FGameFeaturePluginPredownloadHandle : public TSharedFromThis<FGameFeaturePluginPredownloadHandle>
{
	virtual ~FGameFeaturePluginPredownloadHandle() {}
	virtual bool IsComplete() const = 0;
	virtual const UE::GameFeatures::FResult& GetResult() const = 0;
	virtual float GetProgress() const = 0;
	virtual void Cancel() = 0;
};

/** Struct used to transform a GameFeaturePlugin URL into something that can uniquely identify the GameFeaturePlugin
    without including any transient data being passed in through the URL */
USTRUCT()
struct GAMEFEATURES_API FGameFeaturePluginIdentifier
{
	GENERATED_BODY()

	FGameFeaturePluginIdentifier() = default;
	explicit FGameFeaturePluginIdentifier(FString PluginURL);

	FGameFeaturePluginIdentifier(const FGameFeaturePluginIdentifier& Other)
		: FGameFeaturePluginIdentifier(Other.PluginURL)
	{}

	FGameFeaturePluginIdentifier(FGameFeaturePluginIdentifier&& Other);

	FGameFeaturePluginIdentifier& operator=(const FGameFeaturePluginIdentifier& Other)
	{
		FromPluginURL(Other.PluginURL);
		return *this;
	}

	FGameFeaturePluginIdentifier& operator=(FGameFeaturePluginIdentifier&& Other);

	/** Used to determine if 2 FGameFeaturePluginIdentifiers are referencing the same GameFeaturePlugin.
		Only matching on Identifying information instead of all the optional bundle information */
	bool operator==(const FGameFeaturePluginIdentifier& Other) const;

	/** Function that fills out IdentifyingURLSubset from the given PluginURL */
	void FromPluginURL(FString PluginURL);

	/** Returns true if this FGameFeaturePluginIdentifier exactly matches the given PluginURL.
		To match exactly all information in the PluginURL has to match and not just the IdentifyingURLSubset */
	bool ExactMatchesURL(const FString& PluginURL) const;

	EGameFeaturePluginProtocol GetPluginProtocol() const { return PluginProtocol; }

	/** Returns the Identifying information used for this Plugin. It is a subset of the URL used to create it.*/
	FStringView GetIdentifyingString() const { return IdentifyingURLSubset; }

	/** Returns the name of the plugin */
	FStringView GetPluginName() const;

	/** Get the Full PluginURL used to originally construct this identifier */
	const FString& GetFullPluginURL() const { return PluginURL; }

	friend FORCEINLINE uint32 GetTypeHash(const FGameFeaturePluginIdentifier& PluginIdentifier)
	{
		return GetTypeHash(PluginIdentifier.IdentifyingURLSubset);
	}

private:
	/** Full PluginURL used to originally construct this identifier */
	FString PluginURL;

	/** The part of the URL that can be used to uniquely identify this plugin without any transient data */
	FStringView IdentifyingURLSubset;

	/** The protocol used in the URL for this GameFeaturePlugin URL */
	EGameFeaturePluginProtocol PluginProtocol;

	//Friend class so that it can access parsed URL data from under the hood
	friend struct FGameFeaturePluginStateMachineProperties;
};

USTRUCT()
struct GAMEFEATURES_API FInstallBundlePluginProtocolOptions
{
	GENERATED_BODY()

	FInstallBundlePluginProtocolOptions();

	/** EInstallBundleRequestFlags utilized during the download/install by InstallBundleManager */
	EInstallBundleRequestFlags InstallBundleFlags;

	/** EInstallBundleReleaseRequestFlags utilized during our release and uninstall states */
	EInstallBundleReleaseRequestFlags ReleaseInstallBundleFlags;

	/** If we want to attempt to uninstall InstallBundle data installed by this plugin before terminating */
	bool bUninstallBeforeTerminate : 1;

	/** If we want to set the Downloading state to pause because of user interaction */
	bool bUserPauseDownload : 1;

	/** Allow the GFP to load INI files, should only be allowed for trusted content */
	bool bAllowIniLoading : 1;

	/** Disallows downloading, useful for conditionally loading content only if it's already been installed **/
	bool bDoNotDownload : 1;

	bool operator==(const FInstallBundlePluginProtocolOptions& Other) const;
};

struct FGameFeatureProtocolOptions : public TUnion<FInstallBundlePluginProtocolOptions, FNull>
{
	GAMEFEATURES_API FGameFeatureProtocolOptions();
	GAMEFEATURES_API explicit FGameFeatureProtocolOptions(const FInstallBundlePluginProtocolOptions& InOptions);
	GAMEFEATURES_API explicit FGameFeatureProtocolOptions(FNull InOptions);

	/** Force this GFP to load synchronously even if async loading is allowed */
	bool bForceSyncLoading : 1;

	/** Log Warning if loading this GFP forces dependencies to be created, useful for catching GFP load filtering bugs */
	bool bLogWarningOnForcedDependencyCreation : 1;

	/** Log Error if loading this GFP forces dependencies to be created, useful for catching GFP load filtering bugs */
	bool bLogErrorOnForcedDependencyCreation : 1;
};

// some important information about a gamefeature
struct FGameFeatureInfo
{
	FString Name;
	FString URL;
	bool bLoadedAsBuiltIn;
	EGameFeaturePluginState CurrentState;
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
	static TSharedPtr<FStreamableHandle> LoadGameFeatureData(const FString& GameFeatureToLoad, bool bStartStalled = false);
	static void UnloadGameFeatureData(const UGameFeatureData* GameFeatureToUnload);

	void AddObserver(UObject* Observer);
	void RemoveObserver(UObject* Observer);

	void ForEachGameFeature(TFunctionRef<void(FGameFeatureInfo&&)> Visitor) const;

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
	static FString GetPluginURL_FileProtocol(const FString& PluginDescriptorPath, TArrayView<const TPair<FString, FString>> AdditionalOptions);

	/** Construct a 'installbundle:' Plugin URL using from the PluginName and required install bundles */
	static FString GetPluginURL_InstallBundleProtocol(const FString& PluginName, TArrayView<const FString> BundleNames);
	static FString GetPluginURL_InstallBundleProtocol(const FString& PluginName, const FString& BundleName);
	static FString GetPluginURL_InstallBundleProtocol(const FString& PluginName, TArrayView<const FName> BundleNames);
	static FString GetPluginURL_InstallBundleProtocol(const FString& PluginName, FName BundleName);
	static FString GetPluginURL_InstallBundleProtocol(const FString& PluginName, TArrayView<const FName> BundleNames, TArrayView<const TPair<FString, FString>> AdditionalOptions);

	/** Returns the plugin protocol for the specified URL */
	static EGameFeaturePluginProtocol GetPluginURLProtocol(FStringView PluginURL);

	/** Tests whether the plugin URL is the specified protocol */
	static bool IsPluginURLProtocol(FStringView PluginURL, EGameFeaturePluginProtocol PluginProtocol);

	/** Parse the plugin URL into subparts */
	static bool ParsePluginURL(FStringView PluginURL, EGameFeaturePluginProtocol* OutProtocol = nullptr, FStringView* OutPath = nullptr, FStringView* OutOptions = nullptr);

	/** Parse options from a plugin URL or the options subpart of the plugin URL */
	static bool ParsePluginURLOptions(FStringView URLOptionsString,
		TFunctionRef<void(EGameFeatureURLOptions Option, FStringView OptionString, FStringView OptionValue)> Output);
	static bool ParsePluginURLOptions(FStringView URLOptionsString, EGameFeatureURLOptions OptionsFlags,
		TFunctionRef<void(EGameFeatureURLOptions Option, FStringView OptionString, FStringView OptionValue)> Output);
	static bool ParsePluginURLOptions(FStringView URLOptionsString, TConstArrayView<FStringView> AdditionalOptions,
		TFunctionRef<void(EGameFeatureURLOptions Option, FStringView OptionString, FStringView OptionValue)> Output);
	static bool ParsePluginURLOptions(FStringView URLOptionsString, EGameFeatureURLOptions OptionsFlags, TConstArrayView<FStringView> AdditionalOptions,
		TFunctionRef<void(EGameFeatureURLOptions Option, FStringView OptionString, FStringView OptionValue)> Output);


public:
	/** Returns all the active plugins GameFeatureDatas */
	void GetGameFeatureDataForActivePlugins(TArray<const UGameFeatureData*>& OutActivePluginFeatureDatas);

	/** Returns the game feature data for an active plugin specified by PluginURL */
	const UGameFeatureData* GetGameFeatureDataForActivePluginByURL(const FString& PluginURL);

	/** Returns the game feature data for a registered plugin specified by PluginURL */
	const UGameFeatureData* GetGameFeatureDataForRegisteredPluginByURL(const FString& PluginURL, bool bCheckForRegistering = false);

	/** Determines if a plugin is in the Installed state (or beyond) */
	bool IsGameFeaturePluginInstalled(const FString& PluginURL) const;

	/** Determines if a plugin is beyond the Mounting state */
	bool IsGameFeaturePluginMounted(const FString& PluginURL) const;

	/** Determines if a plugin is in the Registered state (or beyond) */
	bool IsGameFeaturePluginRegistered(const FString& PluginURL, bool bCheckForRegistering = false) const;

	/** Determines if a plugin is in the Loaded state (or beyond) */
	bool IsGameFeaturePluginLoaded(const FString& PluginURL) const;

	/** Was this game feature plugin loaded using the LoadBuiltInGameFeaturePlugin path */
	bool WasGameFeaturePluginLoadedAsBuiltIn(const FString& PluginURL) const;

	/** Loads a single game feature plugin. */
	void LoadGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginLoadComplete& CompleteDelegate);
	void LoadGameFeaturePlugin(const FString& PluginURL, const FGameFeatureProtocolOptions& ProtocolOptions, const FGameFeaturePluginLoadComplete& CompleteDelegate);
	void LoadGameFeaturePlugin(TConstArrayView<FString> PluginURLs, const FGameFeatureProtocolOptions& ProtocolOptions, const FMultipleGameFeaturePluginsLoaded& CompleteDelegate);

	/** Loads a single game feature plugin and activates it. */
	void LoadAndActivateGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginLoadComplete& CompleteDelegate);
	void LoadAndActivateGameFeaturePlugin(const FString& PluginURL, const FGameFeatureProtocolOptions& ProtocolOptions, const FGameFeaturePluginLoadComplete& CompleteDelegate);
	void LoadAndActivateGameFeaturePlugin(TConstArrayView<FString> PluginURLs, const FGameFeatureProtocolOptions& ProtocolOptions, const FMultipleGameFeaturePluginsLoaded& CompleteDelegate);

	/** Changes the target state of a game feature plugin */
	void ChangeGameFeatureTargetState(const FString& PluginURL, EGameFeatureTargetState TargetState, const FGameFeaturePluginChangeStateComplete& CompleteDelegate);
	void ChangeGameFeatureTargetState(const FString& PluginURL, const FGameFeatureProtocolOptions& ProtocolOptions, EGameFeatureTargetState TargetState, const FGameFeaturePluginChangeStateComplete& CompleteDelegate);
	void ChangeGameFeatureTargetState(TConstArrayView<FString> PluginURLs, const FGameFeatureProtocolOptions& ProtocolOptions, EGameFeatureTargetState TargetState, const FMultipleGameFeaturePluginsLoaded& CompleteDelegate);

	/** Changes the protocol options of a game feature plugin. Useful to change any options data such as settings flags */
	UE::GameFeatures::FResult UpdateGameFeatureProtocolOptions(const FString& PluginURL, const FGameFeatureProtocolOptions& NewOptions, bool* bOutDidUpdate = nullptr);

	/** Gets the Install_Percent for single game feature plugin if it is active. */
	bool GetGameFeaturePluginInstallPercent(const FString& PluginURL, float& Install_Percent) const;
	bool GetGameFeaturePluginInstallPercent(TConstArrayView<FString> PluginURLs, float& Install_Percent) const;

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
	void UninstallGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginUninstallComplete& CompleteDelegate = FGameFeaturePluginUninstallComplete());
	void UninstallGameFeaturePlugin(const FString& PluginURL, const FGameFeatureProtocolOptions& ProtocolOptions, const FGameFeaturePluginUninstallComplete& CompleteDelegate = FGameFeaturePluginUninstallComplete());

	/** Terminate the GameFeaturePlugin and remove all associated plugin tracking data. */
	void TerminateGameFeaturePlugin(const FString& PluginURL);
	void TerminateGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginTerminateComplete& CompleteDelegate);
	
	/** Attempt to cancel any state change. Calls back when cancelation is complete. Any other pending callbacks will be called with a canceled error. */
	void CancelGameFeatureStateChange(const FString& PluginURL);
	void CancelGameFeatureStateChange(const FString& PluginURL, const FGameFeaturePluginChangeStateComplete& CompleteDelegate);
	void CancelGameFeatureStateChange(TConstArrayView<FString> PluginURLs, const FMultipleGameFeaturePluginChangeStateComplete& CompleteDelegate);

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
	void LoadBuiltInGameFeaturePlugin(const TSharedRef<IPlugin>& Plugin, FBuiltInPluginAdditionalFilters AdditionalFilter, const FGameFeaturePluginLoadComplete& CompleteDelegate = FGameFeaturePluginLoadComplete());

	/** Loads all built-in game feature plugins that pass the specified filters */
	void LoadBuiltInGameFeaturePlugins(FBuiltInPluginAdditionalFilters AdditionalFilter, const FBuiltInGameFeaturePluginsLoaded& CompleteDelegate = FBuiltInGameFeaturePluginsLoaded());

	/** Returns the list of plugin filenames that have progressed beyond installed. Used in cooking to determine which will be cooked. */
	//@TODO: GameFeaturePluginEnginePush: Might not be general enough for engine level, TBD
	void GetLoadedGameFeaturePluginFilenamesForCooking(TArray<FString>& OutLoadedPluginFilenames) const;

	/** Removes assets that are in plugins we know to be inactive.  Order is not maintained. */
	void FilterInactivePluginAssets(TArray<FAssetIdentifier>& AssetsToFilter) const;

	/** Removes assets that are in plugins we know to be inactive.  Order is not maintained. */
	void FilterInactivePluginAssets(TArray<FAssetData>& AssetsToFilter) const;

	/** Returns the current state of the state machine for the specified plugin URL */
	EGameFeaturePluginState GetPluginState(const FString& PluginURL) const;

	/** Returns the current state of the state machine for the specified plugin PluginIdentifier */
	EGameFeaturePluginState GetPluginState(FGameFeaturePluginIdentifier PluginIdentifier) const;

	/** Gets relevant properties out of a uplugin file */
	UE_DEPRECATED(5.4, "Use GetBuiltInGameFeaturePluginDetails instead")
	bool GetGameFeaturePluginDetails(const TSharedRef<IPlugin>& Plugin, FString& OutPluginURL, struct FGameFeaturePluginDetails& OutPluginDetails) const;

	/** Gets relevant properties out of a uplugin file. Should only be used for built-in GFPs */
	bool GetBuiltInGameFeaturePluginDetails(const TSharedRef<IPlugin>& Plugin, FString& OutPluginURL, struct FGameFeaturePluginDetails& OutPluginDetails) const;

	/** Gets relevant properties out of a uplugin file if it's installed */
	bool GetGameFeaturePluginDetails(FString PluginURL, struct FGameFeaturePluginDetails& OutPluginDetails) const;

	/** 
	 * Pre-install any required game feature data, which can be useful for larger payloads. 
	 * This does not instantiate any GFP although it is safe to do so before this finishes. 
	*/
	TSharedRef<FGameFeaturePluginPredownloadHandle> PredownloadGameFeaturePlugins(TConstArrayView<FString> PluginURLs, TUniqueFunction<void(const UE::GameFeatures::FResult&)> OnComplete = nullptr, TUniqueFunction<void(float)> OnProgress = nullptr);
	friend struct FGameFeaturePluginPredownloadContext;

	/** Determine the initial feature state for a built-in plugin */
	static EBuiltInAutoState DetermineBuiltInInitialFeatureState(TSharedPtr<FJsonObject> Descriptor, const FString& ErrorContext);

	static EGameFeaturePluginState ConvertInitialFeatureStateToTargetState(EBuiltInAutoState InitialState);

	/** Used during a DLC cook to determine which plugins should be cooked */
	static void GetPluginsToCook(TSet<FString>& OutPlugins);
private:
	TSet<FString> GetActivePluginNames() const;

	void OnGameFeatureTerminating(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier);
	friend struct FGameFeaturePluginState_Terminal;

	void OnGameFeatureCheckingStatus(const FGameFeaturePluginIdentifier& PluginIdentifier);
	friend struct FGameFeaturePluginState_UnknownStatus;

	void OnGameFeatureStatusKnown(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifierL);
	friend struct FGameFeaturePluginState_CheckingStatus;

	void OnGameFeaturePredownloading(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier);

	void OnGameFeatureDownloading(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier);
	friend struct FGameFeaturePluginState_Downloading;

	void OnGameFeatureReleasing(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier);
	friend struct FGameFeaturePluginState_Releasing;
	friend struct FGameFeaturePluginState_Unmounting;

	void OnGameFeaturePreMounting(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier, FGameFeaturePreMountingContext& Context);
	void OnGameFeaturePostMounting(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier, FGameFeaturePostMountingContext& Context);
	friend struct FGameFeaturePluginState_Mounting;

	void OnGameFeatureRegistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier);
	friend struct FGameFeaturePluginState_Registering;

	void OnGameFeatureUnregistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier);
	friend struct FGameFeaturePluginState_Unregistering;

	void OnGameFeatureActivating(const UGameFeatureData* GameFeatureData, const FString& PluginName, FGameFeatureActivatingContext& Context, const FGameFeaturePluginIdentifier& PluginIdentifier);
	friend struct FGameFeaturePluginState_Activating;

	void OnGameFeatureDeactivating(const UGameFeatureData* GameFeatureData, const FString& PluginName, FGameFeatureDeactivatingContext& Context, const FGameFeaturePluginIdentifier& PluginIdentifier);
	friend struct FGameFeaturePluginState_Deactivating;

	void OnGameFeatureLoading(const UGameFeatureData* GameFeatureData, const FGameFeaturePluginIdentifier& PluginIdentifier);
	friend struct FGameFeaturePluginState_Loading;

	void OnGameFeatureUnloading(const UGameFeatureData* GameFeatureData, const FGameFeaturePluginIdentifier& PluginIdentifier);
	friend struct FGameFeaturePluginState_Unloading;

	void OnGameFeaturePauseChange(const FGameFeaturePluginIdentifier& PluginIdentifier, const FString& PluginName, FGameFeaturePauseStateChangeContext& Context);
	friend struct FGameFeaturePluginState_Downloading;
	friend struct FGameFeaturePluginState_Deactivating;

	void OnAssetManagerCreated();

	/** Scans for assets specified in the game feature data */
	static void AddGameFeatureToAssetManager(const UGameFeatureData* GameFeatureToAdd, const FString& PluginName, TArray<FName>& OutNewPrimaryAssetTypes);

	static void RemoveGameFeatureFromAssetManager(const UGameFeatureData* GameFeatureToRemove, const FString& PluginName, const TArray<FName>& AddedPrimaryAssetTypes);

private:
	bool ShouldUpdatePluginProtocolOptions(const UGameFeaturePluginStateMachine* StateMachine, const FGameFeatureProtocolOptions& NewOptions);
	UE::GameFeatures::FResult UpdateGameFeatureProtocolOptions(UGameFeaturePluginStateMachine* StateMachine, const FGameFeatureProtocolOptions& NewOptions, bool* bOutDidUpdate = nullptr);

	const UGameFeatureData* GetDataForStateMachine(UGameFeaturePluginStateMachine* GFSM) const;
	const UGameFeatureData* GetRegisteredDataForStateMachine(UGameFeaturePluginStateMachine* GFSM) const;

	/** Gets relevant properties out of a uplugin file */
	bool GetGameFeaturePluginDetailsInternal(const FString& PluginDescriptorFilename, struct FGameFeaturePluginDetails& OutPluginDetails) const;

	/** Prunes any cached GFP details */
	void PruneCachedGameFeaturePluginDetails(const FString& PluginURL, const FString& PluginDescriptorFilename) const;
	friend struct FGameFeaturePluginState_Unmounting;

	/** Gets the state machine associated with the specified plugin name */
	UGameFeaturePluginStateMachine* FindGameFeaturePluginStateMachineByPluginName(const FString& PluginName) const;

	/** Gets the state machine associated with the specified URL */
	UGameFeaturePluginStateMachine* FindGameFeaturePluginStateMachine(const FString& PluginURL) const;

	/** Gets the state machine associated with the specified PluginIdentifier */
	UGameFeaturePluginStateMachine* FindGameFeaturePluginStateMachine(const FGameFeaturePluginIdentifier& PluginIdentifier) const;

	/** Gets the state machine associated with the specified URL, creates it if it doesnt exist */
	UGameFeaturePluginStateMachine* FindOrCreateGameFeaturePluginStateMachine(const FString& PluginURL, const FGameFeatureProtocolOptions& ProtocolOptions, bool* bOutFoundExisting = nullptr);

	/** Notification that a game feature has finished loading, and whether it was successful */
	void LoadBuiltInGameFeaturePluginComplete(const UE::GameFeatures::FResult& Result, UGameFeaturePluginStateMachine* Machine, FGameFeaturePluginStateRange RequestedDestination);

	/** 
	 * Sets a new destination state. Will attempt to cancel the current transition if the new destination is incompatible with the current destination 
	 * Note: In the case that the existing machine is terminal, a new one will need to be created. In that case ProtocolOptions will be used for the new machine.
	 */
	void ChangeGameFeatureDestination(UGameFeaturePluginStateMachine* Machine, const FGameFeaturePluginStateRange& StateRange, FGameFeaturePluginChangeStateComplete CompleteDelegate);
	void ChangeGameFeatureDestination(UGameFeaturePluginStateMachine* Machine, const FGameFeatureProtocolOptions& ProtocolOptions, const FGameFeaturePluginStateRange& StateRange, FGameFeaturePluginChangeStateComplete CompleteDelegate);

	/** Generic notification that calls the Complete delegate without broadcasting anything else.*/
	void ChangeGameFeatureTargetStateComplete(UGameFeaturePluginStateMachine* Machine, const UE::GameFeatures::FResult& Result, FGameFeaturePluginChangeStateComplete CompleteDelegate);

	void BeginTermination(UGameFeaturePluginStateMachine* Machine);
	void FinishTermination(UGameFeaturePluginStateMachine* Machine);
	friend class UGameFeaturePluginStateMachine;

	/** Handler for when a state machine requests its dependencies. Returns false if the dependencies could not be read */
	bool FindOrCreatePluginDependencyStateMachines(const FString& PluginURL, const FGameFeaturePluginStateMachineProperties& InStateProperties, TArray<UGameFeaturePluginStateMachine*>& OutDependencyMachines);
	template <typename> friend struct FTransitionDependenciesGameFeaturePluginState;
	friend struct FWaitingForDependenciesTransitionPolicy;

	bool FindPluginDependencyStateMachinesToActivate(const FString& PluginURL, const FString& PluginFilename, TArray<UGameFeaturePluginStateMachine*>& OutDependencyMachines) const;
	friend struct FActivatingDependenciesTransitionPolicy;

	bool FindPluginDependencyStateMachinesToDeactivate(const FString& PluginURL, const FString& PluginFilename, TArray<UGameFeaturePluginStateMachine*>& OutDependencyMachines) const;
	friend struct FDeactivatingDependenciesTransitionPolicy;

	template <typename CallableT>
	bool EnumeratePluginDependenciesWithShouldActivate(const FString& PluginURL, const FString& PluginFilename, CallableT Callable) const;

	/** Handle 'ListGameFeaturePlugins' console command */
	void ListGameFeaturePlugins(const TArray<FString>& Args, UWorld* InWorld, FOutputDevice& Ar);

	enum class EObserverCallback
	{
		CheckingStatus,
		Terminating,
		Predownloading,
		Downloading,
		Releasing,
		PreMounting,
		PostMounting,
		Registering,
		Unregistering,
		Loading,
		Unloading,
		Activating,
		Deactivating,
		PauseChanged,
		Count
	};

	void CallbackObservers(EObserverCallback CallbackType, const FGameFeaturePluginIdentifier& PluginIdentifier,
		const FString* PluginName = nullptr, 
		const UGameFeatureData* GameFeatureData = nullptr, 
		FGameFeatureStateChangeContext* StateChangeContext = nullptr);

private:
	/** The list of all game feature plugin state machine objects */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UGameFeaturePluginStateMachine>> GameFeaturePluginStateMachines;

	/** Game feature plugin state machine objects that are being terminated. Used to prevent GC until termination is complete. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameFeaturePluginStateMachine>> TerminalGameFeaturePluginStateMachines;

	TMap<FString, FString> GameFeaturePluginNameToPathMap;

	struct FCachedGameFeaturePluginDetails
	{
		FGameFeaturePluginDetails Details;
		FDateTime TimeStamp;
		FCachedGameFeaturePluginDetails() = default;
		FCachedGameFeaturePluginDetails(const FGameFeaturePluginDetails& InDetails, const FDateTime& InTimeStamp) : Details(InDetails), TimeStamp(InTimeStamp) {}
	};
	mutable TMap<FString, FCachedGameFeaturePluginDetails> CachedPluginDetailsByFilename;

	UPROPERTY()
	TArray<TObjectPtr<UObject>> Observers;

	UPROPERTY(Transient)
	TObjectPtr<UGameFeaturesProjectPolicies> GameSpecificPolicies;

	bool bInitializedPolicyManager = false;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "GameFeaturePluginOperationResult.h"
#endif

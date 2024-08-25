// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Union.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeaturePluginOperationResult.h"
#include "GameFeatureTypes.h"
#include "GameFeaturePluginStateMachine.generated.h"

class UGameFeatureData;
class UGameFrameworkComponentManager;
class UGameFeaturePluginStateMachine;
struct FComponentRequestHandle;
enum class EInstallBundleResult : uint32;
enum class EInstallBundleReleaseResult : uint32;

/*
*************** GameFeaturePlugin state machine graph ***************
Descriptions for each state are below in EGameFeaturePluginState.
Destination states have a *. These are the only states that external sources can ask to transition to via SetDestinationState().
Error states have !. These states become destinations if an error occurs during a transition.
Transition states are expected to transition the machine to another state after doing some work.

                         +--------------+
                         |              |
                         |Uninitialized |
                         |              |
                         +------+-------+
     +------------+             |
     |     *      |             |
     |  Terminal  <-------------~-----------------------------------------------
     |            |             |                                              |
     +--^------^--+             ----------------------------                   |
        |      |                                           |                   |
        |      |                                    +------v--------+          |
        |      |                                    |      *        |          |
        |      -------------------------------------+ UnknownStatus |          |
        |           ^                      ^        |               |          |
        |           |                      |        +-------+-------+          |
        |           |                      |                |                  |
        |    +------+-------+              |                |                  |
        |    |      *       |              |                |                  |
        |    | Uninstalled  +--------------~--------------->|                  |
        |    |              |              |                |                  |
        |    +------^-------+              |                |                  |
        |           |                      |                |                  |
        |    +------+-------+    *---------+---------+      |                  |
        |    |              |    |         !         |      |                  |
        |    | Uninstalling <----> ErrorUninstalling |      |                  |
        |    |              |    |                   |      |                  |
        |    +---^----------+    +---------+---------+      |                  |
        |        |                         |                |                  |
        |        |    ----------------------                |                  |
        |        |    |                                     |                  |
        |        |    |                     -----------------                  |
        |        |    |                     |                                  |
        |        |    |         +-----------v---+     +--------------------+   |
        |        |    |         |               |     |         !          |   |
        |        |    |         |CheckingStatus <-----> ErrorCheckingStatus+-->|
        |        |    |         |               |     |                    |   |
        |        |    |         +------+------^-+     +--------------------+   |
        |        |    |                |      |                                |
        |        |    |                |      |       +--------------------+   |
        ---------~    |                |      |       |         !          |   |
                 |    |<----------------      --------> ErrorUnavailable   +----
                 |    |                               |                    |
                 |    |                               +--------------------+
                 |    |
            +----+----v----+                            
            |      *       |
         ---> StatusKnown  +----------------------------------------------
         |  |              |                                 |           |
         |  +----------^---+                                 |           |
         |                                                   |           |
         |                                                   |           |
         |                                                   |           |
         |                                                   |           |
         |                                                   |           |
      +--+---------+      +-------------------+       +------v-------+   |
      |            |      |         !         |       |              |   |
      | Releasing  <------> ErrorManagingData <-------> Downloading  |   |
      |            |      |                   |       |              |   |
      +--^---------+      +-------------------+       +-------+------+   |
         |                                                   |           |
         |                                                   |           |
         |     +-------------+                               |           |
         |     |      *      |                               v           |
         ------+ Installed   <--------------------------------------------
               |             |
               +-^---------+-+
                 |         |
		   ------~---------~--------------------------------
           |     |         |                               |
        +--v-----+--+    +-v---------+               +-----v--------------+
        |           |    |           |				 |         !          |
        |Unmounting |    | Mounting  <---------------> ErrorMounting      |
        |           |    |           |				 |                    |
        +--^-----^--+    +--+--------+				 +--------------------+
           |     |          |
           ------~----------~-------------------------------
                 |          |                              |
                 |       +--v------------------- +   +-----v-----------------------+
                 |       |                       |	 |         !                   |
                 |       |WaitingForDependencies <---> ErrorWaitingForDependencies |
                 |       |                       |	 |                             |
                 |       +--+------------------- +	 +-----------------------------+
                 |          |
           ------~----------~-------------------------------
           |     |          |                              |
        +--v-----+----+  +--v-------- +              +-----v--------------+
        |             |  |            |				 |         !          |
        |Unregistering|  |Registering <--------------> ErrorRegistering   |
        |             |  |            |				 |                    |
        +--------^----+  ++---------- +				 +--------------------+
                 |        |
               +-+--------v-+
               |      *     |
               | Registered |
               |            |
               +-^--------+-+
                 |        |
           ------~--------~---------------------------------------
           |     |        |                               ^      |
        +--v-----+--+  +--v--------+                      |    +-+------------+
        |           |  |           |                      |    |      !       |
        | Unloading |  |  Loading  <-------------------- ~----> ErrorLoading |
        |           |  |           |                      |    |              |
        +--------^--+  +--+--------+                      |    +--------------+
                 |        |                               |
               +-+--------v-+                             |
               |      *     |                             |
               |   Loaded   |                             |
               |            |                             |
               +-^--------+-+                             |
		         |        |                               |
        +--------+---+  +-v------------------------+   +--+--------------------------+
        |            |  |                          |   |             !               |
        |Deactivating|  |  ActivatingDependencies  <---> ErrorActivatingDependencies |
        |            |  |                          |   |                             |
        +-^----------+  +---------------------+----+   +-----------------------------+
          |                                   |
		  |  +-----------------------------+  |
		  |  |              !              |  |
		  |  |ErrorDeactivatingDependencies|  |
		  |  |                             |  |
		  |  +--^--------------------------+  |
		  |     |                             |
		+-+-----v----------------+          +-v----------+
		|		                 |          |            |
		|DeactivatingDependencies|          | Activating |
		|	   	                 |          |            |
		+----------------------^-+          +---+--------+
		                       |                |
		     	             +-+----------------v-+
                             |          *         |
                             |       Active       |
                             |                    |
                             +--------------------+
*/

struct FGameFeaturePluginStateRange
{
	EGameFeaturePluginState MinState = EGameFeaturePluginState::Uninitialized;
	EGameFeaturePluginState MaxState = EGameFeaturePluginState::Uninitialized;

	FGameFeaturePluginStateRange() = default;

	FGameFeaturePluginStateRange(EGameFeaturePluginState InMinState, EGameFeaturePluginState InMaxState)
		: MinState(InMinState), MaxState(InMaxState)
	{}

	explicit FGameFeaturePluginStateRange(EGameFeaturePluginState InState)
		: MinState(InState), MaxState(InState)
	{}

	bool IsValid() const { return MinState <= MaxState; }

	bool Contains(EGameFeaturePluginState InState) const
	{
		return InState >= MinState && InState <= MaxState;
	}

	bool Overlaps(const FGameFeaturePluginStateRange& Other) const
	{
		return Other.MinState <= MaxState && Other.MaxState >= MinState;
	}

	TOptional<FGameFeaturePluginStateRange> Intersect(const FGameFeaturePluginStateRange& Other) const
	{
		TOptional<FGameFeaturePluginStateRange> Intersection;

		if (Overlaps(Other))
		{
			Intersection.Emplace(FMath::Max(Other.MinState, MinState), FMath::Min(Other.MaxState, MaxState));
		}

		return Intersection;
	}

	bool operator==(const FGameFeaturePluginStateRange& Other) const { return MinState == Other.MinState && MaxState == Other.MaxState; }
	bool operator<(const FGameFeaturePluginStateRange& Other) const { return MaxState < Other.MinState; }
	bool operator>(const FGameFeaturePluginStateRange& Other) const { return MinState > Other.MaxState; }
};

inline bool operator<(EGameFeaturePluginState State, const FGameFeaturePluginStateRange& StateRange)
{
	return State < StateRange.MinState;
}

inline bool operator<(const FGameFeaturePluginStateRange& StateRange, EGameFeaturePluginState State)
{
	return StateRange.MaxState < State;
}

inline bool operator>(EGameFeaturePluginState State, const FGameFeaturePluginStateRange& StateRange)
{
	return State > StateRange.MaxState;
}

inline bool operator>(const FGameFeaturePluginStateRange& StateRange, EGameFeaturePluginState State)
{
	return StateRange.MinState > State;
}

struct FInstallBundlePluginProtocolMetaData
{
	FInstallBundlePluginProtocolMetaData() = default;
	FInstallBundlePluginProtocolMetaData(TArray<FName> InInstallBundles) : InstallBundles(MoveTemp(InInstallBundles)) {}

	TArray<FName> InstallBundles;

	/** Functions to convert to/from the URL FString representation of this metadata **/
	FString ToString() const;
	static TValueOrError<FInstallBundlePluginProtocolMetaData, void> FromString(FStringView URLOptionsString);
};

struct FGameFeatureProtocolMetadata : public TUnion<FInstallBundlePluginProtocolMetaData, FNull>
{
	FGameFeatureProtocolMetadata() { SetSubtype<FNull>(); }
	FGameFeatureProtocolMetadata(const FInstallBundlePluginProtocolMetaData& InData) : TUnion(InData) {}
	FGameFeatureProtocolMetadata(FNull InOptions) { SetSubtype<FNull>(InOptions); }
};

/** Notification that a state transition is complete */
DECLARE_DELEGATE_TwoParams(FGameFeatureStateTransitionComplete, UGameFeaturePluginStateMachine* /*Machine*/, const UE::GameFeatures::FResult& /*Result*/);

/** Notification that a state transition is canceled */
DECLARE_DELEGATE_OneParam(FGameFeatureStateTransitionCanceled, UGameFeaturePluginStateMachine* /*Machine*/);

/** A request for other state machine dependencies */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FGameFeaturePluginRequestStateMachineDependencies, const FString& /*DependencyPluginURL*/, TArray<UGameFeaturePluginStateMachine*>& /*OutDependencyMachines*/);

/** A request to update the state machine and process states */
DECLARE_DELEGATE(FGameFeaturePluginRequestUpdateStateMachine);

/** A request to update progress for the current state */
DECLARE_DELEGATE_OneParam(FGameFeatureStateProgressUpdate, float Progress);

/** The common properties that can be accessed by the states of the state machine */
USTRUCT()
struct FGameFeaturePluginStateMachineProperties
{
	GENERATED_BODY()

	/** 
	* The Identifier used to find this Plugin. Parsed from the supplied PluginURL at creation.
	* Every protocol will have its own style of identifier URL that will get parsed to generate this.
	* For example, if the file is simply on disk, you can use file:../../../YourGameModule/Plugins/MyPlugin/MyPlugin.uplugin
	**/
	FGameFeaturePluginIdentifier PluginIdentifier;

	/** Filename on disk of the .uplugin file. */
	FString PluginInstalledFilename;
	
	/** Name of the plugin. */
	FString PluginName;

	/** Metadata parsed from the URL for a specific protocol. */
	FGameFeatureProtocolMetadata ProtocolMetadata;

	/** Additional options for a specific protocol. */
	FGameFeatureProtocolOptions ProtocolOptions;

	TArray<FName> AddedPrimaryAssetTypes;

	/** Tracks whether or not this state machine added the plugin to the plugin manager. */
	bool bAddedPluginToManager = false;

	/** Was this plugin loaded using LoadBuiltInGameFeaturePlugin */
	bool bWasLoadedAsBuiltInGameFeaturePlugin = false;

	/** Whether this state machine should attempt to cancel the current transition */
	bool bTryCancel = false;

	/** The desired state during a transition. */
	FGameFeaturePluginStateRange Destination;

	/** The data asset describing this game feature */
	UPROPERTY(Transient)
	TObjectPtr<UGameFeatureData> GameFeatureData = nullptr;

	/** Callbacks for when the current state transition is cancelled */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTransitionCanceled, UGameFeaturePluginStateMachine* /*Machine*/);
	FOnTransitionCanceled OnTransitionCanceled;

	/** Delegate to request the state machine be updated. */
	FGameFeaturePluginRequestUpdateStateMachine OnRequestUpdateStateMachine;

	/** Delegate for when a feature state needs to update progress. */
	FGameFeatureStateProgressUpdate OnFeatureStateProgressUpdate;

	FGameFeaturePluginStateMachineProperties() = default;
	FGameFeaturePluginStateMachineProperties(
		FGameFeaturePluginIdentifier InPluginIdentifier,
		const FGameFeaturePluginStateRange& DesiredDestination,
		const FGameFeaturePluginRequestUpdateStateMachine& RequestUpdateStateMachineDelegate,
		const FGameFeatureStateProgressUpdate& FeatureStateProgressUpdateDelegate);

	EGameFeaturePluginProtocol GetPluginProtocol() const;

	bool ParseURL();

	/** Checks to see if any invalid data was changed during a URL update. True if data updated was all values expected to be changed. */
	UE::GameFeatures::FResult ValidateProtocolOptionsUpdate(const FGameFeatureProtocolOptions& NewProtocolOptions) const;

	/** Returns protocol options suitable for reuse by another state machine */
	FGameFeatureProtocolOptions RecycleProtocolOptions() const;

	/** Whether this machine is allowed to be asynchronous */
	bool AllowAsyncLoading() const;
};

/** Input and output information for a state's UpdateState */
struct FGameFeaturePluginStateStatus
{
private:
	/** The state to transition to after UpdateState is complete. */
	EGameFeaturePluginState TransitionToState = EGameFeaturePluginState::Uninitialized;

	/** Holds the current error for any state transition. */
	UE::GameFeatures::FResult TransitionResult = MakeValue();

	friend class UGameFeaturePluginStateMachine;

public:
	void SetTransition(EGameFeaturePluginState InTransitionToState);
	void SetTransitionError(EGameFeaturePluginState TransitionToErrorState, UE::GameFeatures::FResult TransitionResult);
};

enum class EGameFeaturePluginStateType : uint8
{
	Transition,
	Destination,
	Error
};

struct FDestinationGameFeaturePluginState;
struct FErrorGameFeaturePluginState;

/** Base class for all game feature plugin states */
struct FGameFeaturePluginState
{
	FGameFeaturePluginState(FGameFeaturePluginStateMachineProperties& InStateProperties) : StateProperties(InStateProperties) {}
	virtual ~FGameFeaturePluginState();

	/** Called when this state becomes the active state */
	virtual void BeginState() {}

	/** Process the state's logic to decide if there should be a state transition. */
	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) {}

	/** Attempt to cancel any pending state transition. */
	virtual void TryCancelState() {}

	/** Called if we have updated the protocol options for this FGameFeaturePluginState.
		Returns false if no update occured or the update failed. True on successful update. */
	virtual UE::GameFeatures::FResult TryUpdateProtocolOptions(const FGameFeatureProtocolOptions& NewOptions);
	
	/** Called when this state is no longer the active state */
	virtual void EndState() {}

	/** Returns the type of state this is */
	virtual EGameFeaturePluginStateType GetStateType() const { return EGameFeaturePluginStateType::Transition; }

	FDestinationGameFeaturePluginState* AsDestinationState();
	FErrorGameFeaturePluginState* AsErrorState();

	/** The common properties that can be accessed by the states of the state machine */
	FGameFeaturePluginStateMachineProperties& StateProperties;

	void UpdateStateMachineDeferred(float Delay = 0.0f) const;
	void UpdateStateMachineImmediate() const;

	void UpdateProgress(float Progress) const;

protected:
	/** Builds an end FResult with some minimal error information with overrides for common types we 
		need to generate errors from */
	UE::GameFeatures::FResult GetErrorResult(const FString& ErrorCode, const FText OptionalErrorText = FText()) const;
	UE::GameFeatures::FResult GetErrorResult(const FString& ErrorNamespaceAddition, const FString& ErrorCode, const FText OptionalErrorText = FText()) const;
	UE::GameFeatures::FResult GetErrorResult(const FString& ErrorNamespaceAddition, const EInstallBundleResult ErrorResult) const;
	UE::GameFeatures::FResult GetErrorResult(const FString& ErrorNamespaceAddition, const EInstallBundleReleaseResult ErrorResult) const;

	/** Returns true if this state should transition to the Uninstalled state. 
	    returns False if it should just go directly to the Terminal state instead. */
	bool ShouldVisitUninstallStateBeforeTerminal() const;

	bool AllowIniLoading() const;

	bool AllowAsyncLoading() const;
	bool UseAsyncLoading() const;

private:
	void CleanupDeferredUpdateCallbacks() const;

	mutable FTSTicker::FDelegateHandle TickHandle;
};

/** Base class for destination game feature plugin states */
struct FDestinationGameFeaturePluginState : public FGameFeaturePluginState
{
	FDestinationGameFeaturePluginState(FGameFeaturePluginStateMachineProperties& InStateProperties) : FGameFeaturePluginState(InStateProperties) {}

	/** Returns the type of state this is */
	virtual EGameFeaturePluginStateType GetStateType() const override { return EGameFeaturePluginStateType::Destination; }

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDestinationStateReached, UGameFeaturePluginStateMachine* /*Machine*/, const UE::GameFeatures::FResult& /*Result*/);
	FOnDestinationStateReached OnDestinationStateReached;
};

/** Base class for error game feature plugin states */
struct FErrorGameFeaturePluginState : public FDestinationGameFeaturePluginState
{
	FErrorGameFeaturePluginState(FGameFeaturePluginStateMachineProperties& InStateProperties) : FDestinationGameFeaturePluginState(InStateProperties) {}

	/** Returns the type of state this is */
	virtual EGameFeaturePluginStateType GetStateType() const override { return EGameFeaturePluginStateType::Error; }
};

/** Information about a given plugin state, used to expose information to external code */
struct FGameFeaturePluginStateInfo
{
	/** The state this info represents */
	EGameFeaturePluginState State = EGameFeaturePluginState::Uninitialized;

	/** The progress of this state. Relevant only for transition states. */
	float Progress = 0.0f;

	FGameFeaturePluginStateInfo() = default;
	explicit FGameFeaturePluginStateInfo(EGameFeaturePluginState InState) : State(InState) {}
};

/** A state machine to manage transitioning a game feature plugin from just a URL into a fully loaded and active plugin, including registering its contents with other game systems */
UCLASS()
class UGameFeaturePluginStateMachine : public UObject
{
	GENERATED_BODY()

public:
	UGameFeaturePluginStateMachine(const FObjectInitializer& ObjectInitializer);

	/** Initializes the state machine and assigns the URL for the plugin it manages. This sets the machine to the 'UnknownStatus' state. */
	void InitStateMachine(FGameFeaturePluginIdentifier InPluginIdentifier, const FGameFeatureProtocolOptions& InProtocolOptions);

	/** Asynchronously transitions the state machine to the destination state range and reports when it is done. 
	  * DestinationState must be of type EGameFeaturePluginStateType::Destination.
	  * If returns true and OnFeatureStateTransitionComplete is not called immediately, OutCallbackHandle will be set
	  * Returns false and does not callback if a transition is already in progress and the destination range is not compatible with the current range. */
	bool SetDestination(FGameFeaturePluginStateRange InDestination, FGameFeatureStateTransitionComplete OnFeatureStateTransitionComplete, FDelegateHandle* OutCallbackHandle = nullptr);

	/** Cancel the current transition if possible */
	bool TryCancel(FGameFeatureStateTransitionCanceled OnFeatureStateTransitionCanceled, FDelegateHandle* OutCallbackHandle = nullptr);

	/** Update the current PluginURL data for this plugin if possible. Returns false if this update fails or
		if the supplied InPluginURL matches the existing URL data for this plugin.**/
	UE::GameFeatures::FResult TryUpdatePluginProtocolOptions(const FGameFeatureProtocolOptions& InOptions, bool& bOutDidUpdate);

	/** Remove any pending callback from SetDestination */
	void RemovePendingTransitionCallback(FDelegateHandle InHandle);

	/** Remove any pending callback from SetDestination */
	void RemovePendingTransitionCallback(void* DelegateObject);

	/** Remove any pending callback from TryCancel */
	void RemovePendingCancelCallback(FDelegateHandle InHandle);

	/** Remove any pending callback from TryCancel */
	void RemovePendingCancelCallback(void* DelegateObject);

	/** Returns the PluginIdentifier used to identify this plugin uniquely to the GameFeaturePlugin Subsystem */
	const FGameFeaturePluginIdentifier& GetPluginIdentifier() const;

	/** Returns the name of the game feature. Before StatusKnown, this returns the URL. */
	const FString& GetGameFeatureName() const;

	/** Returns the URL */
	const FString& GetPluginURL() const;

	/** Returns any protocol options */
	const FGameFeatureProtocolOptions& GetProtocolOptions() const;

	/** Returns protocol options suitable for reuse by another state machine */
	FGameFeatureProtocolOptions RecycleProtocolOptions() const;

	/** Returns the plugin name if known (plugin must have been registered to know the name). */
	const FString& GetPluginName() const;

	/** Returns the uplugin filename of the game feature. Before StatusKnown, this returns false. */
	bool GetPluginFilename(FString& OutPluginFilename) const;

	/** Returns the enum state for this machine */
	EGameFeaturePluginState GetCurrentState() const;

	/** Returns the state range this machine is trying to move to */
	FGameFeaturePluginStateRange GetDestination() const;

	/** Returns information about the current state */
	const FGameFeaturePluginStateInfo& GetCurrentStateInfo() const;

	/** Returns true if attempting to reach a new destination state */
	bool IsRunning() const;

	/** Returns true if the state is at least StatusKnown so we can query info about the game feature plugin */
	bool IsStatusKnown() const;

	/** Returns true if the plugin is available to download/load. Only call if IsStatusKnown is true */
	bool IsAvailable() const;

	/** Whether this machine is allowed to be asynchronous */
	bool AllowAsyncLoading() const;

	void SetWasLoadedAsBuiltIn();

	bool WasLoadedAsBuiltIn() const;

	/** If the plugin is activated already, we will retrieve its game feature data */
	UGameFeatureData* GetGameFeatureDataForActivePlugin();

	/** If the plugin is registered already, we will retrieve its game feature data */
	UGameFeatureData* GetGameFeatureDataForRegisteredPlugin(bool bCheckForRegistering = false);

private:
	/** Returns true if the specified state is not a transition state */
	bool IsValidTransitionState(EGameFeaturePluginState InState) const;

	/** Returns true if the specified state is a destination state */
	bool IsValidDestinationState(EGameFeaturePluginState InDestinationState) const;

	/** Returns true if the specified state is a error state */
	bool IsValidErrorState(EGameFeaturePluginState InDestinationState) const;

	/** Processes the current state and looks for state transitions */
	void UpdateStateMachine();

	/** Update Progress for current state */
	void UpdateCurrentStateProgress(float Progress);

	/** Information about the current state */
	FGameFeaturePluginStateInfo CurrentStateInfo;

	/** The common properties that can be accessed by the states of the state machine */
	UPROPERTY(transient)
	FGameFeaturePluginStateMachineProperties StateProperties;

	/** All state machine state objects */
	TUniquePtr<FGameFeaturePluginState> AllStates[EGameFeaturePluginState::MAX];

	/** True when we are currently executing UpdateStateMachine, to avoid reentry */
	bool bInUpdateStateMachine;
};

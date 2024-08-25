// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "RewindData.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Subsystems/WorldSubsystem.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Chaos/PhysicsObject.h"

#include "NetworkPhysicsComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreProcessInputsInternal, const int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostProcessInputsInternal, const int32);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInjectInputsExternal, const int32 /* PhysicsStep */, const int32 /* NumSteps */);

/** Templated data history, holding a data buffer */
template<typename DataType>
struct TNetRewindHistory : public Chaos::TDataRewindHistory<DataType>
{
	using Super = Chaos::TDataRewindHistory<DataType>;

	FORCEINLINE TNetRewindHistory(const int32 FrameCount, const bool bIsHistoryLocal) :
		Super(FrameCount, bIsHistoryLocal)
	{
	}

	FORCEINLINE virtual ~TNetRewindHistory() {}

	virtual TUniquePtr<Chaos::FBaseRewindHistory> CreateNew() const
	{
		TUniquePtr<TNetRewindHistory> Copy = MakeUnique<TNetRewindHistory>(0, Super::bIsLocalHistory);

		return Copy;
	}

	virtual TUniquePtr<Chaos::FBaseRewindHistory> Clone() const
	{
		return MakeUnique<TNetRewindHistory>(*this);
	}

	virtual void ValidateDataInHistory(const void* ActorComponent) override
	{
		const UActorComponent* NetworkComponent = static_cast<const UActorComponent*>(ActorComponent);
		for (int32 FrameIndex = 0; FrameIndex < Super::NumFrames; ++FrameIndex)
		{
			DataType& FrameData = Super::DataHistory[FrameIndex];
			FrameData.ValidateData(NetworkComponent);
		}
	}

	virtual TUniquePtr<Chaos::FBaseRewindHistory> CopyFramesWithOffset(const uint32 StartFrame, const uint32 EndFrame, const int32 FrameOffset) override
	{
		uint32 FramesCount = (uint32)Super::NumValidData(StartFrame, EndFrame);
			
		TUniquePtr<TNetRewindHistory> Copy = MakeUnique<TNetRewindHistory>(FramesCount, Super::bIsLocalHistory);

		DataType FrameData;
		for (uint32 FrameIndex = StartFrame; FrameIndex < EndFrame; ++FrameIndex)
		{
			const int32 LocalFrame = FrameIndex % Super::NumFrames;
			if (FrameIndex == Super::DataHistory[LocalFrame].LocalFrame)
			{
				FrameData = Super::DataHistory[LocalFrame];
				FrameData.ServerFrame = FrameData.LocalFrame + FrameOffset;
				PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to RecordData() in UE 5.6 and remove deprecation pragma
				Copy->RecordDatas(LocalFrame, &FrameData);
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
		}

		return Copy;
	}

	virtual int32 ReceiveNewData(Chaos::FBaseRewindHistory& NewData, const int32 FrameOffset, bool CompareDataForRewind = false) override
	{
		TNetRewindHistory& NetNewData = static_cast<TNetRewindHistory&>(NewData);

		int32 RewindFrame = INDEX_NONE;
		if (NetNewData.NumFrames > 0)
		{
			for (int32 FrameIndex = 0; FrameIndex < NetNewData.NumFrames; ++FrameIndex)
			{
				DataType& FrameData = NetNewData.DataHistory[FrameIndex];

				FrameData.LocalFrame = FrameData.ServerFrame - FrameOffset;
				if (FrameData.LocalFrame >= 0)
				{
					FrameData.bReceivedData = true; // Received data is marked to differentiate from locally predicted data

					if (CompareDataForRewind && FrameData.LocalFrame > RewindFrame && Super::TriggerRewindFromNewData(&FrameData))
					{
						RewindFrame = FrameData.LocalFrame;
					}

					PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to RecordData() in UE 5.6 and remove deprecation pragma
					Super::RecordDatas(FrameData.LocalFrame, &FrameData);
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
				}
			}
		}
		return RewindFrame;
	}

	virtual void NetSerialize(FArchive& Ar, UPackageMap* InPackageMap) override
	{
		Ar << Super::NumFrames;
		
		if (Super::NumFrames > GetMaxArraySize())
		{
			UE_LOG(LogTemp, Warning, TEXT("TNetRewindHistory: serialized array of size %d exceeds maximum size %d."), Super::NumFrames, GetMaxArraySize());
			Ar.SetError();
			return;
		}

		if (Ar.IsLoading())
		{
			Super::DataHistory.SetNum(Super::NumFrames);
		}

		for (DataType& Data : Super::DataHistory)
		{
			NetSerializeData(Data, Ar, InPackageMap);
		}
	}

	/** Debug the data from the archive */
	FORCEINLINE virtual void DebugData(const Chaos::FBaseRewindHistory& DebugHistory, TArray<int32>& LocalFrames, TArray<int32>& ServerFrames, TArray<int32>& InputFrames) override
	{
		const TNetRewindHistory& NetDebugHistory = static_cast<const TNetRewindHistory&>(DebugHistory);

		if(NetDebugHistory.NumFrames >= 0)
		{
			LocalFrames.SetNum(NetDebugHistory.NumFrames);
			ServerFrames.SetNum(NetDebugHistory.NumFrames);
			InputFrames.SetNum(NetDebugHistory.NumFrames);

			DataType FrameData;
			for (int32 FrameIndex = 0; FrameIndex < NetDebugHistory.NumFrames; ++FrameIndex)
			{
				FrameData = NetDebugHistory.DataHistory[FrameIndex];
				LocalFrames[FrameIndex] = FrameData.LocalFrame;
				ServerFrames[FrameIndex] = FrameData.ServerFrame;
				InputFrames[FrameIndex] = FrameData.InputFrame;
			}
		}
	}

private :

	/** Serialized array size limit to guard against invalid network data */
	static int32 GetMaxArraySize()
	{
		static int32 MaxArraySize = UPhysicsSettings::Get()->GetPhysicsHistoryCount() * 4;
		return MaxArraySize;
	}

	/** Use net serialize path to serialize data  */
	FORCEINLINE bool NetSerializeData(DataType& FrameData, FArchive& Ar, UPackageMap* PackageMap) const 
	{
		bool bOutSuccess = false;
		UScriptStruct* ScriptStruct = DataType::StaticStruct();
		if (ScriptStruct->StructFlags & STRUCT_NetSerializeNative)
		{
			ScriptStruct->GetCppStructOps()->NetSerialize(Ar, PackageMap, bOutSuccess, &FrameData);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("TNetRewindHistory::NetSerializeData called on data struct %s without a native NetSerialize"), *ScriptStruct->GetName());

			// Not working for now since the packagemap could be null
			// UNetConnection* Connection = CastChecked<UPackageMapClient>(PackageMap)->GetConnection();
			// UNetDriver* NetDriver = Connection ? Connection->GetDriver() : nullptr;
			// TSharedPtr<FRepLayout> RepLayout = NetDriver ? NetDriver->GetStructRepLayout(ScriptStruct) : nullptr;
			//
			// if (RepLayout.IsValid())
			// {
			// 	bool bHasUnmapped = false;
			// 	RepLayout->SerializePropertiesForStruct(ScriptStruct, Ar, PackageMap, &FrameData, bHasUnmapped);
			//
			// 	bOutSuccess = true;
			// }
		}
		return bOutSuccess;
	}
};

/**
 * Base struct for replicated rewind history properties
 */
USTRUCT()
struct FNetworkPhysicsRewindDataProxy
{
	GENERATED_BODY()

	FNetworkPhysicsRewindDataProxy& operator=(const FNetworkPhysicsRewindDataProxy& Other);

	/** Causes the history to be serialized every time. If implemented, would prevent serializing if the history hasn't changed. */
	bool operator==(const FNetworkPhysicsRewindDataProxy& Other) const { return false; }

protected:
	bool NetSerializeBase(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess, TUniqueFunction<TUniquePtr<Chaos::FBaseRewindHistory>()> CreateHistoryFunction);

public:
	/** The history to be serialized */
	TUniquePtr<Chaos::FBaseRewindHistory> History;

	/** Component that utilizes this data */
	UPROPERTY()
	TObjectPtr<UNetworkPhysicsComponent> Owner = nullptr;
};

/**
 * Struct suitable for use as a replicated property to replicate input rewind history
 */
USTRUCT()
struct FNetworkPhysicsRewindDataInputProxy : public FNetworkPhysicsRewindDataProxy
{
	GENERATED_BODY()
		
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FNetworkPhysicsRewindDataInputProxy> : public TStructOpsTypeTraitsBase2<FNetworkPhysicsRewindDataInputProxy>
{
	enum
	{
		WithNetSerializer = true,
		WithIdenticalViaEquality = true
	};
};

/**
 * Struct suitable for use as a replicated property to replicate state rewind history
 */
USTRUCT()
struct FNetworkPhysicsRewindDataStateProxy : public FNetworkPhysicsRewindDataProxy
{
	GENERATED_BODY()

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FNetworkPhysicsRewindDataStateProxy> : public TStructOpsTypeTraitsBase2<FNetworkPhysicsRewindDataStateProxy>
{
	enum
	{
		WithNetSerializer = true,
		WithIdenticalViaEquality = true
	};
};

/**
 * Network physics rewind callback to manage all the sim callbacks rewind functionalities
 */
struct FNetworkPhysicsCallback : public Chaos::IRewindCallback
{
	FNetworkPhysicsCallback(UWorld* InWorld) : World(InWorld) 
	{
		UpdateNetMode();
	}

	// Delegate on the internal inputs process 
	FOnPreProcessInputsInternal PreProcessInputsInternal;
	FOnPostProcessInputsInternal PostProcessInputsInternal;
	// Bind to this for additional processing on the GT during InjectInputs_External()
	FOnInjectInputsExternal InjectInputsExternal;

	// Rewind API
	virtual void InjectInputs_External(int32 PhysicsStep, int32 NumSteps) override;
	virtual void ProcessInputs_External(int32 PhysicsStep, const TArray<Chaos::FSimCallbackInputAndObject>& SimCallbackInputs);
	virtual void ProcessInputs_Internal(int32 PhysicsStep, const TArray<Chaos::FSimCallbackInputAndObject>& SimCallbackInputs) override;
	virtual void ApplyCallbacks_Internal(int32 PhysicsStep, const TArray<Chaos::ISimCallbackObject*>& SimCallbackObjects) override;
	virtual int32 TriggerRewindIfNeeded_Internal(int32 LatestStepCompleted) override;
	virtual void RegisterRewindableSimCallback_Internal(Chaos::ISimCallbackObject* SimCallbackObject) override
	{
		if (SimCallbackObject && SimCallbackObject->HasOption(Chaos::ESimCallbackOptions::Rewind))
		{
			RewindableCallbackObjects.Add(SimCallbackObject);
		}
	}

	virtual void UnregisterRewindableSimCallback_Internal(Chaos::ISimCallbackObject* SimCallbackObject) override
	{
		RewindableCallbackObjects.Remove(SimCallbackObject);
	}

	// Updates the TMap on PhysScene that stores (non interpolated) physics data for replication.
	// 
	// Needs to be called from PT context to access fixed tick handle
	// but also needs to be able to access GT data (actor iterator, actor state)
	void UpdateReplicationMap_Internal(int32 PhysicsStep);

	// Update client player on GT
	UE_DEPRECATED(5.4, "Physics frame offset is handled by the PlayerController automatically, it's recommended to use APlayerController::GetAsyncPhysicsTimestamp() to get the ServerFrame and LocalFrame on both client and server. Also disable the deprecated flow by setting p.net.CmdOffsetEnabled = 0")
	void UpdateClientPlayer_External(int32 PhysicsStep);

	// Update server player on GT
	UE_DEPRECATED(5.4, "Physics frame offset is handled by the PlayerController automatically, it's recommended to use APlayerController::GetAsyncPhysicsTimestamp() to get the ServerFrame and LocalFrame on both client and server. Also disable the deprecated flow by setting p.net.CmdOffsetEnabled = 0")
	void UpdateServerPlayer_External(int32 PhysicsStep);

	// Cache the current netmode for use in PT
	void UpdateNetMode()
	{
		NetMode = World ? World->GetNetMode() : ENetMode::NM_Client;
	}

	// World owning that callback
	UWorld* World = nullptr;

	// Current NetMode
	ENetMode NetMode;

	// List of rewindable sim callback objects
	TArray<Chaos::ISimCallbackObject*> RewindableCallbackObjects;
};

/**
 * Network physics manager to initialize data required for rewind/resim
 */
UCLASS(MinimalAPI)
class UNetworkPhysicsSystem : public UWorldSubsystem
{
public:

	GENERATED_BODY()
	ENGINE_API UNetworkPhysicsSystem();

	friend struct FNetworkPhysicsCallback;

	// Subsystem Init/Deinit
	ENGINE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	ENGINE_API virtual void Deinitialize() override;

	// Delegate at world init 
	ENGINE_API void OnWorldPostInit(UWorld* World, const UWorld::InitializationValues);

	// Register a component to be used by the callback 
	void RegisterNetworkComponent(class UNetworkPhysicsComponent* NetworkComponent) {NetworkComponents.Add(NetworkComponent); }

	// Remove a network component from registered list
	void UnregisterNetworkComponent(class UNetworkPhysicsComponent* NetworkComponent) { NetworkComponents.Remove(NetworkComponent); }

private:

	// List of physics network components that will be used by the rewind callback
	TArray<class UNetworkPhysicsComponent*> NetworkComponents;
};

/**
 * Base network physics data that will be used by physics
 */
USTRUCT()
struct FNetworkPhysicsData
{
	GENERATED_USTRUCT_BODY()

	virtual ~FNetworkPhysicsData() = default;

	// Server frame at which this data has been generated
	UPROPERTY()
	int32 ServerFrame = INDEX_NONE;

	// Local frame at which this data has been generated
	UPROPERTY()
	int32 LocalFrame = INDEX_NONE;

	// Input frame used to generate the network data
	UPROPERTY()
	int32 InputFrame = INDEX_NONE;

	// If this data was received over the network or locally predicted
	bool bReceivedData = false;

	// Serialize the data into/from the archive
	void SerializeFrames(FArchive& Ar)
	{
		Ar << ServerFrame;
		Ar << LocalFrame;
		Ar << InputFrame;
	}

	// Apply the data onto the network physics component
	virtual void ApplyData(UActorComponent* NetworkComponent) const
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Remove deprecation pragma and deprecated function call in UE 5.6
		ApplyDatas(NetworkComponent);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	// Build the data from the network physics component
	virtual void BuildData(const UActorComponent* NetworkComponent)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Remove deprecation pragma and deprecated function call in UE 5.6
		BuildDatas(NetworkComponent);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	
	/** Define how to interpolate between two data points if we have a gap between known data.
	* @param MinData is data from a previous frame.
	* @param MaxData is data from a future frame.
	* EXAMPLE: We have input data for frame 1 and 4 and we need to interpolate data for frame 2 and 3 based on frame 1 as MinData and frame 4 as MaxData.
	*/
	virtual void InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData) { }

	/** Use to decay desired data during resimulation if data is forward predicted.
	* @param DecayAmount = Total amount of decay as a multiplier. 10% decay = 0.1.
	* NOTE: Decay is not accumulated, the data will be in its original state each time DecayData is called. DecayAmount will increase each time the input is predicted (reused).
	* EXAMPLE: Use to decay steering inputs to make resimulation not predict too much with a high steering value. Use DecayAmount of 0.1 to turn a steering value of 0.5 into 0.45 for example.
	*/ 
	virtual void DecayData(float DecayAmount) { }
	
	/** Define how to merge data together
	* @param FromData is data from a previous frame that is getting merged into the current data.
	* EXAMPLE: Simulated proxies might receive two inputs at the same time after having used the same input twice, to not miss any important inputs we need to take both inputs into account 
	* and to not get behind in simulation we need to apply them both at the same simulation tick meaning we merge the two new inputs to one input.
	*/
	virtual void MergeData(const FNetworkPhysicsData& FromData) { }

	/** Validate data received on the server from clients
	* EXAMPLE: Validate incoming inputs from clients and correct any invalid input commands.
	* NOTE: Changes to the data in this callback will be sent from server to clients.
	*/
	virtual void ValidateData(const UActorComponent* NetworkComponent) { }

	/** Define how to compare client and server data for the same frame, returning false means the data differ enough to trigger a resimulation.
	* @param PredictedData is data predicted on the client to compare with the current data received from the server.
	*/
	virtual bool CompareData(const FNetworkPhysicsData& PredictedData) { return true; }

	/** DEPRECATED */
	UE_DEPRECATED(5.4, "Deprecated, use ApplyData instead")
	virtual void ApplyDatas(UActorComponent* NetworkComponent) const { }
	UE_DEPRECATED(5.4, "Deprecated, use BuildData instead")
	virtual void BuildDatas(const UActorComponent* NetworkComponent) { }
	UE_DEPRECATED(5.4, "Deprecated, use InterpolateData instead")
	virtual void InterpolateDatas(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData) { InterpolateData(MinData, MaxData); }

	friend UNetworkPhysicsComponent;
};

USTRUCT()
struct UE_DEPRECATED(5.4, "Deprecated, use FNetworkPhysicsData instead") FNetworkPhysicsDatas : public FNetworkPhysicsData
{
	GENERATED_USTRUCT_BODY()
	virtual ~FNetworkPhysicsDatas() = default;

	void SerializeFrames(FArchive & Ar) { FNetworkPhysicsData::SerializeFrames(Ar); }
	
	UE_DEPRECATED(5.4, "FNetworkPhysicsDatas is deprecated, use FNetworkPhysicsData instead. InterpolateDatas is also changed from a Static Polymorphic function to a Dynamic Polymorphic virtual function named InterpolateData().")
	virtual void InterpolateDatas(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData) override
	{
		ensureMsgf(false, TEXT("FNetworkPhysicsDatas is deprecated from UE 5.4, use FNetworkPhysicsData instead where InterpolateDatas() has changed from a Static Polymorphic function to a Dynamic Polymorphic virtual function named InterpolateData()."));
		FNetworkPhysicsData::InterpolateData(MinData, MaxData);
	}
};


/**
 * Network physics component that will be attached to any player controller
 */
UCLASS(BlueprintType, MinimalAPI)
class UNetworkPhysicsComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()
public:
	ENGINE_API UNetworkPhysicsComponent();

	// Get the player controller on which the component is attached
	ENGINE_API virtual APlayerController* GetPlayerController() const;

	// Init the network physics component 
	ENGINE_API void InitPhysics();

	// Server RPC to receive inputs from client
	UFUNCTION(Server, unreliable)
	ENGINE_API void ServerReceiveInputData(const FNetworkPhysicsRewindDataInputProxy& ClientInputs);

	UE_DEPRECATED(5.4, "Deprecated, use SendInputData() instead")
	UFUNCTION(Server, unreliable, meta = (DeprecatedFunction, DeprecationMessage = "ServerReceiveInputsDatas has been deprecated. Use ServerReceiveInputData instead."))
	ENGINE_API void ServerReceiveInputsDatas(const FNetworkPhysicsRewindDataInputProxy& ClientInputs);

	// Async physics tick component function per frame from the solver
	ENGINE_API virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;

	// Function to init the replicated properties
	ENGINE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifeTimeProps) const override;

	// Replicate input data 
	ENGINE_API void SendInputData();

	UE_DEPRECATED(5.4, "Deprecated, use SendInputData() instead")
	ENGINE_API void SendLocalInputsDatas() { SendInputData(); }

	// Replicate state data
	ENGINE_API void SendStateData();

	UE_DEPRECATED(5.4, "Deprecated, use SendStateData() instead")
	ENGINE_API void SendLocalStatesDatas() { SendStateData(); }

	// Delegate linked to the physics rewind callback to send record local inputs/states
	ENGINE_API void OnPreProcessInputsInternal(const int32 PhysicsStep);

	// Delegate linked to the physics rewind callback to send record local inputs/states
	ENGINE_API void OnPostProcessInputsInternal(const int32 PhysicsStep);

	// Correct the player controller Server to local offset based on the received replicated states
	UE_DEPRECATED(5.4, "Physics frame offset is handled by the PlayerController automatically, it's recommended to use APlayerController::GetAsyncPhysicsTimestamp() to get the ServerFrame and LocalFrame on both client and server. Also disable the deprecated flow by setting p.net.CmdOffsetEnabled = 0")
	ENGINE_API void CorrectServerToLocalOffset(const int32 LocalToServerOffset);

	// Used to create any physics engine information for this component 
	ENGINE_API virtual void BeginPlay() override;

	// Register the component into the network manager
	ENGINE_API virtual void InitializeComponent() override;

	// Unregister the component from the network manager
	ENGINE_API virtual void UninitializeComponent() override;

	// Register and create the states/inputs history
	template<typename PhysicsTraits>
	void CreateDataHistory(UActorComponent* HistoryComponent);
	
	template<typename PhysicsTraits>
	UE_DEPRECATED(5.4, "Deprecated, use CreateDataHistory() instead")
	void CreateDatasHistory(UActorComponent* HistoryComponent);
	
	

	// Remove state/input history from rewind data
	ENGINE_API void RemoveDataHistory();
	UE_DEPRECATED(5.4, "Deprecated, use RemoveDataHistory() instead")
	ENGINE_API void RemoveDatasHistory() { RemoveDataHistory(); }

	// Add state/input history to rewind data
	ENGINE_API void AddDataHistory();
	UE_DEPRECATED(5.4, "Deprecated, use AddDataHistory() instead")
	ENGINE_API void AddDatasHistory() { AddDataHistory(); }

	// Enable RewindData history caching and return the history size
	ENGINE_API int32 SetupRewindData();

	// Get the data factory that will be used for net serialization
	TSharedPtr<Chaos::FBaseRewindHistory>& GetStateHistory() { return StateHistory; }
	UE_DEPRECATED(5.4, "Deprecated, use GetStateHistory() instead")
	TSharedPtr<Chaos::FBaseRewindHistory>& GetStatesHistory() { return GetStateHistory(); }

	// Get the data factory that will be used for net serialization
	TSharedPtr<Chaos::FBaseRewindHistory>& GetInputHistory() { return InputHistory; }
	UE_DEPRECATED(5.4, "Deprecated, use GetInputHistory() instead")
	TSharedPtr<Chaos::FBaseRewindHistory>& GetInputsHistory() { return GetInputHistory(); }

	// Check if the world is on server
	ENGINE_API bool HasServerWorld() const;

	// Check if the player controller exists and is local
	UE_DEPRECATED(5.4, "Deprecated, use IsLocallyControlled() which takes both local player controlled and local relayed inputs into account.")
	ENGINE_API bool HasLocalController() const;
	
	// Check if this is controlled locally through relayed inputs or an existing local player controller
	ENGINE_API bool IsLocallyControlled() const;

	/** Mark this as controlled through locally relayed inputs rather than controlled as a pawn through a player controller.
	* Set if NetworkPhysicsComponent is implemented on an AActor instead of APawn and it's currently being fed inputs from the local player / autonomous proxy */
	ENGINE_API void SetIsRelayingLocalInputs(bool bInRelayingLocalInputs)
	{
		bIsRelayingLocalInputs = bInRelayingLocalInputs;
	}

	/** Check if this is controlled locally through relayed inputs from autonomous proxy. It's recommended to use IsLocallyControlled() when checking if this is locally controlled. */
	ENGINE_API const bool GetIsRelayingLocalInputs() const { return bIsRelayingLocalInputs; }

	/** Returns the current amount of input decay during resimulation as a magnitude from 0.0 to 1.0. Returns 0 if not currently resimulating. */
	ENGINE_API const float GetCurrentInputDecay(FNetworkPhysicsData* PhysicsData);

protected : 

	// repnotify for the inputs on the client
	UFUNCTION()
	ENGINE_API void OnRep_SetReplicatedInputs();

	// repnotify for the states on the client
	UFUNCTION()
	ENGINE_API void OnRep_SetReplicatedStates();

	// replicated physics inputs
	UPROPERTY(Transient, ReplicatedUsing = OnRep_SetReplicatedInputs)
	FNetworkPhysicsRewindDataInputProxy ReplicatedInputs;

	// replicated physics states 
	UPROPERTY(Transient, ReplicatedUsing = OnRep_SetReplicatedStates)
	FNetworkPhysicsRewindDataStateProxy ReplicatedStates;

	// Frame counter to compute the local to server offset
	int32 FrameCounter = 0;

private:

	friend FNetworkPhysicsCallback;
	friend struct FNetworkPhysicsRewindDataInputProxy;
	friend struct FNetworkPhysicsRewindDataStateProxy;

	// States history uses to rewind simulation 
	TSharedPtr<Chaos::FBaseRewindHistory> StateHistory;

	// Inputs history used during simulation
	TSharedPtr<Chaos::FBaseRewindHistory> InputHistory;

	// Local temporary inputs data used by pre/post process inputs functions
	TUniquePtr<FNetworkPhysicsData> InputData;

	// Local temporary states data used by pre/post process inputs functions
	TUniquePtr<FNetworkPhysicsData> StateData;

	// Send last N number of inputs each replication call to patch up holes due to packet loss
	int8 InputRedundancy = 3;

	// Current index used in the inputs offsets
	int8 InputIndex = 0;

	// Input offsets defined on PT based on the newly recorded input data
	TArray<int32> InputOffsets;

	// Send last N number of states each replication call to patch up holes due to packet loss
	int8 StateRedundancy = 1;

	// Current index used in the states offsets
	int8 StateIndex = 0;

	// State offsets defined on PT based on the newly recorded state data
	TArray<int32> StateOffsets;

	// Actor component that will be used to fill the histories
	TObjectPtr<UActorComponent> ActorComponent;

	// Root components physics object
	Chaos::FPhysicsObjectHandle RootPhysicsObject;

	// Locally relayed inputs makes this component act as if it's a locally controlled pawn.
	bool bIsRelayingLocalInputs = false;

	// Cache locally predicted states and then compare then via FNetworkPhysicsData::CompareData to trigger rewind if comparison differ
	bool bCompareStateToTriggerRewind = false;

	// Compare locally predicted inputs via FNetworkPhysicsData::CompareData to trigger rewind if comparison differ
	bool bCompareInputToTriggerRewind = false;
};

/** DEPRECATED UE 5.4 */
template<typename PhysicsTraits>
FORCEINLINE void UNetworkPhysicsComponent::CreateDatasHistory(UActorComponent* HistoryComponent)
{
	CreateDataHistory<PhysicsTraits>(HistoryComponent);
}

template<typename PhysicsTraits>
FORCEINLINE void UNetworkPhysicsComponent::CreateDataHistory(UActorComponent* HistoryComponent)
{
	const int32 NumFrames = SetupRewindData();

	APlayerController* Controller = GetPlayerController();
	const bool bIsLocalHistory = (Controller && Controller->IsLocalController()); // FIXME: The controller is null at this point, but bIsLocalHistory isn't currently used so doesn't create an issue.

	InputHistory = MakeShared<TNetRewindHistory<typename PhysicsTraits::InputsType>>(NumFrames, bIsLocalHistory);
	StateHistory = MakeShared<TNetRewindHistory<typename PhysicsTraits::StatesType>>(NumFrames, bIsLocalHistory);

	InputData = MakeUnique<typename PhysicsTraits::InputsType>();
	StateData = MakeUnique<typename PhysicsTraits::StatesType>();

	ReplicatedInputs.History = MakeUnique<TNetRewindHistory<typename PhysicsTraits::InputsType>>(NumFrames, bIsLocalHistory);
	ReplicatedInputs.Owner = this;

	ReplicatedStates.History = MakeUnique<TNetRewindHistory<typename PhysicsTraits::StatesType>>(NumFrames, bIsLocalHistory);
	ReplicatedStates.Owner = this;
	
	ActorComponent = HistoryComponent;
	
	AddDataHistory();
}

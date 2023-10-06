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

#include "NetworkPhysicsComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreProcessInputsInternal, const int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostProcessInputsInternal, const int32);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInjectInputsExternal, const int32 /* PhysicsStep */, const int32 /* NumSteps */);

/** Templated datas history holding a datas buffer */
template<typename DatasType>
struct TNetRewindHistory : public Chaos::TDatasRewindHistory<DatasType>
{
	using Super = Chaos::TDatasRewindHistory<DatasType>;

	FORCEINLINE TNetRewindHistory(const int32 FrameCount, const bool bIsHistoryLocal, UPackageMap* InPackageMap) :
		Super(FrameCount, bIsHistoryLocal)
	{
		DatasWriter.SetAllowResize(true);
		SetPackageMap(InPackageMap);
	}

	FORCEINLINE virtual ~TNetRewindHistory() {}

	/** Set the package map for serialization */
	FORCEINLINE virtual void SetPackageMap(class UPackageMap* InPackageMap) override
	{
		PackageMap = InPackageMap;

		DatasWriter.PackageMap = PackageMap;
		DatasReader.PackageMap = PackageMap;
	}

	/** Serialize the datas to the archive */
	FORCEINLINE virtual void SerializeDatas(const uint32 StartFrame, const uint32 EndFrame, TArray<uint8>& ArchiveDatas, const int32 FrameOffset) const override
	{
		if (FrameOffset >= 0)
		{
			DatasWriter.Reset();

			uint32 FramesCount = (uint32)Super::NumValidDatas(StartFrame, EndFrame);
			DatasWriter << FramesCount;

			DatasType FrameDatas;
			for (uint32 FrameIndex = StartFrame; FrameIndex < EndFrame; ++FrameIndex)
			{
				const int32 LocalFrame = FrameIndex % Super::NumFrames;
				if (FrameIndex == Super::DatasArray[LocalFrame].LocalFrame)
				{
					FrameDatas = Super::DatasArray[LocalFrame];
					FrameDatas.ServerFrame = FrameDatas.LocalFrame + FrameOffset;
					NetSerializeDatas(FrameDatas, DatasWriter);
				}
			}

			ArchiveDatas = *DatasWriter.GetBuffer();
		}
	}

	/** Deserialize the datas from the archive */
	FORCEINLINE virtual void DeserializeDatas(const TArray<uint8>& ArchiveDatas, const int32 FrameOffset) override
	{
		if ((FrameOffset >= 0) && !ArchiveDatas.IsEmpty())
		{
			// TODO : temporary copy since the archive datas are const
			TArray<uint8> LocalDatas = ArchiveDatas;
			DatasReader.SetData(LocalDatas.GetData(), ArchiveDatas.Num() * 8);

			uint32 FramesCount = 0;
			DatasReader << FramesCount;

			DatasType FrameDatas;
			for (uint32 FrameIndex = 0; FrameIndex < FramesCount; ++FrameIndex)
			{
				NetSerializeDatas(FrameDatas, DatasReader);
				FrameDatas.LocalFrame = FrameDatas.ServerFrame - FrameOffset;
				if (FrameDatas.LocalFrame >= 0)
				{
					Super::RecordDatas(FrameDatas.LocalFrame, &FrameDatas);
				}
			}
		}
	}

	/** Debug the datas from the archive */
	FORCEINLINE virtual void DebugDatas(const TArray<uint8>& ArchiveDatas, TArray<int32>& LocalFrames, TArray<int32>& ServerFrames, TArray<int32>& InputFrames) override
	{
		if((PackageMap != nullptr) && !ArchiveDatas.IsEmpty())
		{
			// TODO : temporary copy since the archive datas are const
			TArray<uint8> LocalDatas = ArchiveDatas;
			DatasReader.SetData(LocalDatas.GetData(), ArchiveDatas.Num() * 8);

			uint32 FramesCount = 0;
			DatasReader << FramesCount;

			LocalFrames.SetNum(FramesCount);
			ServerFrames.SetNum(FramesCount);
			InputFrames.SetNum(FramesCount);

			DatasType FrameDatas;
			for (uint32 FrameIndex = 0; FrameIndex < FramesCount; ++FrameIndex)
			{
				NetSerializeDatas(FrameDatas, DatasReader);
				LocalFrames[FrameIndex] = FrameDatas.LocalFrame;
				ServerFrames[FrameIndex] = FrameDatas.ServerFrame;
				InputFrames[FrameIndex] = FrameDatas.InputFrame;
			}
		}
	}

private :

	/** Use net serialize path to serialize datas  */
	FORCEINLINE bool NetSerializeDatas(DatasType& FrameDatas, FBitArchive& Ar) const 
	{
		bool bOutSuccess = false;
		UScriptStruct* ScriptStruct = DatasType::StaticStruct();
		if (ScriptStruct->StructFlags & STRUCT_NetSerializeNative)
		{
			ScriptStruct->GetCppStructOps()->NetSerialize(Ar, PackageMap, bOutSuccess, &FrameDatas);
		}
		else
		{
			UE_LOG(LogPhysics, Error, TEXT("TNetRewindHistory::NetSerializeDatas called on data struct %s without a native NetSerialize"), *ScriptStruct->GetName());

			// Not working for now since the packagemap could be null
			// UNetConnection* Connection = CastChecked<UPackageMapClient>(PackageMap)->GetConnection();
			// UNetDriver* NetDriver = Connection ? Connection->GetDriver() : nullptr;
			// TSharedPtr<FRepLayout> RepLayout = NetDriver ? NetDriver->GetStructRepLayout(ScriptStruct) : nullptr;
			//
			// if (RepLayout.IsValid())
			// {
			// 	bool bHasUnmapped = false;
			// 	RepLayout->SerializePropertiesForStruct(ScriptStruct, Ar, PackageMap, &FrameDatas, bHasUnmapped);
			//
			// 	bOutSuccess = true;
			// }
		}
		return bOutSuccess;
	}

	// Datas bits writer to be used for serialization
	mutable FNetBitWriter DatasWriter;

	// Datas bits reader to be used for serialization
	mutable FNetBitReader DatasReader;

	// Package map used for the net bit writer/reader
	UPackageMap* PackageMap = nullptr;
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

	// Updates the TMap on PhysScene that stores (non interpolated) physics data for replication.
	// 
	// Needs to be called from PT context to access fixed tick handle
	// but also needs to be able to access GT data (actor iterator, actor state)
	void UpdateReplicationMap_Internal(int32 PhysicsStep);

	// Update client player on GT
	void UpdateClientPlayer_External(int32 PhysicsStep);

	// Update server player on GT
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
 * Network physics manager to initialize datas required for rewind/resim
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
 * Base network physics datas that will be used by physics
 */
 USTRUCT()
struct FNetworkPhysicsDatas
{
	GENERATED_USTRUCT_BODY()

	virtual ~FNetworkPhysicsDatas() = default;

	// Server frame at which this datas has been generated
	UPROPERTY()
	int32 ServerFrame = INDEX_NONE;

	// Local frame at which this datas has been generated
	UPROPERTY()
	int32 LocalFrame = INDEX_NONE;

	// Input frame used to generate the network datas
	UPROPERTY()
	int32 InputFrame = INDEX_NONE;

	// Serialize the datas into/from the archive
	void SerializeFrames(FArchive& Ar)
	{
		Ar << ServerFrame;
		Ar << LocalFrame;
		Ar << InputFrame;
	}

	// Apply the datas from onto the network physics component
	virtual void ApplyDatas(UActorComponent* NetworkComponent) const {}

	// Build the datas from the network physics component
	virtual void BuildDatas(const UActorComponent* NetworkComponent) {}

	friend UNetworkPhysicsComponent;
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
	UFUNCTION(Server, unreliable, WithValidation)
	ENGINE_API void ServerReceiveInputsDatas(const TArray<uint8>& ClientInputs);

	// Async physics tick component function per frame from the solver
	ENGINE_API virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;

	// Function to init the replicated properties
	ENGINE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifeTimeProps) const override;

	// Send the inputs replicated datas
	ENGINE_API void SendLocalInputsDatas();

	// Send the states replicated datas
	ENGINE_API void SendLocalStatesDatas();

	// Delegate linked to the physics rewind callback to send record local inputs/states
	ENGINE_API void OnPreProcessInputsInternal(const int32 PhysicsStep);

	// Delegate linked to the physics rewind callback to send record local inputs/states
	ENGINE_API void OnPostProcessInputsInternal(const int32 PhysicsStep);

	// Correct the player controller Server to local offset based on the received replicated states
	ENGINE_API void CorrectServerToLocalOffset(const int32 LocalToServerOffset);

	// Used to create any physics engine information for this component 
	ENGINE_API virtual void BeginPlay() override;

	// Register the component into the network manager
	ENGINE_API virtual void InitializeComponent() override;

	// Unregister the component from the network manager
	ENGINE_API virtual void UninitializeComponent() override;

	// Register and create the states/inputs history
	template<typename PhysicsTraits>
	void CreateDatasHistory(UActorComponent* HistoryComponent);

	// Remove states/inputs history from rewind datas
	ENGINE_API void RemoveDatasHistory();

	// Add states/inputs history to rewind datas
	ENGINE_API void AddDatasHistory();

	// Get the datas factory that will be used for net serialization
	TSharedPtr<Chaos::FBaseRewindHistory>& GetStatesHistory() { return StatesHistory; }

	// Get the datas factory that will be used for net serialization
	TSharedPtr<Chaos::FBaseRewindHistory>& GetInputsHistory() { return InputsHistory; }

	// Check if the world is on server
	ENGINE_API bool HasServerWorld() const;

	// Check if the player controller exists and is local
	ENGINE_API bool HasLocalController() const;

protected : 

	// Update the histories packagemap for serialization 
	ENGINE_API void UpdatePackageMap();

	// repnotify for the inputs on the client
	UFUNCTION()
	ENGINE_API void OnRep_SetReplicatedInputs();

	// repnotify for the states on the client
	UFUNCTION()
	ENGINE_API void OnRep_SetReplicatedStates();

	// replicated physics inputs
	UPROPERTY(Transient, ReplicatedUsing = OnRep_SetReplicatedInputs)
	TArray<uint8> ReplicatedInputs;

	// replicated physics states 
	UPROPERTY(Transient, ReplicatedUsing = OnRep_SetReplicatedStates)
	TArray<uint8> ReplicatedStates;

	// List of inputs that are processed locally
	TArray<uint8> LocalInputs;

	// List of states that are processed locally
	TArray<uint8> LocalStates;

	// Frame counter to compute the local to server offset
	int32 FrameCounter = 0;

private:

	friend FNetworkPhysicsCallback;

	// States history uses to rewind simulation 
	TSharedPtr<Chaos::FBaseRewindHistory> StatesHistory;

	// Inputs history used during simulation
	TSharedPtr<Chaos::FBaseRewindHistory> InputsHistory;

	// Local temporary inputs datas used by pre/post process inputs functions
	TUniquePtr<FNetworkPhysicsDatas> InputsDatas;

	// Local temporary states datas used by pre/post process inputs functions
	TUniquePtr<FNetworkPhysicsDatas> StatesDatas;

	// Specify how much times the network will resend the inputs in case of packet loss
	int8 InputsRedundancy = 4;

	// Current index used in the inputs offsets
	int8 InputsIndex = 0;

	// Inputs offsets defined on PT based on the newly recorded inputs datas
	TArray<int32> InputsOffsets;

	// Specify how much times the network will resend the states in case of packet loss
	int8 StatesRedundancy = 1;

	// Current index used in the states offsets
	int8 StatesIndex = 0;

	// States offsets defined on PT based on the newly recorded states datas
	TArray<int32> StatesOffsets;

	// Actor component that will be used to fill the histories
	TObjectPtr<UActorComponent> ActorComponent;
};

template<typename PhysicsTraits>
FORCEINLINE void UNetworkPhysicsComponent::CreateDatasHistory(UActorComponent* HistoryComponent)
{
	APlayerController* Controller = GetPlayerController();
	const bool bIsLocalHistory = (Controller && Controller->IsLocalController());
	const int32 NumFrames = UPhysicsSettings::Get()->GetPhysicsHistoryCount();
	
	UPackageMap* PackageMap = (Controller  && Controller->GetNetConnection())? Controller->GetNetConnection()->PackageMap : nullptr;

	InputsHistory = MakeShared<TNetRewindHistory<typename PhysicsTraits::InputsType>>(NumFrames, bIsLocalHistory, PackageMap);
	StatesHistory = MakeShared<TNetRewindHistory<typename PhysicsTraits::StatesType>>(NumFrames, bIsLocalHistory, PackageMap);

	InputsDatas = MakeUnique<typename PhysicsTraits::InputsType>();
	StatesDatas = MakeUnique<typename PhysicsTraits::StatesType>();

	ActorComponent = HistoryComponent;
	
	AddDatasHistory();
}

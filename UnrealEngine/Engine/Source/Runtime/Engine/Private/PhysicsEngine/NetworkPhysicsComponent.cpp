// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/NetworkPhysicsComponent.h"

#include "Components/PrimitiveComponent.h"
#include "EngineLogs.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PBDRigidsSolver.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsReplication.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NetworkPhysicsComponent)

namespace InputCmdCVars
{
	static int32 ForceFault = 0;
	static FAutoConsoleVariableRef CVarForceFault(TEXT("p.net.ForceFault"), ForceFault, TEXT("Forces server side input fault"));

	static int32 MaxBufferedCmds = 16;
	static FAutoConsoleVariableRef CVarMaxBufferedCmds(TEXT("p.net.MaxBufferedCmds"), MaxBufferedCmds, TEXT("MaxNumber of buffered server side commands"));

	static int32 TimeDilationEnabled = 0;
	static FAutoConsoleVariableRef CVarTimeDilationEnabled(TEXT("p.net.TimeDilationEnabled"), TimeDilationEnabled, TEXT("Enable clientside TimeDilation"));

	static float MaxTargetNumBufferedCmds = 5.0;
	static FAutoConsoleVariableRef CVarMaxTargetNumBufferedCmds(TEXT("p.net.MaxTargetNumBufferedCmds"), MaxTargetNumBufferedCmds, TEXT("Maximum number of buffered inputs the server will target per client."));

	static float MaxTimeDilationMag = 0.01f;
	static FAutoConsoleVariableRef CVarMaxTimeDilationMag(TEXT("p.net.MaxTimeDilationMag"), MaxTimeDilationMag, TEXT("Maximum time dilation that client will use to slow down / catch up with server"));

	static float TimeDilationAlpha = 0.1f;
	static FAutoConsoleVariableRef CVarTimeDilationAlpha(TEXT("p.net.TimeDilationAlpha"), TimeDilationAlpha, TEXT("Lerp strength for sliding client time dilation"));

	static float TargetNumBufferedCmdsDeltaOnFault = 1.0f;
	static FAutoConsoleVariableRef CVarTargetNumBufferedCmdsDeltaOnFault(TEXT("p.net.TargetNumBufferedCmdsDeltaOnFault"), TargetNumBufferedCmdsDeltaOnFault, TEXT("How much to increase TargetNumBufferedCmds when an input fault occurs"));

	static float TargetNumBufferedCmds = 1.9f;
	static FAutoConsoleVariableRef CVarTargetNumBufferedCmds(TEXT("p.net.TargetNumBufferedCmds"), TargetNumBufferedCmds, TEXT("How much to increase TargetNumBufferedCmds when an input fault occurs"));

	static float TargetNumBufferedCmdsAlpha = 0.005f;
	static FAutoConsoleVariableRef CVarTargetNumBufferedCmdsAlpha(TEXT("p.net.TargetNumBufferedCmdsAlpha"), TargetNumBufferedCmdsAlpha, TEXT("Lerp strength for TargetNumBufferedCmds"));

	static int32 LerpTargetNumBufferedCmdsAggresively = 0;
	static FAutoConsoleVariableRef CVarLerpTargetNumBufferedCmdsAggresively(TEXT("p.net.LerpTargetNumBufferedCmdsAggresively"), LerpTargetNumBufferedCmdsAggresively, TEXT("Aggresively lerp towards TargetNumBufferedCmds. Reduces server side buffering but can cause more artifacts."));
}

// --------------------------------------------------------------------------------------------------------------------------------------------------
//	Client InputCmd Stream stuff
// --------------------------------------------------------------------------------------------------------------------------------------------------

namespace
{

int8 QuantizeTimeDilation(float F)
{
	if (F == 1.f)
	{
		return 0;
	}

	float Normalized = FMath::Clamp<float>((F - 1.f) / InputCmdCVars::MaxTimeDilationMag, -1.f, 1.f);
	return (int8)(Normalized * 128.f);
}

float DeQuantizeTimeDilation(int8 i)
{
	if (i == 0)
	{
		return 1.f;
	}

	float Normalized = (float)i / 128.f;
	float Uncompressed = 1.f + (Normalized * InputCmdCVars::MaxTimeDilationMag);
	return Uncompressed;
}

}

// after presimulate internal (asyncinput internal simulation done and the output created)
void FNetworkPhysicsCallback::ApplyCallbacks_Internal(int32 PhysicsStep, const TArray<Chaos::ISimCallbackObject*>& SimCallbackObjects)
{
	QUICK_SCOPE_CYCLE_COUNTER(NetworkPhysicsComponent_ApplyCallbacks_Internal);
	UpdateNetMode();

	if ((NetMode == NM_ListenServer) || (NetMode == NM_DedicatedServer))
	{
		if (FPhysScene_Chaos* Scene = static_cast<FPhysScene_Chaos*>(World->GetPhysicsScene()))
		{
			Scene->PopulateReplicationCache(PhysicsStep);
		}
	}
}

// before presimulate internal (asyncinput internal simulation is not done yet)
void FNetworkPhysicsCallback::ProcessInputs_Internal(int32 PhysicsStep, const TArray<Chaos::FSimCallbackInputAndObject>& SimCallbacks)
{
	PreProcessInputsInternal.Broadcast(PhysicsStep);
	for (Chaos::ISimCallbackObject* SimCallbackObject : RewindableCallbackObjects)
	{
		SimCallbackObject->ProcessInputs_Internal(PhysicsStep);
	}
	PostProcessInputsInternal.Broadcast(PhysicsStep);
}

int32 FNetworkPhysicsCallback::TriggerRewindIfNeeded_Internal(int32 LatestStepCompleted)
{
	int32 ResimFrame = INDEX_NONE;
	for (Chaos::ISimCallbackObject* SimCallbackObject : RewindableCallbackObjects)
	{
		const int32 CallbackFrame = SimCallbackObject->TriggerRewindIfNeeded_Internal(LatestStepCompleted);
		ResimFrame = (ResimFrame == INDEX_NONE) ? CallbackFrame : FMath::Min(CallbackFrame, ResimFrame);
	}

#if DEBUG_NETWORK_PHYSICS
	UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | TriggerRewindIfNeeded_Internal | Callbacks Frame = %d"), ResimFrame);
#endif

	if (RewindData)
	{
		if (NetMode == NM_Client)
		{
			const int32 ReplicationFrame = RewindData->GetResimFrame();

#if DEBUG_NETWORK_PHYSICS
			UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | TriggerRewindIfNeeded_Internal | Replication Frame = %d"), ReplicationFrame);
#endif
			ResimFrame = (ResimFrame == INDEX_NONE) ? ReplicationFrame : (ReplicationFrame == INDEX_NONE) ? ResimFrame : FMath::Min(ReplicationFrame, ResimFrame);
			RewindData->SetResimFrame(INDEX_NONE);
		}

		if (ResimFrame != INDEX_NONE)
		{
			const int32 ValidFrame = RewindData->FindValidResimFrame(ResimFrame);
#if DEBUG_NETWORK_PHYSICS
			UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | TriggerRewindIfNeeded_Internal | Resim Frame = %d | Valid Frame = %d"), ResimFrame, ValidFrame);
#endif
			ResimFrame = ValidFrame;
		}
	}
	
	return ResimFrame;
}

void FNetworkPhysicsCallback::UpdateClientPlayer_External(int32 PhysicsStep)
{
	int32 LocalOffset = 0;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		// ------------------------------------------------
		// Send RPC to server telling them what (client/local) physics step we are running
		//	* Note that SendData is empty because of the existing API, should change this
		// ------------------------------------------------	

		TArray<uint8> SendData;
		PC->PushClientInput(PhysicsStep, SendData);

		// -----------------------------------------------------------------
		// Calculate latest frame offset to map server frame to local frame
		// -----------------------------------------------------------------
		APlayerController::FClientFrameInfo& ClientFrameInfo = PC->GetClientFrameInfo();

		// -----------------------------------------------------------------
		// Apply local TIme Dilation based on server's recommendation.
		// This speeds up or slows down our consumption of real time (by like < 1%)
		// Ultimately this causes us to send InputCmds at a lower or higher rate in
		// order to keep server side buffer at optimal capacity. 
		// Optimal capacity = as small as possible without ever "missing" a frame (e.g, minimal buffer yet always a new fresh cmd to consume server side)
		// -----------------------------------------------------------------
		const float RealTimeDilation = DeQuantizeTimeDilation(ClientFrameInfo.QuantizedTimeDilation);

		if (InputCmdCVars::TimeDilationEnabled > 0)
		{
			FPhysScene* PhysScene = World->GetPhysicsScene();
			PhysScene->SetNetworkDeltaTimeScale(RealTimeDilation);
		}
	}
}

void FNetworkPhysicsCallback::UpdateServerPlayer_External(int32 PhysicsStep)
{
	// -----------------------------------------------
	// Server: "consume" an InputCmd from each Player Controller
	// All this means in this context is updating FServerFrameInfo:: LastProcessedInputFrame, LastLocalFrame
	// (E.g, telling each client what "Input" of theirs we were processing and our local physics frame number.
	// In cases where the buffer has a fault, we calculate a suggested time dilation to temporarily make client speed up 
	// or slow down their input cmd production.
	// -----------------------------------------------

	const bool bForceFault = InputCmdCVars::ForceFault > 0;
	InputCmdCVars::ForceFault = FMath::Max(0, InputCmdCVars::ForceFault - 1);

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (APlayerController* PC = Iterator->Get())
		{
			APlayerController::FServerFrameInfo& FrameInfo = PC->GetServerFrameInfo();
			APlayerController::FInputCmdBuffer& InputBuffer = PC->GetInputBuffer();

			{
				const int32 NumBufferedInputCmds = bForceFault ? 0 : (InputBuffer.HeadFrame() - FrameInfo.LastProcessedInputFrame);

				// Check Overflow
				if (NumBufferedInputCmds > InputCmdCVars::MaxBufferedCmds)
				{
					UE_LOG(LogPhysics, Warning, TEXT("[Remote.Input] overflow %d %d -> %d"), InputBuffer.HeadFrame(), FrameInfo.LastProcessedInputFrame, NumBufferedInputCmds);
					FrameInfo.LastProcessedInputFrame = InputBuffer.HeadFrame() - InputCmdCVars::MaxBufferedCmds + 1;
				}

				// Check fault - we are waiting for Cmds to reach TargetNumBufferedCmds before continuing
				if (FrameInfo.bFault)
				{
					if (NumBufferedInputCmds < (int32)FrameInfo.TargetNumBufferedCmds)
					{
						// Skip this because it is in fault. We will use the prev input for this frame.
						UE_CLOG(FrameInfo.LastProcessedInputFrame != INDEX_NONE, LogPhysics, Warning, TEXT("[Remote.Input] in fault. Reusing Inputcmd. (Client) Input: %d. (Server) Local Frame: %d"), FrameInfo.LastProcessedInputFrame, FrameInfo.LastLocalFrame);
						continue;
					}
					FrameInfo.bFault = false;
				}
				else if (NumBufferedInputCmds <= 0)
				{
					// No Cmds to process, enter fault state. Increment TargetNumBufferedCmds each time this happens.
					// TODO: We should have something to bring this back down (which means skipping frames) we don't want temporary poor conditions to cause permanent high input buffering
					FrameInfo.bFault = true;
					FrameInfo.TargetNumBufferedCmds = FMath::Min(FrameInfo.TargetNumBufferedCmds + InputCmdCVars::TargetNumBufferedCmdsDeltaOnFault, InputCmdCVars::MaxTargetNumBufferedCmds);

					UE_CLOG(FrameInfo.LastProcessedInputFrame != INDEX_NONE, LogPhysics, Warning, TEXT("[Remote.Input] ENTERING fault. New Target: %.2f. (Client) Input: %d. (Server) Local Frame: %d"), FrameInfo.TargetNumBufferedCmds, FrameInfo.LastProcessedInputFrame, FrameInfo.LastLocalFrame);
					continue;
				}

				float TargetTimeDilation = 1.f;
				if (NumBufferedInputCmds < (int32)FrameInfo.TargetNumBufferedCmds)
				{
					TargetTimeDilation += InputCmdCVars::MaxTimeDilationMag; // Tell client to speed up, we are starved on cmds
				}

				FrameInfo.TargetTimeDilation = FMath::Lerp(FrameInfo.TargetTimeDilation, TargetTimeDilation, InputCmdCVars::TimeDilationAlpha);
				FrameInfo.QuantizedTimeDilation = QuantizeTimeDilation(TargetTimeDilation);

				if (InputCmdCVars::LerpTargetNumBufferedCmdsAggresively != 0)
				{
					// When aggressive, always lerp towards target
					FrameInfo.TargetNumBufferedCmds = FMath::Lerp(FrameInfo.TargetNumBufferedCmds, InputCmdCVars::TargetNumBufferedCmds, InputCmdCVars::TargetNumBufferedCmdsAlpha);
				}

				FrameInfo.LastProcessedInputFrame++;
				FrameInfo.LastLocalFrame = PhysicsStep;
			}
		}
	}
}

void FNetworkPhysicsCallback::InjectInputs_External(int32 PhysicsStep, int32 NumSteps)
{
	InjectInputsExternal.Broadcast(PhysicsStep, NumSteps);
}

void FNetworkPhysicsCallback::ProcessInputs_External(int32 PhysicsStep, const TArray<Chaos::FSimCallbackInputAndObject>& SimCallbackInputs)
{
	for(const Chaos::FSimCallbackInputAndObject& SimCallbackObject : SimCallbackInputs)
	{
		if (SimCallbackObject.CallbackObject && SimCallbackObject.CallbackObject->HasOption(Chaos::ESimCallbackOptions::Rewind))
		{
			SimCallbackObject.CallbackObject->ProcessInputs_External(PhysicsStep);
		}
	}

	if (NetMode == NM_Client)
	{
		UpdateClientPlayer_External(PhysicsStep);
	}
	else
	{
		UpdateServerPlayer_External(PhysicsStep);
	}
}

UNetworkPhysicsSystem::UNetworkPhysicsSystem()
{}

void UNetworkPhysicsSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UWorld* World = GetWorld();
	check(World);

	if (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
	{
		FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UNetworkPhysicsSystem::OnWorldPostInit);
	}
}

void UNetworkPhysicsSystem::Deinitialize()
{}

void UNetworkPhysicsSystem::OnWorldPostInit(UWorld* World, const UWorld::InitializationValues)
{
	if (World != GetWorld())
	{
		return;
	}

	if(UPhysicsSettings::Get()->PhysicsPrediction.bEnablePhysicsPrediction)
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if(Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{ 
				if (Solver->GetRewindCallback() == nullptr)
				{
					const int32 NumFrames = FMath::Max<int32>(1, UPhysicsSettings::Get()->GetPhysicsHistoryCount());
					Solver->EnableRewindCapture(NumFrames, true, MakeUnique<FNetworkPhysicsCallback>(World));
				}
			}
		}
	}
}

UNetworkPhysicsComponent::UNetworkPhysicsComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitPhysics();
}

UNetworkPhysicsComponent::UNetworkPhysicsComponent() : Super()
{
	InitPhysics();
}

void UNetworkPhysicsComponent::InitPhysics()
{
	bAutoActivate = true;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
	SetAsyncPhysicsTickEnabled(true);

	StatesOffsets.SetNumZeroed(StatesRedundancy + 1);
	InputsOffsets.SetNumZeroed(InputsRedundancy + 1);

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		FRepMovement& RepMovement = Pawn->GetReplicatedMovement_Mutable();
		RepMovement.LocationQuantizationLevel = EVectorQuantization::RoundTwoDecimals;
		RepMovement.RotationQuantizationLevel = ERotatorQuantization::ShortComponents;
		RepMovement.VelocityQuantizationLevel = EVectorQuantization::RoundTwoDecimals;
	}
}

void UNetworkPhysicsComponent::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();

	if (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				if(FNetworkPhysicsCallback* SolverCallback = static_cast<FNetworkPhysicsCallback*>(Solver->GetRewindCallback()))
				{
					SolverCallback->PreProcessInputsInternal.AddUObject(this, &UNetworkPhysicsComponent::OnPreProcessInputsInternal);
					SolverCallback->PostProcessInputsInternal.AddUObject(this, &UNetworkPhysicsComponent::OnPostProcessInputsInternal);
				}
			}
		}
	}
}

void UNetworkPhysicsComponent::InitializeComponent()
{
	Super::InitializeComponent();
	UWorld* World = GetWorld();

	if (UNetworkPhysicsSystem* NetworkManager = World->GetSubsystem<UNetworkPhysicsSystem>())
	{
		NetworkManager->RegisterNetworkComponent(this);
	}
}

void UNetworkPhysicsComponent::UninitializeComponent()
{
	Super::UninitializeComponent();
	UWorld* World = GetWorld();

	if (UNetworkPhysicsSystem* NetworkManager = World->GetSubsystem<UNetworkPhysicsSystem>())
	{
		NetworkManager->UnregisterNetworkComponent(this);
	}
}

void UNetworkPhysicsComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UNetworkPhysicsComponent, ReplicatedInputs, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNetworkPhysicsComponent, ReplicatedStates, COND_None, REPNOTIFY_Always);
}

void UNetworkPhysicsComponent::UpdatePackageMap()
{
	APlayerController* Controller = GetPlayerController();
	UPackageMap* PackageMap = (Controller && Controller->GetNetConnection()) ? Controller->GetNetConnection()->PackageMap : nullptr;

	if(InputsHistory && StatesHistory)
	{
		InputsHistory->SetPackageMap(PackageMap);
		StatesHistory->SetPackageMap(PackageMap);
	}
}

void UNetworkPhysicsComponent::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(NetworkPhysicsComponent_AsyncPhysicsTick);

	Super ::AsyncPhysicsTickComponent(DeltaTime, SimTime);
#if DEBUG_NETWORK_PHYSICS
	if(HasServerWorld() && !HasLocalController() && InputsHistory)
	{
		TArray<int32> LocalFrames, ServerFrames, InputFrames;
		InputsHistory->DebugDatas(ReplicatedInputs, LocalFrames, ServerFrames, InputFrames);

		UE_LOG(LogTemp, Log, TEXT("SERVER | PT | AsyncPhysicsTickComponent | Receiving %d inputs from CLIENT | Component = %s"), LocalFrames.Num(), *GetFullName());
		for (int32 FrameIndex = 0; FrameIndex < LocalFrames.Num(); ++FrameIndex)
		{
			UE_LOG(LogTemp, Log, TEXT("		Debugging replicated inputs at local frame = %d | server frame = %d | Component = %s"),
				LocalFrames[FrameIndex], ServerFrames[FrameIndex], *GetFullName());
		}
	}
#endif
	// Update the package map for serialization
	UpdatePackageMap();

	// Record the received states from the server into the history for future use
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if(!PhysScene->GetSolver()->GetEvolution()->IsResimming())
			{
				// Send the inputs across the network
				SendLocalInputsDatas();

				// Send the states across the network
				SendLocalStatesDatas();

				// Advance the NetworkIndex
				InputsIndex = (InputsIndex + 1) % (InputsRedundancy + 1);
				StatesIndex = (StatesIndex + 1) % (StatesRedundancy + 1);
			}
		}
	}
}

void UNetworkPhysicsComponent::SendLocalInputsDatas()
{
	if (HasLocalController() && InputsHistory)
	{
		// We just check that the local client to server offset is valid before doing something
		const int32 LocalOffset = HasServerWorld() ? 0 : GetPlayerController()->GetLocalToServerAsyncPhysicsTickOffset();

		if (LocalOffset >= 0)
		{
			const int32 NextIndex = (InputsIndex + 1) % (InputsRedundancy + 1);
			InputsHistory->SerializeDatas(InputsOffsets[NextIndex], InputsOffsets[InputsIndex], LocalInputs, LocalOffset);

			if (HasServerWorld())
			{
				// if on server (Listen server) we should send the inputs onto all the clients through repnotify
				ReplicatedInputs = LocalInputs;
			}
			else
			{
#if DEBUG_NETWORK_PHYSICS
				if (APlayerController* PlayerController = GetPlayerController())
				{
					FAsyncPhysicsTimestamp Timestamp = PlayerController->GetAsyncPhysicsTimestamp();

					TArray<int32> LocalFrames, ServerFrames, InputFrames;
					InputsHistory->DebugDatas(LocalInputs, LocalFrames, ServerFrames, InputFrames);

					UE_LOG(LogTemp, Log, TEXT("CLIENT | GT | SendLocalInputsDatas | Sending %d inputs from CLIENT | Component = %s"), LocalFrames.Num(), *GetFullName());
					for (int32 FrameIndex = 0; FrameIndex < LocalFrames.Num(); ++FrameIndex)
					{
						UE_LOG(LogTemp, Log, TEXT("		Debugging local inputs at local frame = %d | server frame = %d | Current local frame = %d | Current server frame = %d"),
							LocalFrames[FrameIndex], ServerFrames[FrameIndex], Timestamp.LocalFrame, Timestamp.ServerFrame);
					}
				}
#endif

				// if on the client we should first send the replicated inputs onto the server
				// the RPC will then resend them onto all the other clients (except the local ones)
				ServerReceiveInputsDatas(LocalInputs);
			}
		}
	}
}

void UNetworkPhysicsComponent::SendLocalStatesDatas()
{
	if (HasServerWorld() && StatesHistory)
	{
		const int32 NextIndex = (StatesIndex + 1) % (StatesRedundancy + 1);
		StatesHistory->SerializeDatas(StatesOffsets[NextIndex], StatesOffsets[StatesIndex], LocalStates, 0);

		// if on server we should send the states onto all the clients through repnotify
		ReplicatedStates = LocalStates;
	}
}

void UNetworkPhysicsComponent::CorrectServerToLocalOffset(const int32 LocalToServerOffset)
{
	if (HasLocalController() && !HasServerWorld() && StatesHistory)
	{
		const TArray<uint8> ReceivedStates = ReplicatedStates;

		TArray<int32> LocalFrames, ServerFrames, InputFrames;
		StatesHistory->DebugDatas(ReceivedStates, LocalFrames, ServerFrames, InputFrames);

		int32 ServerToLocalOffset = LocalToServerOffset;
		for (int32 FrameIndex = 0; FrameIndex < LocalFrames.Num(); ++FrameIndex)
		{
#if DEBUG_NETWORK_PHYSICS
			UE_LOG(LogTemp, Log, TEXT("CLIENT | GT | CorrectServerToLocalOffset | Server frame = %d | Client Frame = %d"), ServerFrames[FrameIndex], InputFrames[FrameIndex]);
#endif
			ServerToLocalOffset = FMath::Min(ServerToLocalOffset, ServerFrames[FrameIndex] - InputFrames[FrameIndex]);
		}

		GetPlayerController()->SetServerToLocalAsyncPhysicsTickOffset(ServerToLocalOffset);
#if DEBUG_NETWORK_PHYSICS
		UE_LOG(LogTemp, Log, TEXT("CLIENT | GT | CorrectServerToLocalOffset | Server to local offset = %d | Local to server offset = %d"), ServerToLocalOffset, LocalToServerOffset);
#endif
	}
}

void UNetworkPhysicsComponent::OnRep_SetReplicatedStates()
{
	// The replicated states should only be used on the client since the server already have authoritative local ones
	if (!HasServerWorld() && StatesHistory)
	{
		APlayerController* PlayerController = GetPlayerController();
		if (!PlayerController)
		{
			PlayerController = GetWorld()->GetFirstPlayerController();
		}
		CorrectServerToLocalOffset(PlayerController->GetLocalToServerAsyncPhysicsTickOffset());
		const int32 LocalOffset = PlayerController->GetServerToLocalAsyncPhysicsTickOffset();

		// Record the received states from the server into the history for future use
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				const TArray<uint8> ReceivedStates = ReplicatedStates;
				PhysScene->EnqueueAsyncPhysicsCommand(0, this, [this, PhysScene, ReceivedStates, LocalOffset]()
				{
					StatesHistory->DeserializeDatas(ReceivedStates, LocalOffset);
#if DEBUG_NETWORK_PHYSICS
					{
						TArray<int32> LocalFrames, ServerFrames, InputFrames;
						StatesHistory->DebugDatas(ReceivedStates, LocalFrames, ServerFrames, InputFrames);

						UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | OnRep_SetReplicatedStates | Receiving %d states from SERVER | Local offset = %d | Component = %s "), LocalFrames.Num(), LocalOffset, *GetFullName());
						for (int32 FrameIndex = 0; FrameIndex < LocalFrames.Num(); ++FrameIndex)
						{
							UE_LOG(LogTemp, Log, TEXT("		Recording replicated states at local frame = %d | server frame = %d | life time = %d | Component = %s"), ServerFrames[FrameIndex] - LocalOffset, ServerFrames[FrameIndex], InputFrames[FrameIndex], *GetFullName());

							if (HasLocalController() && (InputFrames[FrameIndex] != (ServerFrames[FrameIndex] - LocalOffset)))
							{
								UE_LOG(LogTemp, Log, TEXT("		Bad local frame compared to input frame!!!"));
							}
						}
					}
#endif
				}, false);
			}
		}
	}
}

void UNetworkPhysicsComponent::OnRep_SetReplicatedInputs()
{
	// For local controller we should already have correct replicated inputs
	if (!HasLocalController() && !HasServerWorld() && InputsHistory)
	{
		APlayerController* PlayerController = GetPlayerController();
		if(!PlayerController)
		{
			PlayerController = GetWorld()->GetFirstPlayerController();
		}
		const int32 LocalOffset = PlayerController->GetServerToLocalAsyncPhysicsTickOffset();

		// Record the received inputs from the server into the history for future use
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				const TArray<uint8> ReceivedInputs = ReplicatedInputs;
				PhysScene->EnqueueAsyncPhysicsCommand(0, this, [this, PhysScene, ReceivedInputs, LocalOffset]()
				{
					InputsHistory->DeserializeDatas(ReceivedInputs, LocalOffset);

#if DEBUG_NETWORK_PHYSICS
					{
						TArray<int32> LocalFrames, ServerFrames, InputFrames;
						InputsHistory->DebugDatas(ReceivedInputs, LocalFrames, ServerFrames, InputFrames);

						UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | OnRep_SetReplicatedInputs | Receiving %d inputs from SERVER | Local offset = %d | Component = %s"), LocalFrames.Num(), LocalOffset, *GetFullName());
						for (int32 FrameIndex = 0; FrameIndex < LocalFrames.Num(); ++FrameIndex)
						{
							UE_LOG(LogTemp, Log, TEXT("		Recording replicated inputs at local frame = %d | server frame = %d | Component = %s"), ServerFrames[FrameIndex] - LocalOffset, ServerFrames[FrameIndex], *GetFullName());
						}
					}
#endif
				}, false);
			}
		}
	}
}

bool UNetworkPhysicsComponent::ServerReceiveInputsDatas_Validate(const TArray<uint8>& ClientInputs)
{
	return true;
}

void UNetworkPhysicsComponent::ServerReceiveInputsDatas_Implementation(const TArray<uint8>& ClientInputs)
{
	if(InputsHistory)
	{ 
		// We could probably skip that test since the server RPC is on server
		ensure(HasServerWorld());

		// We could probably skip that test since the server RPC is on server
		ensure(!HasLocalController());

		ReplicatedInputs = ClientInputs;

		// Record the received inputs from the client into the history for future use
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				const TArray<uint8> ReceivedInputs = ClientInputs;
				PhysScene->EnqueueAsyncPhysicsCommand(0, this, [this, ReceivedInputs, PhysScene]()
				{
					InputsHistory->DeserializeDatas(ReceivedInputs, 0);
	#if DEBUG_NETWORK_PHYSICS
					{
						TArray<int32> LocalFrames, ServerFrames, InputFrames;
						InputsHistory->DebugDatas(ReceivedInputs, LocalFrames, ServerFrames, InputFrames);

						const int32 CurrentFrame = PhysScene->GetSolver()->GetCurrentFrame();

						const int32 EvalOffset = CurrentFrame - InputFrames[InputFrames.Num()-1] + 4;
						UE_LOG(LogTemp, Log, TEXT("SERVER | PT | ServerReceiveInputsDatas | Receiving %d inputs from CLIENT | Inputs frame = %d | Server frame = %d | Eval Offset = %d | Component = %s | Num Bits = %d"), LocalFrames.Num(), InputFrames[InputFrames.Num() - 1], CurrentFrame, EvalOffset, *GetFullName(), ReceivedInputs.Num() * 8);
						for (int32 FrameIndex = 0; FrameIndex < LocalFrames.Num(); ++FrameIndex)
						{
							UE_LOG(LogTemp, Log, TEXT("		Recording replicated inputs at local frame = %d | server frame = %d | Solver offset = %d | Component = %s"), 
								LocalFrames[FrameIndex], ServerFrames[FrameIndex], ServerFrames[FrameIndex] - LocalFrames[FrameIndex], *GetFullName());
						}
					}
	#endif
				}, false);
			}
		}
	}
}

void UNetworkPhysicsComponent::OnPreProcessInputsInternal(const int32 PhysicsStep)
{
#if DEBUG_NETWORK_PHYSICS
	if (HasServerWorld())
	{
		UE_LOG(LogTemp, Log, TEXT("SERVER | PT | OnPreProcessInputsInternal | At Frame %d | Component = %s"), PhysicsStep, *GetFullName());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | OnPreProcessInputsInternal | At Frame %d | Component = %s"), PhysicsStep, *GetFullName());
	}
#endif

	if(InputsHistory && StatesHistory && ActorComponent)
	{ 
		bool bIsSolverReset = false;
		bool bIsSolverResim = false;

		if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
		{ 
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				bIsSolverResim = Solver->GetEvolution()->IsResimming();
				bIsSolverReset = Solver->GetEvolution()->IsResetting();
			}
		}

		// for the inputs client local ones are ground truth otherwise use the replicated ones coming from the server
		if (!HasLocalController() || bIsSolverResim)
		{
			FNetworkPhysicsDatas* PhysicsDatas = InputsDatas.Get();
			PhysicsDatas->LocalFrame = PhysicsStep;
	#if DEBUG_NETWORK_PHYSICS
			UE_LOG(LogTemp, Log, TEXT("		Extracting history inputs at frame %d | Component = %s"), PhysicsStep, *GetFullName());
	#endif
			if (InputsHistory->ExtractDatas(PhysicsStep, bIsSolverReset, PhysicsDatas))
			{ 
				PhysicsDatas->ApplyDatas(ActorComponent);
			}
		}

		if (!HasServerWorld() && bIsSolverResim)
		{
			FNetworkPhysicsDatas* PhysicsDatas = StatesDatas.Get();
			PhysicsDatas->LocalFrame = PhysicsStep;
	#if DEBUG_NETWORK_PHYSICS
			UE_LOG(LogTemp, Log, TEXT("		Extracting history states at frame %d | Component = %s"), PhysicsStep, *GetFullName());
	#endif
			if (StatesHistory->ExtractDatas(PhysicsStep, bIsSolverReset, PhysicsDatas, true))
			{
				PhysicsDatas->ApplyDatas(ActorComponent);
			}
		}
	}
}

void UNetworkPhysicsComponent::OnPostProcessInputsInternal(const int32 PhysicsStep)
{
	bool bIsSolverReset = false;
	bool bIsSolverResim = false;
#if DEBUG_NETWORK_PHYSICS
	if (HasServerWorld())
	{
		UE_LOG(LogTemp, Log, TEXT("SERVER | PT | OnPostProcessInputsInternal | At Frame %d | Component = %s"), PhysicsStep, *GetFullName());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | OnPostProcessInputsInternal | At Frame %d | Component = %s"), PhysicsStep, *GetFullName());
	}
#endif

	if(InputsHistory && StatesHistory && ActorComponent)
	{
		if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				bIsSolverResim = Solver->GetEvolution()->IsResimming();
				bIsSolverReset = Solver->GetEvolution()->IsResetting();
			}
		}

		// for the inputs client local ones are ground truth otherwise use the replicated ones coming from the server
		if (HasLocalController() && !bIsSolverResim && (InputsDatas != nullptr))
		{
			FNetworkPhysicsDatas* PhysicsDatas = InputsDatas.Get();
			PhysicsDatas->LocalFrame = PhysicsStep;
			PhysicsDatas->ServerFrame = HasServerWorld() ? PhysicsStep : PhysicsStep + GetPlayerController()->GetLocalToServerAsyncPhysicsTickOffset();
			PhysicsDatas->InputFrame = PhysicsStep;

			PhysicsDatas->BuildDatas(ActorComponent);

			InputsOffsets[InputsIndex] = FMath::Max(InputsOffsets[InputsIndex], PhysicsStep + 1);
			InputsHistory->RecordDatas(PhysicsStep, PhysicsDatas);

	#if DEBUG_NETWORK_PHYSICS
			UE_LOG(LogTemp, Log, TEXT("		Recording local inputs at frame %d | Component = %s"), PhysicsDatas->LocalFrame, *GetFullName());
	#endif
		}
		// Compute of the local frame coming from the client that was used to generate this state
		int32 InputFrame = INDEX_NONE;
		{
			FNetworkPhysicsDatas* PhysicsDatas = InputsDatas.Get();
			if(InputsHistory->ExtractDatas(PhysicsStep, false, PhysicsDatas, true))
			{
				InputFrame = PhysicsDatas->InputFrame;
			}
		}

		if (HasServerWorld() && !bIsSolverResim)
		{
			FNetworkPhysicsDatas* PhysicsDatas = StatesDatas.Get();
			PhysicsDatas->LocalFrame = PhysicsStep;
			PhysicsDatas->ServerFrame = PhysicsStep;
			PhysicsDatas->InputFrame = InputFrame;

			PhysicsDatas->BuildDatas(ActorComponent);

			StatesOffsets[StatesIndex] = FMath::Max(StatesOffsets[StatesIndex], PhysicsStep+1);
			StatesHistory->RecordDatas(PhysicsStep, PhysicsDatas);

	#if DEBUG_NETWORK_PHYSICS
			UE_LOG(LogTemp, Log, TEXT("		Recording local states at frame %d | from input frame = %d | Component = %s"), PhysicsDatas->LocalFrame, PhysicsDatas->InputFrame, *GetFullName());
	#endif
		}
	}
}

bool UNetworkPhysicsComponent::HasServerWorld() const
{
	return GetWorld()->IsNetMode(NM_DedicatedServer) || GetWorld()->IsNetMode(NM_ListenServer);
}

bool UNetworkPhysicsComponent::HasLocalController() const
{
	if (APlayerController* PlayerController = GetPlayerController())
	{
		return PlayerController->IsLocalController();
	}
	return false;
}

APlayerController* UNetworkPhysicsComponent::GetPlayerController() const
{
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		return PC;
	}

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Pawn->GetController<APlayerController>();
	}

	return nullptr;
}

void UNetworkPhysicsComponent::RemoveDatasHistory()
{
	if (GetWorld())
	{
		if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				if (Chaos::FRewindData* RewindData = Solver->GetRewindData())
				{
					RewindData->RemoveInputsHistory(InputsHistory);
					RewindData->RemoveStatesHistory(StatesHistory);
				}
			}
		}
	}
}
void UNetworkPhysicsComponent::AddDatasHistory()
{
	if (FPhysScene* PhysScene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
		{
			if (Chaos::FRewindData* RewindData = Solver->GetRewindData())
			{
				RewindData->AddInputsHistory(InputsHistory);
				RewindData->AddStatesHistory(StatesHistory);
			}
		}
	}
}








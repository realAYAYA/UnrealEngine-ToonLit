// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterMotionComponent.h"
#include "CharacterMotionSimulation.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionProxyWrite.h"

#include "GameFramework/CharacterMovementComponent.h" // To facilitate A/B testing by disabling CMC if present
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterMotion, Log, All);

namespace CharacterMotionCVars
{
static float MaxSpeed = 1200.f;
static FAutoConsoleVariableRef CVarMaxSpeed(TEXT("CharacterMotion.MaxSpeed"),
	MaxSpeed,
	TEXT("Temp value for testing changes to max speed."),
	ECVF_Default);

static int32 RequestMispredict = 0;
static FAutoConsoleVariableRef CVarRequestMispredict(TEXT("CharacterMotion.RequestMispredict"),
	RequestMispredict, TEXT("Causes a misprediction by inserting random value into stream on authority side"), ECVF_Default);
}

float UCharacterMotionComponent::GetDefaultMaxSpeed() { return CharacterMotionCVars::MaxSpeed; }

// ----------------------------------------------------------------------------------------------------------
//	FCharacterMotionModelDef: the piece that ties everything together that we use to register with the NP system.
// ----------------------------------------------------------------------------------------------------------

class FCharacterMotionModelDef : public FNetworkPredictionModelDef
{
public:

	NP_MODEL_BODY();

	using Simulation = FCharacterMotionSimulation;
	using StateTypes = CharacterMotionStateTypes;
	using Driver = UCharacterMotionComponent;

	static const TCHAR* GetName() { return TEXT("CharacterMotion"); }
	static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::PreKinematicMovers; }
};

NP_MODEL_REGISTER(FCharacterMotionModelDef);

// ----------------------------------------------------------------------------------------------------------
//	UCharacterMotionComponent
// ----------------------------------------------------------------------------------------------------------

UCharacterMotionComponent::UCharacterMotionComponent()
{

}

void UCharacterMotionComponent::InitializeNetworkPredictionProxy()
{
	OwnedMovementSimulation = MakePimpl<FCharacterMotionSimulation>();
	InitCharacterMotionSimulation(OwnedMovementSimulation.Get());
	
	NetworkPredictionProxy.Init<FCharacterMotionModelDef>(GetWorld(), GetReplicationProxies(), OwnedMovementSimulation.Get(), this);
}

void UCharacterMotionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// TEMP! Disable existing CMC if it is activate. Just makes A/B testing easier for now.
	if (AActor* Owner = GetOwner())
	{
		if (UCharacterMovementComponent* OldComp = Owner->FindComponentByClass<UCharacterMovementComponent>())
		{
			if (OldComp->IsActive())
			{
				OldComp->Deactivate();
			}
		}

		Owner->SetReplicatingMovement(false);
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ENetRole OwnerRole = GetOwnerRole();

	// Check if we should trip a mispredict. (Note how its not possible to do this inside the Update function!)
	if (OwnerRole == ROLE_Authority && CharacterMotionCVars::RequestMispredict)
	{
		FCharacterMotionSimulation::ForceMispredict = true;
		CharacterMotionCVars::RequestMispredict = 0;
	}


	static bool bDrawSimLocation = true;
	static bool bDrawPresentationLocation = true;
	/*
	if (bDrawSimLocation)
	{
		DrawDebugSphere(GetWorld(), 
		NetworkPredictionProxy.ReadSyncState<FCharacterMotionSyncState>(ENetworkPredictionStateRead::Simulation)->Location + FVector(0.f, 0.f, 100.f),
		25.f, 12, FColor::Green, false, -1.f);
	}
	if (bDrawPresentationLocation)
	{
		DrawDebugSphere(GetWorld(), 
			NetworkPredictionProxy.ReadSyncState<FCharacterMotionSyncState>()->Location + FVector(0.f, 0.f, 200.f),
			25.f, 12, FColor::Red, false, -1.f);
	}
	*/
	// Temp
	/*
	if (OwnerRole == ROLE_Authority)
	{
		if (MovementAuxState->Get()->MaxSpeed != CharacterMotionCVars::MaxSpeed)
		{
			MovementAuxState->Modify([](FCharacterMotionAuxState& Aux)
			{
				Aux.MaxSpeed = CharacterMotionCVars::MaxSpeed;
			});
		}
	}
	*/
}

void UCharacterMotionComponent::OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ActiveMovementSimulation)
	{
		ActiveMovementSimulation->OnBeginOverlap(OverlappedComp, Other, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
	}
}

void UCharacterMotionComponent::ProduceInput(const int32 DeltaTimeMS, FCharacterMotionInputCmd* Cmd)
{
	// This isn't ideal. It probably makes sense for the component to do all the input binding rather.
	ProduceInputDelegate.ExecuteIfBound(DeltaTimeMS, *Cmd);
}

void UCharacterMotionComponent::RestoreFrame(const FCharacterMotionSyncState* SyncState, const FCharacterMotionAuxState* AuxState)
{
	FTransform Transform(SyncState->Rotation.Quaternion(), SyncState->Location, UpdatedComponent->GetComponentTransform().GetScale3D() );
	UpdatedComponent->SetWorldTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	UpdatedComponent->ComponentVelocity = SyncState->Velocity;
}

void UCharacterMotionComponent::FinalizeFrame(const FCharacterMotionSyncState* SyncState, const FCharacterMotionAuxState* AuxState)
{
	// The component will often be in the "right place" already on FinalizeFrame, so a comparison check makes sense before setting it.
	if (UpdatedComponent->GetComponentLocation().Equals(SyncState->Location) == false || UpdatedComponent->GetComponentQuat().Rotator().Equals(SyncState->Rotation, FCharacterMotionSimulation::ROTATOR_TOLERANCE) == false)
	{
		RestoreFrame(SyncState, AuxState);
	}
}

void UCharacterMotionComponent::InitializeSimulationState(FCharacterMotionSyncState* Sync, FCharacterMotionAuxState* Aux)
{
	npCheckSlow(UpdatedComponent);
	npCheckSlow(Sync);
	npCheckSlow(Aux);

	Sync->Location = UpdatedComponent->GetComponentLocation();
	Sync->Rotation = UpdatedComponent->GetComponentQuat().Rotator();

	Aux->MaxSpeed = GetDefaultMaxSpeed();
}

// Init function. This is broken up from ::InstantiateNetworkedSimulation and templated so that subclasses can share the init code
void UCharacterMotionComponent::InitCharacterMotionSimulation(FCharacterMotionSimulation* Simulation)
{
	check(UpdatedComponent);
	check(ActiveMovementSimulation == nullptr); // Reinstantiation not supported
	ActiveMovementSimulation = Simulation;

	Simulation->SetComponents(UpdatedComponent, UpdatedPrimitive);
}

float UCharacterMotionComponent::GetMaxMoveSpeed() const
{
	if (const FCharacterMotionAuxState* AuxState = NetworkPredictionProxy.ReadAuxState<FCharacterMotionAuxState>())
	{
		return AuxState->MaxSpeed;
	}
	return 0;
}

void UCharacterMotionComponent::SetMaxMoveSpeed(float NewMaxMoveSpeed)
{
	NetworkPredictionProxy.WriteAuxState<FCharacterMotionAuxState>([NewMaxMoveSpeed](FCharacterMotionAuxState& AuxState)
	{
		AuxState.MaxSpeed = NewMaxMoveSpeed;
	}, "SetMaxMoveSpeed");
}

void UCharacterMotionComponent::AddMaxMoveSpeed(float AdditiveMaxMoveSpeed)
{
	NetworkPredictionProxy.WriteAuxState<FCharacterMotionAuxState>([AdditiveMaxMoveSpeed](FCharacterMotionAuxState& AuxState)
	{
		AuxState.MaxSpeed += AdditiveMaxMoveSpeed;
	}, "AddMaxMoveSpeed");
}



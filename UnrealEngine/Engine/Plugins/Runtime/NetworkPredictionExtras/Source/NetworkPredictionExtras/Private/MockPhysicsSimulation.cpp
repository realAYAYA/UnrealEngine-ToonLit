// Copyright Epic Games, Inc. All Rights Reserved.

#include "MockPhysicsSimulation.h"
#include "Physics/Experimental/PhysInterface_Chaos.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/Framework/PhysicsSolverBase.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"
#include "PBDRigidsSolver.h"
#include "Chaos/ChaosScene.h"
#include "UObject/Object.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Physics/GenericPhysicsInterface.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "PhysicsEngine/BodyInstance.h"
#include "DrawDebugHelpers.h"
#include "Logging/LogMacros.h"
#include "NetworkPredictionCues.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

NETSIMCUE_REGISTER(FMockPhysicsJumpCue, TEXT("MockPhysicsJumpCue"));
NETSIMCUE_REGISTER(FMockPhysicsChargeCue, TEXT("FMockPhysicsChargeCue"));

DEFINE_LOG_CATEGORY_STATIC(LogMockPhysicsSimulation, Log, All);

void FMockPhysicsSimulation::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<MockPhysicsStateTypes>& Input, const TNetSimOutput<MockPhysicsStateTypes>& Output)
{
	npCheckSlow(this->PrimitiveComponent);

	const bool bAllowSubstepping = false;
	const bool bAccelChange = true;
	const FTransform CurrentTransform = PrimitiveComponent->GetComponentTransform();	
	const FVector CurrentLocation = CurrentTransform.GetLocation();

	if (Input.Cmd->MovementInput.IsNearlyZero() == false)
	{
		FVector Force = Input.Cmd->MovementInput;
		Force *= (1000.f * Input.Aux->ForceMultiplier);
		Force.Z = 0.f;

		PrimitiveComponent->AddForce(Force, NAME_None, bAccelChange);
	}

	// Apply jump force if bJumpedPressed and not on cooldown
	if (Input.Cmd->bJumpedPressed && Input.Aux->JumpCooldownTime < TimeStep.TotalSimulationTime)
	{
		FVector JumpForce(0.f, 0.f, 50000.f);
		PrimitiveComponent->AddForce(JumpForce, NAME_None, bAccelChange);

		Output.Aux.Get()->JumpCooldownTime = TimeStep.TotalSimulationTime + 2000; // 2 second cooldown
		
		// Jump cue: this will emit an event to the game code to play a particle or whatever they want
		Output.CueDispatch.Invoke<FMockPhysicsJumpCue>(CurrentLocation);
	}


	if (Input.Aux->ChargeEndTime != 0)
	{
		const int32 TimeSinceChargeEnd = TimeStep.TotalSimulationTime - Input.Aux->ChargeEndTime;
		if (TimeSinceChargeEnd > 100)
		{
			Output.Aux.Get()->ChargeEndTime = 0;

			UWorld* World = PrimitiveComponent->GetWorld();
			npCheckSlow(World);

			// Obviously not good to hardcode the collision settings. We could just use the default settings on the UPrimitiveComponent, or these could be 
			// custom "how do you scene query for the charge ability" settings on this simulation object.

			FVector TracePosition = CurrentLocation;
			FCollisionShape Shape = FCollisionShape::MakeSphere(250.f);
			ECollisionChannel CollisionChannel = ECollisionChannel::ECC_PhysicsBody; 
			FCollisionQueryParams QueryParams = FCollisionQueryParams::DefaultQueryParam;
			FCollisionResponseParams ResponseParams = FCollisionResponseParams::DefaultResponseParam;
			FCollisionObjectQueryParams ObjectParams(ECollisionChannel::ECC_PhysicsBody);

			TArray<FOverlapResult> Overlaps;

			World->OverlapMultiByChannel(Overlaps, TracePosition, FQuat::Identity, CollisionChannel, Shape);

			for (FOverlapResult& Result : Overlaps)
			{
				//UE_LOG(LogTemp, Warning, TEXT("  Hit: %s"), *GetNameSafe(Result.Actor.Get()));
				if (UPrimitiveComponent* PrimitiveComp = Result.Component.Get())
				{
					// We are just applying an impulse to all simulating primitive components. 
					// A real game would have game specific logic determining which actors/components can be affected
					if (PrimitiveComp->IsSimulatingPhysics())
					{
						// Debug: confirm the hit component and its underlying physics body are in sync
						if (FBodyInstance* Instance = PrimitiveComp->GetBodyInstance())
						{
							FPhysicsActorHandle HitHandle = Instance->GetPhysicsActorHandle();
							const FVector PhysicsLocation = HitHandle->GetGameThreadAPI().X();

							const FVector Delta = PhysicsLocation - PrimitiveComp->GetComponentLocation();
							if (Delta.Size2D() > 0.1f)
							{
								UE_LOG(LogMockPhysicsSimulation, Warning, TEXT("FMockPhysicsSimulation Not in sync! %s %s (%s)"), *PhysicsLocation.ToString(), *PrimitiveComp->GetComponentLocation().ToString(), *Delta.ToString());
								DrawDebugSphere(PrimitiveComp->GetWorld(), PhysicsLocation, 100.f, 32, FColor::Red, true, 10.f);
								DrawDebugSphere(PrimitiveComp->GetWorld(), PrimitiveComp->GetComponentLocation(), 100.f, 32, FColor::Green, true, 10.f);
								npEnsure(false);
							}
						}

						FVector Dir = (PrimitiveComp->GetComponentLocation() - TracePosition);
						Dir.Z = 0.f;
						Dir.Normalize();

						FVector Impulse = Dir * 100000.f;
						Impulse.Z = 100000.f;

						PrimitiveComp->AddImpulseAtLocation(Impulse, TracePosition);
					}
				}
			}

			Output.CueDispatch.Invoke<FMockPhysicsChargeCue>( TracePosition );
		}
	}
	else
	{
		const bool bWasCharging = (Input.Aux->ChargeStartTime != 0);

		if (!bWasCharging && Input.Cmd->bChargePressed)
		{
			// Press
			Output.Aux.Get()->ChargeStartTime = TimeStep.TotalSimulationTime;
			Output.Aux.Get()->ChargeEndTime = 0;
		}

		if (bWasCharging && !Input.Cmd->bChargePressed)
		{
			// Release
			Output.Aux.Get()->ChargeStartTime = 0;
			Output.Aux.Get()->ChargeEndTime = TimeStep.TotalSimulationTime;
		}
	}
}

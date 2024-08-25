// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleSimulationCU.h"
#include "ChaosModularVehicle/ModularVehicleDefaultAsyncInput.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Chaos/ClusterUnionManager.h"
#include "Chaos/DebugDrawQueue.h"
#include "Engine/World.h"
#include "ChaosModularVehicle/ModularVehicleDebug.h"

FModularVehicleDebugParams GModularVehicleDebugParams;

#if CHAOS_DEBUG_DRAW
FAutoConsoleVariableRef CVarChaosModularVehiclesRaycastsEnabled(TEXT("p.ModularVehicle.SuspensionRaycastsEnabled"), GModularVehicleDebugParams.SuspensionRaycastsEnabled, TEXT("Enable/Disable Suspension Raycasts."));
FAutoConsoleVariableRef CVarChaosModularVehiclesShowRaycasts(TEXT("p.ModularVehicle.ShowSuspensionRaycasts"), GModularVehicleDebugParams.ShowSuspensionRaycasts, TEXT("Enable/Disable Suspension Raycast Visualisation."));
FAutoConsoleVariableRef CVarChaosModularVehiclesShowWheelData(TEXT("p.ModularVehicle.ShowWheelData"), GModularVehicleDebugParams.ShowWheelData, TEXT("Enable/Disable Displaying Wheel Simulation Data."));
FAutoConsoleVariableRef CVarChaosModularVehiclesShowRaycastMaterial(TEXT("p.ModularVehicle.ShowRaycastMaterial"), GModularVehicleDebugParams.ShowRaycastMaterial, TEXT("Enable/Disable Raycast Material Hit Visualisation."));
FAutoConsoleVariableRef CVarChaosModularVehiclesShowWheelCollisionNormal(TEXT("p.ModularVehicle.ShowWheelCollisionNormal"), GModularVehicleDebugParams.ShowWheelCollisionNormal, TEXT("Enable/Disable Wheel Collision Normal Visualisation."));
FAutoConsoleVariableRef CVarChaosModularVehiclesFrictionOverride(TEXT("p.ModularVehicle.FrictionOverride"), GModularVehicleDebugParams.FrictionOverride, TEXT("Override the physics material friction value.."));
FAutoConsoleVariableRef CVarChaosModularVehiclesDisableAnim(TEXT("p.ModularVehicle.DisableAnim"), GModularVehicleDebugParams.DisableAnim, TEXT("Disable animating wheels, etc"));
#endif


void FModularVehicleSimulationCU::Initialize(TUniquePtr<Chaos::FSimModuleTree>& InSimModuleTree)
{
	SimModuleTree = MoveTemp(InSimModuleTree);
	InputInterpolation.Init(FModularVehicleInputRate(), EModularVehicleInputType::Max);
}

void FModularVehicleSimulationCU::Terminate()
{
	SimModuleTree.Reset(nullptr);
}

void FModularVehicleSimulationCU::Simulate(UWorld* InWorld, float DeltaSeconds, const FModularVehicleAsyncInput& InputData, FModularVehicleAsyncOutput& OutputData, IPhysicsProxyBase* Proxy)
{
#if DEBUG_NETWORK_PHYSICS

	if (InWorld->IsNetMode(NM_ListenServer) || InWorld->IsNetMode(NM_DedicatedServer))
	{
		UE_LOG(LogTemp, Log, TEXT("SERVER | PT | TickVehicle | Async tick vehicle with inputs at frame %d : Throttle = %f Brake = %f Roll = %f Pitch = %f Yaw = %f Steering = %f Handbrake = %f"),
			InputData.PhysicsInputs.NetworkInputs.LocalFrame, VehicleInputs.Throttle, VehicleInputs.Brake, VehicleInputs.Roll, VehicleInputs.Pitch,
			VehicleInputs.Yaw, VehicleInputs.Steering, VehicleInputs.Handbrake);

		UE_LOG(LogTemp, Log, TEXT("ALT-SERVER | PT | TickVehicle | Async tick vehicle with inputs at frame %d : Throttle = %f Brake = %f Roll = %f Pitch = %f Yaw = %f Steering = %f Handbrake = %f"),
			InputData.PhysicsInputs.NetworkInputs.LocalFrame, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Throttle, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Brake, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Roll, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Pitch,
			InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Yaw, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Steering, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Handbrake);
	}
	else if (InWorld->IsNetMode(NM_Client))
	{
		UE_LOG(LogTemp, Log, TEXT("CLIENT | PT | TickVehicle | Async tick vehicle with inputs at frame %d : Throttle = %f Brake = %f Roll = %f Pitch = %f Yaw = %f Steering = %f Handbrake = %f"),
			InputData.PhysicsInputs.NetworkInputs.LocalFrame, VehicleInputs.Throttle, VehicleInputs.Brake, VehicleInputs.Roll, VehicleInputs.Pitch,
			VehicleInputs.Yaw, VehicleInputs.Steering, VehicleInputs.Handbrake);

		UE_LOG(LogTemp, Log, TEXT("ALT-CLIENT | PT | TickVehicle | Async tick vehicle with inputs at frame %d : Throttle = %f Brake = %f Roll = %f Pitch = %f Yaw = %f Steering = %f Handbrake = %f"),
			InputData.PhysicsInputs.NetworkInputs.LocalFrame, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Throttle, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Brake, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Roll, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Pitch,
			InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Yaw, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Steering, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Handbrake);
	}

#endif


	Chaos::FPBDRigidsSolver* RigidsSolver = static_cast<Chaos::FPBDRigidsSolver*>(Proxy->GetSolver<Chaos::FPhysicsSolver>());
	int CurrentFrame = -1;
	if (RigidsSolver != nullptr)
	{
		Chaos::FRewindData* RewindData = RigidsSolver->GetRewindData();
		if (RewindData != nullptr)
		{
			CurrentFrame = RewindData->CurrentFrame();
		}
	}

	//if (InWorld)
	//{
	//	WriteNetReport(InWorld->IsNetMode(NM_Client), FString::Printf(TEXT("Frame %d,  Throttle %3.2f,  Brake %3.2f, Current Frame %d")
	//		, InputData.PhysicsInputs.NetworkInputs.LocalFrame
	//		, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Throttle
	//		, InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Brake
	//		, CurrentFrame));
	//}

	check(Proxy->GetType() == EPhysicsProxyType::ClusterUnionProxy)
	Simulate_ClusterUnion(InWorld, DeltaSeconds, InputData, OutputData, static_cast<Chaos::FClusterUnionPhysicsProxy*>(Proxy));
}

void FModularVehicleSimulationCU::Simulate_ClusterUnion(UWorld* InWorld, float DeltaSeconds, const FModularVehicleAsyncInput& InputData, FModularVehicleAsyncOutput& OutputData, Chaos::FClusterUnionPhysicsProxy* Proxy)
{
	Chaos::EnsureIsInPhysicsThreadContext();

	if (Proxy && SimModuleTree.IsValid())
	{
		int InitialNum = SimModuleTree->GetSimulationModuleTree().Num();

		//if (InWorld)
		//{
		//	WriteNetReport(InWorld->IsNetMode(NM_Client), FString::Printf(TEXT("X %s,  R %s,  V %s,  W %s")
		//		, *Proxy->GetParticle_Internal()->X().ToString()
		//		, *Proxy->GetParticle_Internal()->R().ToString()
		//		, *Proxy->GetParticle_Internal()->V().ToString()
		//		, *Proxy->GetParticle_Internal()->W().ToString()));
		//}

		//InterpolateInputs(DeltaSeconds, ExternalInputs, InterpolatedInputs);

		SimInputData.ControlInputs.Throttle = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Throttle;
		SimInputData.ControlInputs.Steering = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Steering;
		SimInputData.ControlInputs.Brake = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Brake;
		SimInputData.ControlInputs.Handbrake = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Handbrake;
		SimInputData.ControlInputs.Roll = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Roll;
		SimInputData.ControlInputs.Pitch = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Pitch;
		SimInputData.ControlInputs.Yaw = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Yaw;
		SimInputData.ControlInputs.Boost = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Boost;
		SimInputData.ControlInputs.Drift = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Drift;
		SimInputData.ControlInputs.IsReversing = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Reverse;
		SimInputData.bKeepVehicleAwake = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.KeepAwake;

		PerformAdditionalSimWork(InWorld, InputData, Proxy, SimInputData);

		// run the dynamics simulation, engine, suspension, wheels, aerofoils etc.
		SimModuleTree->Simulate(DeltaSeconds, SimInputData, Proxy);
	}

}

//void FModularVehicleSimulationCU::InterpolateInputs(float DeltaSeconds, const Chaos::FControlInputs& ExternalInputIn, Chaos::FControlInputs& InterpolatedInputsInOut)
//{
//	InterpolatedInputsInOut.Steering = InputInterpolation[EModularVehicleInputType::Steering].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Steering, ExternalInputIn.Steering);
//	InterpolatedInputsInOut.Throttle = InputInterpolation[EModularVehicleInputType::Throttle].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Throttle, ExternalInputIn.Throttle);
//	InterpolatedInputsInOut.Brake = InputInterpolation[EModularVehicleInputType::Brake].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Brake, ExternalInputIn.Brake);
//	InterpolatedInputsInOut.Handbrake = InputInterpolation[EModularVehicleInputType::Handbrake].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Handbrake, ExternalInputIn.Handbrake);
//	InterpolatedInputsInOut.Pitch = InputInterpolation[EModularVehicleInputType::Pitch].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Pitch, ExternalInputIn.Pitch);
//	InterpolatedInputsInOut.Roll = InputInterpolation[EModularVehicleInputType::Roll].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Roll, ExternalInputIn.Roll);
//	InterpolatedInputsInOut.Yaw = InputInterpolation[EModularVehicleInputType::Yaw].InterpInputValue(DeltaSeconds, InterpolatedInputsInOut.Yaw, ExternalInputIn.Yaw);
//	InterpolatedInputsInOut.GearNumber = ExternalInputIn.GearNumber;
//	InterpolatedInputsInOut.InputDebugIndex = ExternalInputIn.InputDebugIndex;
//}


void FModularVehicleSimulationCU::PerformAdditionalSimWork(UWorld* InWorld, const FModularVehicleAsyncInput& InputData, Chaos::FClusterUnionPhysicsProxy* Proxy, Chaos::FAllInputs& AllInputs)
{
	using namespace Chaos;
	check(Proxy);
	Chaos::EnsureIsInPhysicsThreadContext();

	if (SimModuleTree)
	{
		FPBDRigidsEvolutionGBF& Evolution = *static_cast<FPBDRigidsSolver*>(Proxy->GetSolver<FPBDRigidsSolver>())->GetEvolution();
		FClusterUnionManager& ClusterUnionManager = Evolution.GetRigidClustering().GetClusterUnionManager();
		const FClusterUnionIndex& CUI = Proxy->GetClusterUnionIndex();
		if (FClusterUnion* ClusterUnion = ClusterUnionManager.FindClusterUnion(CUI))
		{
			if (FPBDRigidClusteredParticleHandle* ClusterHandle = ClusterUnion->InternalCluster)
			{
				TArray<FPBDRigidParticleHandle*> Particles = ClusterUnion->ChildParticles;
				FRigidTransform3 ClusterWorldTM = FRigidTransform3(ClusterHandle->GetX(), ClusterHandle->GetR());

				//Chaos::FDebugDrawQueue::GetInstance().DrawDebugCoordinateSystem(ClusterHandle->GetX(), FRotator(ClusterHandle->R()), 200, false, -1.f, 1.f, 3.0f);

				const TArray<Chaos::FSimModuleTree::FSimModuleNode>& ModuleArray = SimModuleTree->GetSimulationModuleTree();

				for (const Chaos::FSimModuleTree::FSimModuleNode& Node : ModuleArray)
				{
					if (Node.IsValid() && Node.SimModule && Node.SimModule->IsEnabled())
					{
						FRigidTransform3 Frame = FRigidTransform3::Identity;

						FPBDRigidParticleHandle* Child = Node.SimModule->GetParticleFromUniqueIndex(Node.SimModule->GetParticleIndex().Idx, Particles);
						if (Child == nullptr)
						{
							continue;
						}

						if (FPBDRigidClusteredParticleHandle* ClusterChild = Child->CastToClustered(); ClusterChild && ClusterChild->IsChildToParentLocked())
						{
							Frame = ClusterChild->ChildToParent();
						}
						else
						{
							Frame = ClusterChild->ChildToParent();

							const FRigidTransform3 ChildWorldTM(Child->GetX(), Child->GetR());
							Frame = ChildWorldTM.GetRelativeTransform(ClusterWorldTM);
						}

						if (ClusterHandle)
						{
							AllInputs.VehicleWorldTransform = ClusterWorldTM;

							if (Node.SimModule->IsClustered() && Node.SimModule->IsBehaviourType(Chaos::eSimModuleTypeFlags::Raycast))
							{
								Chaos::FSpringTrace OutTrace;
								Chaos::FSuspensionSimModule* Suspension = static_cast<Chaos::FSuspensionSimModule*>(Node.SimModule);

								// would be cleaner an faster to just store radius in suspension also
								float WheelRadius = 0;
								if (Suspension->GetWheelSimTreeIndex() != Chaos::ISimulationModuleBase::INVALID_IDX)
								{
									Chaos::FWheelSimModule* Wheel = static_cast<Chaos::FWheelSimModule*>(ModuleArray[Suspension->GetWheelSimTreeIndex()].SimModule);
									if (Wheel)
									{
										WheelRadius = Wheel->Setup().Radius;
									}
								}

								Suspension->GetWorldRaycastLocation(ClusterWorldTM, WheelRadius, OutTrace);

								FVector TraceStart = OutTrace.Start;
								FVector TraceEnd = OutTrace.End;

								const FCollisionQueryParams& TraceParams = InputData.PhysicsInputs.TraceParams;
								FVector TraceVector(TraceStart - TraceEnd);
								FVector TraceNormal = TraceVector.GetSafeNormal();

								FHitResult HitResult = FHitResult();
								ECollisionChannel SpringCollisionChannel = ECollisionChannel::ECC_WorldDynamic;
								const FCollisionResponseParams& ResponseParams = InputData.PhysicsInputs.TraceCollisionResponse;
								if (InWorld)
								{
									InWorld->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, SpringCollisionChannel, TraceParams, ResponseParams);
								}

								float Offset = Suspension->Setup().MaxLength;
								if (HitResult.bBlockingHit && GModularVehicleDebugParams.SuspensionRaycastsEnabled)
								{
									Offset = HitResult.Distance - WheelRadius;

									if (Suspension->GetWheelSimTreeIndex() != Chaos::ISimulationModuleBase::INVALID_IDX)
									{
										const Chaos::FSimModuleTree::FSimModuleNode& WheelNode = ModuleArray[Suspension->GetWheelSimTreeIndex()];

										Chaos::FWheelSimModule* Wheel = static_cast<Chaos::FWheelSimModule*>(WheelNode.SimModule);
										if (Wheel && HitResult.PhysMaterial.IsValid())
										{
											if (GModularVehicleDebugParams.FrictionOverride > 0)
											{
												Wheel->SetSurfaceFriction(GModularVehicleDebugParams.FrictionOverride);
											}
											else
											{
												Wheel->SetSurfaceFriction(HitResult.PhysMaterial->Friction);
											}
										}
									}

#if CHAOS_DEBUG_DRAW
									if (GModularVehicleDebugParams.ShowSuspensionRaycasts)
									{
										Chaos::FDebugDrawQueue::GetInstance().DrawDebugSphere(HitResult.ImpactPoint, 3, 16, FColor::Red, false, -1.f, 0, 10.f);
									}

									if (Suspension->GetWheelSimTreeIndex() != Chaos::ISimulationModuleBase::INVALID_IDX)
									{
										Chaos::FWheelSimModule* Wheel = static_cast<Chaos::FWheelSimModule*>(ModuleArray[Suspension->GetWheelSimTreeIndex()].SimModule);
										if (Wheel)
										{
											if (GModularVehicleDebugParams.ShowWheelData)
											{
												FString TextOut = FString::Format(TEXT("{0}"), { Wheel->GetForceIntoSurface() });
												FColor Col = FColor::White;
												if (InWorld)
												{
													if (InWorld->GetNetMode() == ENetMode::NM_Client)
													{
														Col = FColor::Blue;
													}
													else
													{
														Col = FColor::Red;
													}
												}
												Chaos::FDebugDrawQueue::GetInstance().DrawDebugString(HitResult.ImpactPoint + FVec3(0, 50, 50), TextOut, nullptr, Col, -1.f, true, 1.0f);
											}
										}
									}

#endif
								}

#if CHAOS_DEBUG_DRAW
								if (GModularVehicleDebugParams.ShowSuspensionRaycasts)
								{
									FColor DrawColor = FColor::Green;
									DrawColor = (HitResult.bBlockingHit) ? FColor::Red : FColor::Green;
									Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(TraceStart, TraceEnd, DrawColor, false, -1.f, 0, 2.f);
									Chaos::FDebugDrawQueue::GetInstance().DrawDebugSphere(TraceStart, 3, 16, FColor::White, false, -1.f, 0, 10.f);
									Chaos::FDebugDrawQueue::GetInstance().DrawDebugSphere(HitResult.ImpactPoint, 1, 16, FColor::Red, false, -1.f, 0, 10.f);
									FString TextOut = FString::Format(TEXT("{0}"), { HitResult.Time });

									FColor Col = FColor::White;
									if (InWorld)
									{
										if (InWorld->GetNetMode() == ENetMode::NM_Client)
										{
											Col = FColor::Blue;
										}
										else
										{
											Col = FColor::Red;
										}
									}
									Chaos::FDebugDrawQueue::GetInstance().DrawDebugString(HitResult.ImpactPoint + FVec3(0, 50, 50), TextOut, nullptr, Col, -1.f, true, 1.0f);
								}

								if (GModularVehicleDebugParams.ShowRaycastMaterial)
								{
									if (HitResult.PhysMaterial.IsValid())
									{
										FDebugDrawQueue::GetInstance().DrawDebugString(HitResult.ImpactPoint, HitResult.PhysMaterial->GetName(), nullptr, FColor::White, -1.f, true, 1.0f);
									}
								}

								if (GModularVehicleDebugParams.ShowWheelCollisionNormal)
								{
									FVector Pt = HitResult.ImpactPoint;
									FDebugDrawQueue::GetInstance().DrawDebugLine(Pt, Pt + HitResult.Normal * 20.0f, FColor::Yellow, false, 1.0f, 0, 1.0f);
									FDebugDrawQueue::GetInstance().DrawDebugSphere(Pt, 5.0f, 4, FColor::White, false, 1.0f, 0, 1.0f);
								}

#endif
								Suspension->SetSpringLength(Offset, WheelRadius);
								FVector Up = ClusterWorldTM.GetUnitAxis(EAxis::Z);
								Suspension->SetTargetPoint(HitResult.ImpactPoint + Up * WheelRadius, HitResult.ImpactNormal, HitResult.bBlockingHit);
							}

						}
					}

				}
			}
		}

	}
}

/**
 * ApplyDeferredForces should be called after the ParallelUpdatePT to send the calculated forces to the physics thread serially
 * as this cannot be done in parallel
 */
void FModularVehicleSimulationCU::ApplyDeferredForces(FGeometryCollectionPhysicsProxy* Proxy)
{
	Chaos::EnsureIsInPhysicsThreadContext();

	if (SimModuleTree && Proxy)
	{
		if (UGeometryCollectionComponent* GCComponent = Cast<UGeometryCollectionComponent>(Proxy->GetOwner()))
		{
			check(GCComponent->GetOwner());

			if (const UGeometryCollection* Rest = GCComponent->GetRestCollection())
			{
				if (Rest->GetGeometryCollection() && Rest->GetGeometryCollection()->HasAttribute(TEXT("MassToLocal"), FTransformCollection::TransformGroup))
				{
					const TManagedArray<FTransform>& CollectionMassToLocal = Rest->GetGeometryCollection()->GetAttribute<FTransform>(TEXT("MassToLocal"), FTransformCollection::TransformGroup);

					SimModuleTree->AccessDeferredForces().Apply(
						Proxy,
						Rest->GetGeometryCollection()->Transform,
						CollectionMassToLocal,
						Rest->GetGeometryCollection()->Parent);
				}
			}
		}
	}
}

void FModularVehicleSimulationCU::ApplyDeferredForces(Chaos::FClusterUnionPhysicsProxy* Proxy)
{
	using namespace Chaos;

	Chaos::EnsureIsInPhysicsThreadContext();

	if (SimModuleTree && Proxy)
	{
		// This gives us access to the PT parent cluster and child particles
		FPBDRigidsEvolutionGBF& Evolution = *static_cast<FPBDRigidsSolver*>(Proxy->GetSolver<FPBDRigidsSolver>())->GetEvolution();
		FClusterUnionManager& ClusterUnionManager = Evolution.GetRigidClustering().GetClusterUnionManager();
		const FClusterUnionIndex& CUI = Proxy->GetClusterUnionIndex();
		if (FClusterUnion* ClusterUnion = ClusterUnionManager.FindClusterUnion(CUI))
		{
			FPBDRigidClusteredParticleHandle* ClusterHandle = ClusterUnion->InternalCluster;
			TArray<FPBDRigidParticleHandle*> Particles = ClusterUnion->ChildParticles;

			TArray<FPBDRigidClusteredParticleHandle*> Clusters;
			Clusters.Add(ClusterHandle);

			SimModuleTree->AccessDeferredForces().Apply(
				Particles,
				Clusters);
		}
	}
}

void FModularVehicleSimulationCU::FillOutputState(FModularVehicleAsyncOutput& Output)
{
	if (Chaos::FSimModuleTree* SimTree = GetSimComponentTree().Get())
	{
		for (int I = 0; I < SimTree->GetNumNodes(); I++)
		{
			if (SimTree->GetSimModule(I))
			{
				if (Chaos::FSimOutputData* OutData = SimTree->AccessSimModule(I)->GenerateOutputData())
				{
					OutData->FillOutputState(SimTree->GetSimModule(I));
					Output.VehicleSimOutput.SimTreeOutputData.Add(OutData);
				}
			}
		}
	}
}

Chaos::FControlInputs& FModularVehicleSimulationCU::AccessControlInputs()
{
	Chaos::EnsureIsInPhysicsThreadContext();
	return SimModuleTree->GetControlInputs();
}

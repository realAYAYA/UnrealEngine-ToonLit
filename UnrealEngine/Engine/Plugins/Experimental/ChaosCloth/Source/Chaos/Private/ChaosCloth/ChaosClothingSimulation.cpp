// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosCloth/ChaosClothingSimulation.h"
#include "ChaosCloth/ChaosClothPrivate.h"

#include "ClothingSimulation.h"
#include "ClothingAsset.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ChaosCloth/ChaosWeightMapTarget.h"

#include "ChaosCloth/ChaosClothingSimulationSolver.h"
#include "ChaosCloth/ChaosClothingSimulationMesh.h"
#include "ChaosCloth/ChaosClothingSimulationCloth.h"
#include "ChaosCloth/ChaosClothingSimulationCollider.h"

#include "PhysicsField/PhysicsFieldComponent.h"
#include "PhysicsProxy/PerSolverFieldSystem.h"
#include "DynamicMeshBuilder.h"

#include "HAL/IConsoleManager.h"

#if WITH_EDITOR || CHAOS_DEBUG_DRAW
#include "Chaos/Sphere.h"
#include "Chaos/TaperedCylinder.h"
#include "Chaos/Capsule.h"
#include "Chaos/TaperedCapsule.h"
#include "Chaos/Convex.h"
#include "Chaos/Triangle.h"
#include "Chaos/PBDSphericalConstraint.h"
#include "Chaos/PBDAnimDriveConstraint.h"
#include "Chaos/PBDLongRangeConstraints.h"
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/PBDCollisionSpringConstraints.h"
#include "Chaos/PBDTriangleMeshCollisions.h"
#include "Chaos/PBDBendingConstraints.h"
#include "Chaos/XPBDSpringConstraints.h"
#include "Chaos/XPBDBendingConstraints.h"
#include "Chaos/VelocityField.h"
#endif  // #if WITH_EDITOR || CHAOS_DEBUG_DRAW

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Engine/Canvas.h"  // For debug draw text
#include "CanvasItem.h"     //
#include "Engine/Engine.h"  //
#endif  // #if WITH_EDITOR

#if CHAOS_DEBUG_DRAW
#include "Chaos/DebugDrawQueue.h"
#endif  // #if CHAOS_DEBUG_DRAW

#if !UE_BUILD_SHIPPING
#include "FramePro/FramePro.h"
#else
#define FRAMEPRO_ENABLED 0
#endif

#if INTEL_ISPC
#include "ChaosClothingSimulation.ispc.generated.h"
#endif

#if INTEL_ISPC && !UE_BUILD_SHIPPING  // Include only used for the Ispc command
#include "Chaos/PBDBendingConstraints.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/PBDEvolution.h"
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/PBDAxialSpringConstraints.h"
#include "Chaos/PerParticleDampVelocity.h"
#include "Chaos/PerParticlePBDCollisionConstraint.h"
#include "Chaos/PBDSphericalConstraint.h"
#include "Chaos/PBDLongRangeConstraints.h"
#include "Chaos/VelocityField.h"
#include "Chaos/XPBDSpringConstraints.h"
#include "Chaos/XPBDBendingConstraints.h"
#endif

#if INTEL_ISPC && !UE_BUILD_SHIPPING
static_assert(sizeof(ispc::FVector3f) == sizeof(FVector3f), "sizeof(ispc::FVector3f) != sizeof(FVector3f)");
static_assert(sizeof(ispc::FVector) == sizeof(Chaos::FVec3), "sizeof(ispc::FVector) != sizeof(Chaos::FVec3)");
static_assert(sizeof(ispc::FTransform) == sizeof(Chaos::FRigidTransform3), "sizeof(ispc::FTransform) != sizeof(Chaos::FRigidTransform3)");

bool bChaos_GetSimData_ISPC_Enabled = true;
FAutoConsoleVariableRef CVarChaosGetSimDataISPCEnabled(TEXT("p.Chaos.GetSimData.ISPC"), bChaos_GetSimData_ISPC_Enabled, TEXT("Whether to use ISPC optimizations when getting simulation data"));
#endif


DECLARE_CYCLE_STAT(TEXT("Chaos Cloth Simulate"), STAT_ChaosClothSimulate, STATGROUP_ChaosCloth);
DECLARE_CYCLE_STAT(TEXT("Chaos Cloth Create Actor"), STAT_ChaosClothCreateActor, STATGROUP_ChaosCloth);
DECLARE_CYCLE_STAT(TEXT("Chaos Cloth Get Simulation Data"), STAT_ChaosClothGetSimulationData, STATGROUP_ChaosCloth);

namespace Chaos
{
bool bClothUseTimeStepSmoothing = true;
FAutoConsoleVariableRef CVarClothUseTimeStepSmoothing(TEXT("p.ChaosCloth.UseTimeStepSmoothing"), bClothUseTimeStepSmoothing, TEXT("Use time step smoothing to avoid jitter during drastic changes in time steps."));

#if CHAOS_DEBUG_DRAW
namespace ClothingSimulationCVar
{
	TAutoConsoleVariable<bool> DebugDrawLocalSpace          (TEXT("p.ChaosCloth.DebugDrawLocalSpace"          ), false, TEXT("Whether to debug draw the Chaos Cloth local space"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawBounds              (TEXT("p.ChaosCloth.DebugDrawBounds"              ), false, TEXT("Whether to debug draw the Chaos Cloth bounds"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawGravity             (TEXT("p.ChaosCloth.DebugDrawGravity"             ), false, TEXT("Whether to debug draw the Chaos Cloth gravity acceleration vector"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawPhysMeshWired       (TEXT("p.ChaosCloth.DebugDrawPhysMeshWired"       ), false, TEXT("Whether to debug draw the Chaos Cloth wireframe meshes"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawAnimMeshWired       (TEXT("p.ChaosCloth.DebugDrawAnimMeshWired"       ), false, TEXT("Whether to debug draw the animated/kinematic Cloth wireframe meshes"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawAnimNormals         (TEXT("p.ChaosCloth.DebugDrawAmimNormals"         ), false, TEXT("Whether to debug draw the animated/kinematic Cloth normals"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawPointNormals        (TEXT("p.ChaosCloth.DebugDrawPointNormals"        ), false, TEXT("Whether to debug draw the Chaos Cloth point normals"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawPointVelocities     (TEXT("p.ChaosCloth.DebugDrawPointVelocities"     ), false, TEXT("Whether to debug draw the Chaos Cloth point velocities"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawFaceNormals         (TEXT("p.ChaosCloth.DebugDrawFaceNormals"         ), false, TEXT("Whether to debug draw the Chaos Cloth face normals"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawInversedFaceNormals (TEXT("p.ChaosCloth.DebugDrawInversedFaceNormals" ), false, TEXT("Whether to debug draw the Chaos Cloth inversed face normals"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawCollision           (TEXT("p.ChaosCloth.DebugDrawCollision"           ), false, TEXT("Whether to debug draw the Chaos Cloth collisions"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawBackstops           (TEXT("p.ChaosCloth.DebugDrawBackstops"           ), false, TEXT("Whether to debug draw the Chaos Cloth backstops"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawBackstopDistances   (TEXT("p.ChaosCloth.DebugDrawBackstopDistances"   ), false, TEXT("Whether to debug draw the Chaos Cloth backstop distances"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawMaxDistances        (TEXT("p.ChaosCloth.DebugDrawMaxDistances"        ), false, TEXT("Whether to debug draw the Chaos Cloth max distances"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawAnimDrive           (TEXT("p.ChaosCloth.DebugDrawAnimDrive"           ), false, TEXT("Whether to debug draw the Chaos Cloth anim drive"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawEdgeConstraint      (TEXT("p.ChaosCloth.DebugDrawEdgeConstraint"      ), false, TEXT("Whether to debug draw the Chaos Cloth edge constraint"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawBendingConstraint   (TEXT("p.ChaosCloth.DebugDrawBendingConstraint"   ), false, TEXT("Whether to debug draw the Chaos Cloth bending constraint"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawLongRangeConstraint (TEXT("p.ChaosCloth.DebugDrawLongRangeConstraint" ), false, TEXT("Whether to debug draw the Chaos Cloth long range constraint (aka tether constraint)"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawWindForces          (TEXT("p.ChaosCloth.DebugDrawWindForces"          ), false, TEXT("Whether to debug draw the Chaos Cloth wind forces"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawSelfCollision       (TEXT("p.ChaosCloth.DebugDrawSelfCollision"       ), false, TEXT("Whether to debug draw the Chaos Cloth self collision information"), ECVF_Cheat);
	TAutoConsoleVariable<bool> DebugDrawSelfIntersection    (TEXT("p.ChaosCloth.DebugDrawSelfIntersection"    ), false, TEXT("Whether to debug draw the Chaos Cloth self intersection information"), ECVF_Cheat);
}
#endif  // #if CHAOS_DEBUG_DRAW

#if !UE_BUILD_SHIPPING
namespace ClothingSimulationConsole
{
#if INTEL_ISPC
	static FAutoConsoleCommand CommandIspc(
		TEXT("p.ChaosCloth.Ispc"),
		TEXT("Enable or disable ISPC optimizations for cloth simulation."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			bool bEnableISPC;
			switch (Args.Num())
			{
			default:
				break; // Invalid arguments
			case 1:
				if (Args[0] == TEXT("1") || Args[0] == TEXT("true") || Args[0] == TEXT("on"))
				{
					bEnableISPC = true;
				}
				else if (Args[0] == TEXT("0") || Args[0] == TEXT("false") || Args[0] == TEXT("off"))
				{
					bEnableISPC = false;
				}
				else
				{
					break; // Invalid arguments
				}
				bChaos_AxialSpring_ISPC_Enabled =
					bChaos_LongRange_ISPC_Enabled =
					bChaos_Spherical_ISPC_Enabled =
					bChaos_Spring_ISPC_Enabled =
					bChaos_DampVelocity_ISPC_Enabled =
					bChaos_PerParticleCollision_ISPC_Enabled =
					bChaos_VelocityField_ISPC_Enabled =
					bChaos_GetSimData_ISPC_Enabled =
					bChaos_SkinPhysicsMesh_ISPC_Enabled =
					bChaos_PreSimulationTransforms_ISPC_Enabled =
					bChaos_CalculateBounds_ISPC_Enabled =
					bChaos_PostIterationUpdates_ISPC_Enabled =
					bChaos_Bending_ISPC_Enabled =
					bChaos_XPBDSpring_ISPC_Enabled =
					bChaos_XPBDBending_ISPC_Enabled =
					bEnableISPC;
				return;
			}

			UE_LOG(LogChaosCloth, Display, TEXT("Invalid arguments."));
			UE_LOG(LogChaosCloth, Display, TEXT("Usage:"));
			UE_LOG(LogChaosCloth, Display, TEXT("  p.ChaosCloth.Ispc [0|1]|[true|false]|[on|off]"));
			UE_LOG(LogChaosCloth, Display, TEXT("Example: p.Chaos.Ispc on"));
		}),
		ECVF_Cheat);
#endif // #if INTEL_ISPC

#if !UE_BUILD_TEST
	class FCommand final
	{
	public:
		FCommand()
			: StepCount(0)
			, bIsPaused(false)
			, ResetCount(0)
		{
			// Register DebugStep console command
			ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
				TEXT("p.ChaosCloth.DebugStep"),
				TEXT("Pause/step/resume cloth simulations."),
				FConsoleCommandWithArgsDelegate::CreateRaw(this, &FCommand::DebugStep),
				ECVF_Cheat));

			// Register Reset console command
			ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
				TEXT("p.ChaosCloth.Reset"),
				TEXT("Reset all cloth simulations."),
				FConsoleCommandDelegate::CreateRaw(this, &FCommand::Reset),
				ECVF_Cheat));
		}

		~FCommand()
		{
			for (IConsoleObject* ConsoleObject : ConsoleObjects)
			{
				IConsoleManager::Get().UnregisterConsoleObject(ConsoleObject);
			}
		}

		bool MustStep(int32& InOutStepCount) const
		{
			const bool bMustStep = !bIsPaused || (InOutStepCount != StepCount);
			InOutStepCount = StepCount;
			return bMustStep;
		}

		bool MustReset(int32& InOutResetCount) const
		{
			const bool bMustReset = (InOutResetCount != ResetCount);
			InOutResetCount = ResetCount;
			return bMustReset;
		}

	private:
		void DebugStep(const TArray<FString>& Args)
		{
			switch (Args.Num())
			{
			default:
				break;  // Invalid arguments
			case 1:
				if (Args[0].Compare(TEXT("Pause"), ESearchCase::IgnoreCase) == 0)
				{
					UE_CLOG(bIsPaused, LogChaosCloth, Warning, TEXT("Cloth simulations are already paused!"));
					UE_CLOG(!bIsPaused, LogChaosCloth, Display, TEXT("Cloth simulations are now pausing..."));
					bIsPaused = true;
				}
				else if (Args[0].Compare(TEXT("Step"), ESearchCase::IgnoreCase) == 0)
				{
					if (bIsPaused)
					{
						UE_LOG(LogChaosCloth, Display, TEXT("Cloth simulations are now stepping..."));
						++StepCount;
					}
					else
					{
						UE_LOG(LogChaosCloth, Warning, TEXT("The Cloth simulations aren't paused yet!"));
						UE_LOG(LogChaosCloth, Display, TEXT("Cloth simulations are now pausing..."));
						bIsPaused = true;
					}
				}
				else if (Args[0].Compare(TEXT("Resume"), ESearchCase::IgnoreCase) == 0)
				{
					UE_CLOG(!bIsPaused, LogChaosCloth, Warning, TEXT("Cloth simulations haven't been paused yet!"));
					UE_CLOG(bIsPaused, LogChaosCloth, Display, TEXT("Cloth simulations are now resuming..."));
					bIsPaused = false;
				}
				else
				{
					break;  // Invalid arguments
				}
				return;
			}
			UE_LOG(LogChaosCloth, Display, TEXT("Invalid arguments."));
			UE_LOG(LogChaosCloth, Display, TEXT("Usage:"));
			UE_LOG(LogChaosCloth, Display, TEXT("  p.ChaosCloth.DebugStep [Pause|Step|Resume]"));
		}

		void Reset()
		{
			UE_LOG(LogChaosCloth, Display, TEXT("All cloth simulations have now been asked to reset."));
			++ResetCount;
		}

	private:
		TArray<IConsoleObject*> ConsoleObjects;
		TAtomic<int32> StepCount;
		TAtomic<bool> bIsPaused;
		TAtomic<int32> ResetCount;
	};
	static TUniquePtr<FCommand> Command;
#endif // #if !UE_BUILD_TEST
}
#endif  // #if !UE_BUILD_SHIPPING

// Default parameters, will be overwritten when cloth assets are loaded
namespace ChaosClothingSimulationDefault
{
	static const FVector Gravity(0.f, 0.f, -980.665f);
	static const FReal MaxDistancesMultipliers = (FReal)1.;
}

FClothingSimulation::FClothingSimulation()
	: ClothSharedSimConfig(nullptr)
	, bUseLocalSpaceSimulation(false)
	, bUseGravityOverride(false)
	, GravityOverride(ChaosClothingSimulationDefault::Gravity)
	, MaxDistancesMultipliers(ChaosClothingSimulationDefault::MaxDistancesMultipliers)
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	, StepCount(0)
	, ResetCount(0)
#endif
	, bHasInvalidReferenceBoneTransforms(false)
{
#if WITH_EDITOR
	DebugClothMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EditorMaterials/Cloth/CameraLitDoubleSided.CameraLitDoubleSided"), nullptr, LOAD_None, nullptr);  // LOAD_EditorOnly
	DebugClothMaterialVertex = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EditorMaterials/WidgetVertexColorMaterial"), nullptr, LOAD_None, nullptr);  // LOAD_EditorOnly
#endif  // #if WITH_EDITOR
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!ClothingSimulationConsole::Command)
	{
		ClothingSimulationConsole::Command = MakeUnique<ClothingSimulationConsole::FCommand>();
	}
#endif
}

FClothingSimulation::~FClothingSimulation()
{}

void FClothingSimulation::Initialize()
{
	// Create solver
	Solver = MakeUnique<FClothingSimulationSolver>();

	ResetStats();
}

void FClothingSimulation::Shutdown()
{
	Solver.Reset();
	Meshes.Reset();
	Cloths.Reset();
	Colliders.Reset();
	ClothSharedSimConfig = nullptr;
}

void FClothingSimulation::DestroyActors()
{
	Shutdown();
	Initialize();
}

IClothingSimulationContext* FClothingSimulation::CreateContext()
{
	return new FClothingSimulationContext();
}

void FClothingSimulation::CreateActor(USkeletalMeshComponent* InOwnerComponent, UClothingAssetBase* InAsset, int32 InSimDataIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FClothingSimulation_CreateActor);
	SCOPE_CYCLE_COUNTER(STAT_ChaosClothCreateActor);

	check(InOwnerComponent);
	check(Solver);

	if (!InAsset)
	{
		return;
	}

	// ClothSharedSimConfig should either be a nullptr, or point to an object common to the whole skeletal mesh
	UClothingAssetCommon* const Asset = Cast<UClothingAssetCommon>(InAsset);
	if (!ClothSharedSimConfig)
	{
		ClothSharedSimConfig = Asset->GetClothConfig<UChaosClothSharedSimConfig>();

		UpdateSimulationFromSharedSimConfig();

		// Must set the local space location prior to adding any mesh/cloth, as otherwise the start poses would be in the wrong local space
		const FClothingSimulationContext* const Context = static_cast<const FClothingSimulationContext*>(InOwnerComponent->GetClothingSimulationContext());
		check(Context);
		static const bool bReset = true;
		Solver->SetLocalSpaceLocation(bUseLocalSpaceSimulation ? (FVec3)Context->ComponentToWorld.GetLocation() : FVec3(0.), bReset);
		Solver->SetLocalSpaceRotation(bUseLocalSpaceSimulation ? (FQuat)Context->ComponentToWorld.GetRotation() : FQuat::Identity);
	}
	else
	{
		check(ClothSharedSimConfig == Asset->GetClothConfig<UChaosClothSharedSimConfig>());
	}

	// Retrieve the cloth config stored in the asset
	const UChaosClothConfig* const ClothConfig = Asset->GetClothConfig<UChaosClothConfig>();
	if (!ClothConfig)
	{
		UE_LOG(LogChaosCloth, Warning, TEXT("Missing Chaos config Cloth LOD asset to %s in sim slot %d"), InOwnerComponent->GetOwner() ? *InOwnerComponent->GetOwner()->GetName() : TEXT("None"), InSimDataIndex);
		return;
	}

	// Create mesh node
	const int32 MeshIndex = Meshes.Emplace(MakeUnique<FClothingSimulationMesh>(
		Asset,
		InOwnerComponent));

	// Create collider node
	const int32 ColliderIndex = Colliders.Emplace(MakeUnique<FClothingSimulationCollider>(
		Asset,
		InOwnerComponent,
		/*bInUseLODIndexOverride =*/ false,
		/*InLODIndexOverride =*/ INDEX_NONE));

	// Set the external collision data to get updated at every frame
	Colliders[ColliderIndex]->SetCollisionData(&ExternalCollisionData);

	// Create cloth node
	const int32 ClothIndex = Cloths.Emplace(MakeUnique<FClothingSimulationCloth>(
		Meshes[MeshIndex].Get(),
		TArray<FClothingSimulationCollider*>({ Colliders[ColliderIndex].Get() }),
		InSimDataIndex,
		(FClothingSimulationCloth::EMassMode)ClothConfig->MassMode,
		ClothConfig->GetMassValue(),
		ClothConfig->MinPerParticleMass,
		TVec2<FRealSingle>(ClothConfig->EdgeStiffnessWeighted.Low, ClothConfig->EdgeStiffnessWeighted.High),
		TVec2<FRealSingle>(ClothConfig->BendingStiffnessWeighted.Low, ClothConfig->BendingStiffnessWeighted.High),
		ClothConfig->BucklingRatio,
		TVec2<FRealSingle>(ClothConfig->BucklingStiffnessWeighted.Low, ClothConfig->BucklingStiffnessWeighted.High),
		ClothConfig->bUseBendingElements,
		TVec2<FRealSingle>(ClothConfig->AreaStiffnessWeighted.Low, ClothConfig->AreaStiffnessWeighted.High),
		ClothConfig->VolumeStiffness,
		ClothConfig->bUseThinShellVolumeConstraints,
		TVec2<FRealSingle>(ClothConfig->TetherStiffness.Low, ClothConfig->TetherStiffness.High),  // Animatable
		TVec2<FRealSingle>(ClothConfig->TetherScale.Low, ClothConfig->TetherScale.High),  // Animatable
		ClothConfig->bUseGeodesicDistance ? FClothingSimulationCloth::ETetherMode::Geodesic : FClothingSimulationCloth::ETetherMode::Euclidean,
		/*MaxDistancesMultiplier =*/ 1.f,  // Animatable
		TVec2<FRealSingle>(ClothConfig->AnimDriveStiffness.Low, ClothConfig->AnimDriveStiffness.High),  // Animatable
		TVec2<FRealSingle>(ClothConfig->AnimDriveDamping.Low, ClothConfig->AnimDriveDamping.High),  // Animatable
		ClothConfig->ShapeTargetStiffness,  // TODO: This is now deprecated
		false, // bUseXPBDEdgeSprings
		false, // bUseXPBDBendingElements
		false, // bUseXPBDAreaSprings
		ClothConfig->GravityScale,
		ClothConfig->bUseGravityOverride,
		ClothConfig->Gravity,
		ClothConfig->LinearVelocityScale,
		ClothConfig->AngularVelocityScale,
		ClothConfig->FictitiousAngularScale,
		TVec2<FRealSingle>(ClothConfig->Drag.Low, ClothConfig->Drag.High),  // Animatable
		TVec2<FRealSingle>(ClothConfig->Lift.Low, ClothConfig->Lift.High),  // Animatable
		ClothConfig->bUsePointBasedWindModel,
		TVec2<FRealSingle>(ClothConfig->Pressure.Low, ClothConfig->Pressure.High),  // Animatable
		ClothConfig->DampingCoefficient,
		ClothConfig->LocalDampingCoefficient,
		ClothConfig->CollisionThickness,
		ClothConfig->FrictionCoefficient,
		ClothConfig->bUseCCD,
		ClothConfig->bUseSelfCollisions,
		ClothConfig->SelfCollisionThickness,
		ClothConfig->SelfCollisionFriction,
		ClothConfig->bUseSelfIntersections,
		ClothConfig->bUseLegacyBackstop,
		/*bUseLODIndexOverride =*/ false,
		/*LODIndexOverride =*/ INDEX_NONE));

	// Add cloth to solver
	Solver->AddCloth(Cloths[ClothIndex].Get());

	// Update stats
	UpdateStats(Cloths[ClothIndex].Get());

	UE_LOG(LogChaosCloth, Verbose, TEXT("Added Cloth asset to %s in sim slot %d"), InOwnerComponent->GetOwner() ? *InOwnerComponent->GetOwner()->GetName() : TEXT("None"), InSimDataIndex);
}

void FClothingSimulation::UpdateWorldForces(const USkeletalMeshComponent* OwnerComponent)
{
	if (OwnerComponent)
	{
		UWorld* OwnerWorld = OwnerComponent->GetWorld();
		if (OwnerWorld && OwnerWorld->PhysicsField && Solver)
		{
			const FBox BoundingBox = OwnerComponent->CalcBounds(OwnerComponent->GetComponentTransform()).GetBox();

			OwnerWorld->PhysicsField->FillTransientCommands(false, BoundingBox, Solver->GetTime(), Solver->GetPerSolverField().GetTransientCommands());
			OwnerWorld->PhysicsField->FillPersistentCommands(false, BoundingBox, Solver->GetTime(), Solver->GetPerSolverField().GetPersistentCommands());
		}
	}
}

void FClothingSimulation::ResetStats()
{
	check(Solver);
	NumCloths = 0;
	NumKinematicParticles = 0;
	NumDynamicParticles = 0;
	SimulationTime = 0.f;
	NumSubsteps = Solver->GetNumSubsteps();
	NumIterations = Solver->GetNumIterations();
	bHasInvalidReferenceBoneTransforms = false;
}

void FClothingSimulation::UpdateStats(const FClothingSimulationCloth* Cloth)
{
	NumCloths = Cloths.Num();
	NumKinematicParticles += Cloth->GetNumActiveKinematicParticles();
	NumDynamicParticles += Cloth->GetNumActiveDynamicParticles();
}

void FClothingSimulation::UpdateSimulationFromSharedSimConfig()
{
	check(Solver);
	if (ClothSharedSimConfig) // ClothSharedSimConfig will be a null pointer if all cloth instances are disabled in which case we will use default Evolution parameters
	{
		// Update local space simulation switch
		bUseLocalSpaceSimulation = ClothSharedSimConfig->bUseLocalSpaceSimulation;

		// Set common simulation parameters
		Solver->SetNumSubsteps(ClothSharedSimConfig->SubdivisionCount);
		Solver->SetNumIterations(ClothSharedSimConfig->IterationCount);
		Solver->SetMaxNumIterations(ClothSharedSimConfig->MaxIterationCount);
	}
}

void FClothingSimulation::SetNumIterations(int32 InNumIterations)
{
	Solver->SetNumIterations(InNumIterations);
}

void FClothingSimulation::SetMaxNumIterations(int32 MaxNumIterations)
{
	Solver->SetMaxNumIterations(MaxNumIterations);
}

void FClothingSimulation::SetNumSubsteps(int32 InNumSubsteps)
{
	Solver->SetNumSubsteps(InNumSubsteps);
}

bool FClothingSimulation::ShouldSimulate() const
{
	for (const TUniquePtr<FClothingSimulationCloth>& Cloth : Cloths)
	{
		if (Cloth->GetLODIndex(Solver.Get()) != INDEX_NONE && Cloth->GetOffset(Solver.Get()) != INDEX_NONE)
		{
			return true;
		}
	}
	return false;
}

void FClothingSimulation::Simulate(IClothingSimulationContext* InContext)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FClothingSimulation_Simulate);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (ClothingSimulationConsole::Command && ClothingSimulationConsole::Command->MustStep(StepCount))
#endif
	{
		SCOPE_CYCLE_COUNTER(STAT_ChaosClothSimulate);
		const FClothingSimulationContext* const Context = static_cast<FClothingSimulationContext*>(InContext);
		if (Context->DeltaSeconds == 0.f)
		{
			return;
		}

		// Filter delta time to smoothen time variations and prevent unwanted vibrations
		constexpr Softs::FSolverReal DeltaTimeDecay = (Softs::FSolverReal)0.1;
		const Softs::FSolverReal DeltaTime = (Softs::FSolverReal)Context->DeltaSeconds;
		const Softs::FSolverReal PrevDeltaTime = Solver->GetDeltaTime();
		const Softs::FSolverReal SmoothedDeltaTime = bClothUseTimeStepSmoothing ?
			PrevDeltaTime + (DeltaTime - PrevDeltaTime) * DeltaTimeDecay :
			DeltaTime;

		const double StartTime = FPlatformTime::Seconds();
		const float PrevSimulationTime = SimulationTime;  // Copy the atomic to prevent a re-read

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const bool bNeedsReset = (ClothingSimulationConsole::Command && ClothingSimulationConsole::Command->MustReset(ResetCount)) ||
			Context->TeleportMode == EClothingTeleportMode::TeleportAndReset || PrevSimulationTime == 0.f;
#else
		const bool bNeedsReset = Context->TeleportMode == EClothingTeleportMode::TeleportAndReset || PrevSimulationTime == 0.f;
#endif
		const bool bNeedsTeleport = (Context->TeleportMode > EClothingTeleportMode::None);
		bIsTeleported = bNeedsTeleport;

		// Update Solver animatable parameters
		Solver->SetLocalSpaceLocation(bUseLocalSpaceSimulation ? (FVec3)Context->ComponentToWorld.GetLocation() : FVec3(0), bNeedsReset);
		Solver->SetLocalSpaceRotation(bUseLocalSpaceSimulation ? (FQuat)Context->ComponentToWorld.GetRotation() : FQuat::Identity);
		Solver->SetWindVelocity(Context->WindVelocity, Context->WindAdaption);
		Solver->SetGravity(bUseGravityOverride ? GravityOverride : Context->WorldGravity);
		Solver->EnableClothGravityOverride(!bUseGravityOverride);  // Disable all cloth gravity overrides when the interactor takes over
		Solver->SetVelocityScale(!bNeedsReset ? (FReal)Context->VelocityScale * (FReal)SmoothedDeltaTime / DeltaTime : 1.f);

		// Check teleport modes
		for (const TUniquePtr<FClothingSimulationCloth>& Cloth : Cloths)
		{
			// Update Cloth animatable parameters while in the cloth loop
			Cloth->SetMaxDistancesMultiplier(Context->MaxDistanceScale);

			if (bNeedsReset)
			{
				Cloth->Reset();
			}
			if (bNeedsTeleport)
			{
				Cloth->Teleport();
			}
		}

		// Step the simulation
		if(Solver->GetEnableSolver() || (Context->CachedPositions.Num() == 0 && Context->CachedVelocities.Num() == 0))
		{
			Solver->Update(SmoothedDeltaTime);
		}
		else
		{
			Solver->UpdateFromCache(Context->CachedPositions, Context->CachedVelocities);
		}

		// Keep the actual used number of iterations for the stats
		NumIterations = Solver->GetNumUsedIterations();

		// Update simulation time in ms (and provide an instant average instead of the value in real-time)
		const float CurrSimulationTime = (float)((FPlatformTime::Seconds() - StartTime) * 1000.);
		static const float SimulationTimeDecay = 0.03f; // 0.03 seems to provide a good rate of update for the instant average
		SimulationTime = PrevSimulationTime ? PrevSimulationTime + (CurrSimulationTime - PrevSimulationTime) * SimulationTimeDecay : CurrSimulationTime;

#if FRAMEPRO_ENABLED
		FRAMEPRO_CUSTOM_STAT("ChaosClothSimulationTimeMs", SimulationTime, "ChaosCloth", "ms", FRAMEPRO_COLOUR(0,128,255));
		FRAMEPRO_CUSTOM_STAT("ChaosClothNumDynamicParticles", NumDynamicParticles, "ChaosCloth", "Particles", FRAMEPRO_COLOUR(0,128,128));
		FRAMEPRO_CUSTOM_STAT("ChaosClothNumKinematicParticles", NumKinematicParticles, "ChaosCloth", "Particles", FRAMEPRO_COLOUR(128, 0, 128));
#endif
	}

	// Debug draw
#if CHAOS_DEBUG_DRAW
	if (ClothingSimulationCVar::DebugDrawLocalSpace          .GetValueOnAnyThread()) { DebugDrawLocalSpace          (); }
	if (ClothingSimulationCVar::DebugDrawBounds              .GetValueOnAnyThread()) { DebugDrawBounds              (); }
	if (ClothingSimulationCVar::DebugDrawGravity             .GetValueOnAnyThread()) { DebugDrawGravity             (); }
	if (ClothingSimulationCVar::DebugDrawPhysMeshWired       .GetValueOnAnyThread()) { DebugDrawPhysMeshWired       (); }
	if (ClothingSimulationCVar::DebugDrawAnimMeshWired       .GetValueOnAnyThread()) { DebugDrawAnimMeshWired       (); }
	if (ClothingSimulationCVar::DebugDrawPointVelocities     .GetValueOnAnyThread()) { DebugDrawPointVelocities     (); }
	if (ClothingSimulationCVar::DebugDrawAnimNormals         .GetValueOnAnyThread()) { DebugDrawAnimNormals         (); }
	if (ClothingSimulationCVar::DebugDrawPointNormals        .GetValueOnAnyThread()) { DebugDrawPointNormals        (); }
	if (ClothingSimulationCVar::DebugDrawCollision           .GetValueOnAnyThread()) { DebugDrawCollision           (); }
	if (ClothingSimulationCVar::DebugDrawBackstops           .GetValueOnAnyThread()) { DebugDrawBackstops           (); }
	if (ClothingSimulationCVar::DebugDrawBackstopDistances   .GetValueOnAnyThread()) { DebugDrawBackstopDistances   (); }
	if (ClothingSimulationCVar::DebugDrawMaxDistances        .GetValueOnAnyThread()) { DebugDrawMaxDistances        (); }
	if (ClothingSimulationCVar::DebugDrawAnimDrive           .GetValueOnAnyThread()) { DebugDrawAnimDrive           (); }
	if (ClothingSimulationCVar::DebugDrawEdgeConstraint      .GetValueOnAnyThread()) { DebugDrawEdgeConstraint      (); }
	if (ClothingSimulationCVar::DebugDrawBendingConstraint   .GetValueOnAnyThread()) { DebugDrawBendingConstraint   (); }
	if (ClothingSimulationCVar::DebugDrawLongRangeConstraint .GetValueOnAnyThread()) { DebugDrawLongRangeConstraint (); }
	if (ClothingSimulationCVar::DebugDrawWindForces          .GetValueOnAnyThread()) { DebugDrawWindAndPressureForces(); }
	if (ClothingSimulationCVar::DebugDrawSelfCollision       .GetValueOnAnyThread()) { DebugDrawSelfCollision       (); }
	if (ClothingSimulationCVar::DebugDrawSelfIntersection    .GetValueOnAnyThread()) { DebugDrawSelfIntersection    (); }
#endif  // #if CHAOS_DEBUG_DRAW
}

void FClothingSimulation::GetSimulationData(
	TMap<int32, FClothSimulData>& OutData,
	USkeletalMeshComponent* InOwnerComponent,
	USkinnedMeshComponent* InOverrideComponent) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FClothingSimulation_GetSimulationData);
	SCOPE_CYCLE_COUNTER(STAT_ChaosClothGetSimulationData);

	if (!Cloths.Num() || !InOwnerComponent)
	{
		OutData.Reset();
		return;
	}

	// Reset map when new cloths have appeared
	if (OutData.Num() != Cloths.Num())
	{
		OutData.Reset();
	}

	// Get the solver's local space
	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation(); // Note: Since the ReferenceSpaceTransform can be suspended with the simulation, it is important that the suspended local space location is used too in order to get the simulation data back into reference space

	// Retrieve the component transforms
	const FClothingSimulationContext* const Context = static_cast<const FClothingSimulationContext*>(InOwnerComponent->GetClothingSimulationContext());
	check(Context);  // A simulation can't be created without context
	const FTransform& OwnerTransform = Context->ComponentToWorld;

	const TArray<FTransform>& ComponentSpaceTransforms = InOverrideComponent ? InOverrideComponent->GetComponentSpaceTransforms() : InOwnerComponent->GetComponentSpaceTransforms();

	// Set the simulation data for each of the cloths
	for (const TUniquePtr<FClothingSimulationCloth>& Cloth : Cloths)
	{
		const int32 AssetIndex = Cloth->GetGroupId();

		if (!Cloth->GetMesh())
		{
			OutData.Remove(AssetIndex);  // Ensures that the cloth vertex factory won't run unnecessarily
			continue;  // Invalid or empty cloth
		}

		// If the LOD has changed while the simulation is suspended, the cloth still needs to be updated with the correct LOD data
		const int32 LODIndex = Cloth->GetMesh()->GetLODIndex();
		if (LODIndex != Cloth->GetLODIndex(Solver.Get()))
		{
			Solver->Update(FSolverReal(0.));  // Update for LOD switching, but do not simulate
		}

		if (Cloth->GetOffset(Solver.Get()) == INDEX_NONE || Cloth->GetLODIndex(Solver.Get()) == INDEX_NONE)
		{
			OutData.Remove(AssetIndex);  // Ensures that the cloth vertex factory won't run unnecessarily
			continue;  // No valid LOD, there's nothing to write out
		}

		// Get the reference bone index for this cloth
		const int32 ReferenceBoneIndex = InOverrideComponent ? InOwnerComponent->GetLeaderBoneMap()[Cloth->GetReferenceBoneIndex()] : Cloth->GetReferenceBoneIndex();
		if (!ComponentSpaceTransforms.IsValidIndex(ReferenceBoneIndex))
		{
			UE_CLOG(!bHasInvalidReferenceBoneTransforms, LogSkeletalMesh, Warning, TEXT("Failed to write back clothing simulation data for component %s as bone transforms are invalid."), *InOwnerComponent->GetName());
			bHasInvalidReferenceBoneTransforms = true;
			OutData.Reset();
			return;
		}

		// Get the reference transform used in the current animation pose
		FTransform ReferenceBoneTransform = ComponentSpaceTransforms[ReferenceBoneIndex];
		ReferenceBoneTransform *= OwnerTransform;
		ReferenceBoneTransform.SetScale3D(FVector(1.0f));  // Scale is already baked in the cloth mesh

		// Set the world space transform to be this cloth's reference bone
		FClothSimulData& Data = OutData.FindOrAdd(AssetIndex);
		Data.Transform = ReferenceBoneTransform;
		Data.ComponentRelativeTransform = ReferenceBoneTransform.GetRelativeTransform(OwnerTransform);

		// Retrieve the last reference space transform used for this cloth
		// Note: This won't necessary match the current bone reference transform when the simulation is paused,
		//       and still allows for the correct positioning of the sim data while the component is animated.
		const FRigidTransform3& ReferenceSpaceTransform = Cloth->GetReferenceSpaceTransform();

		// Copy positions and normals
		Data.Positions = Cloth->GetParticlePositions(Solver.Get());
		Data.Normals = Cloth->GetParticleNormals(Solver.Get());

		// Transform into the cloth reference simulation space used at the time of simulation
#if INTEL_ISPC
		if (bChaos_GetSimData_ISPC_Enabled)
		{
			// ISPC is assuming float input here
			check(sizeof(ispc::FVector3f) == Data.Positions.GetTypeSize());
			check(sizeof(ispc::FVector3f) == Data.Normals.GetTypeSize());

			ispc::GetClothingSimulationData(
				(ispc::FVector3f*)Data.Positions.GetData(),
				(ispc::FVector3f*)Data.Normals.GetData(),
				(ispc::FTransform&)ReferenceSpaceTransform,
				(ispc::FVector&)LocalSpaceLocation,
				Data.Positions.Num());
		}
		else
#endif
		{
			for (int32 Index = 0; Index < Data.Positions.Num(); ++Index)
			{
				using FPositionsType = decltype(Data.Positions)::ElementType;
				using FNormalsType = decltype(Data.Normals)::ElementType;
				Data.Positions[Index] = FPositionsType(ReferenceSpaceTransform.InverseTransformPosition(FVec3(Data.Positions[Index]) + LocalSpaceLocation));  // Move into world space first
				Data.Normals[Index] = FNormalsType(ReferenceSpaceTransform.InverseTransformVector(FVec3(-Data.Normals[Index])));  // Normals are inverted due to how barycentric coordinates are calculated (see GetPointBaryAndDist in ClothingMeshUtils.cpp)
			}
		}

		// Set the current LOD these data apply to, so that the correct deformer mappings can be applied
		if (const UClothingAssetCommon* const ClothingAsset = Cloth->GetMesh()->GetAsset())
		{
			Data.LODIndex = ClothingAsset->LodMap.Find(LODIndex);  // Store the mesh LOD index, which is different to the cloth asset's LOD index
		}
		else
		{
			Data.LODIndex = 0;
		}
	}
}

FBoxSphereBounds FClothingSimulation::GetBounds(const USkeletalMeshComponent* InOwnerComponent) const
{
	check(Solver);
	FBoxSphereBounds Bounds = Solver->CalculateBounds();

	if (bUseLocalSpaceSimulation)
	{
		// The component could be moving while the simulation is suspended so getting the bounds
		// in world space isn't good enough and the bounds origin needs to be continuously updated
		Bounds = Bounds.TransformBy(FTransform((FQuat)Solver->GetLocalSpaceRotation(), (FVector)Solver->GetLocalSpaceLocation()).Inverse());
	}
	else if (InOwnerComponent)
	{
		const FClothingSimulationContext* const Context = static_cast<const FClothingSimulationContext*>(InOwnerComponent->GetClothingSimulationContext());
		check(Context);  // A simulation can't be created without context
		const FTransform& OwnerTransform = Context->ComponentToWorld;
		// Return local bounds
		Bounds = Bounds.TransformBy(OwnerTransform.Inverse());
	}
	// Else return bounds in world space
	return Bounds;
}

void FClothingSimulation::AddExternalCollisions(const FClothCollisionData& InData)
{
	ExternalCollisionData.Append(InData);
}

void FClothingSimulation::ClearExternalCollisions()
{
	ExternalCollisionData.Reset();
}

void FClothingSimulation::GetCollisions(FClothCollisionData& OutCollisions, bool bIncludeExternal) const
{
	// This code only gathers old apex collisions that don't appear in the physics mesh
	// It is also never called with bIncludeExternal = true 
	// but the collisions are then added untransformed and added as external
	// This function is bound to be deprecated at some point

	OutCollisions.Reset();

	// Add internal asset collisions
	for (const TUniquePtr<FClothingSimulationCloth>& Cloth : Cloths)
	{
		for (const FClothingSimulationCollider* const Collider : Cloth->GetColliders())
		{
			OutCollisions.Append(Collider->GetCollisionData(Solver.Get(), Cloth.Get()));
		}
	}

	// Add external asset collisions
	if (bIncludeExternal)
	{
		OutCollisions.Append(ExternalCollisionData);
	}

	UE_LOG(LogChaosCloth, VeryVerbose, TEXT("GetCollisions returned collisions: %d spheres, %d capsules, %d convexes, %d boxes."), OutCollisions.Spheres.Num() - 2 * OutCollisions.SphereConnections.Num(), OutCollisions.SphereConnections.Num(), OutCollisions.Convexes.Num(), OutCollisions.Boxes.Num());
}

void FClothingSimulation::RefreshClothConfig(const IClothingSimulationContext* InContext)
{
	UpdateSimulationFromSharedSimConfig();

	// Update new space location
	const FClothingSimulationContext* const Context = static_cast<const FClothingSimulationContext*>(InContext);
	static const bool bReset = true;
	Solver->SetLocalSpaceLocation(bUseLocalSpaceSimulation ? (FVec3)Context->ComponentToWorld.GetLocation() : FVec3(0), bReset);
	Solver->SetLocalSpaceRotation(bUseLocalSpaceSimulation ? (FQuat)Context->ComponentToWorld.GetRotation() : FQuat::Identity);

	// Reset stats
	ResetStats();

	// Clear all cloths from the solver
	Solver->RemoveCloths();

	// Recreate all cloths
	for (TUniquePtr<FClothingSimulationCloth>& Cloth : Cloths)
	{
		FClothingSimulationMesh* const Mesh = Cloth->GetMesh();
		TArray<FClothingSimulationCollider*> ClothColliders = Cloth->GetColliders();
		const uint32 GroupId = Cloth->GetGroupId();
		const UChaosClothConfig* const ClothConfig = Mesh->GetAsset()->GetClothConfig<UChaosClothConfig>();

		Cloth = MakeUnique<FClothingSimulationCloth>(
			Mesh,
			MoveTemp(ClothColliders),
			GroupId,
			(FClothingSimulationCloth::EMassMode)ClothConfig->MassMode,
			ClothConfig->GetMassValue(),
			ClothConfig->MinPerParticleMass,
			TVec2<FRealSingle>(ClothConfig->EdgeStiffnessWeighted.Low, ClothConfig->EdgeStiffnessWeighted.High),
			TVec2<FRealSingle>(ClothConfig->BendingStiffnessWeighted.Low, ClothConfig->BendingStiffnessWeighted.High),
			ClothConfig->BucklingRatio,
			TVec2<FRealSingle>(ClothConfig->BucklingStiffnessWeighted.Low, ClothConfig->BucklingStiffnessWeighted.High),
			ClothConfig->bUseBendingElements,
			TVec2<FRealSingle>(ClothConfig->AreaStiffnessWeighted.Low, ClothConfig->AreaStiffnessWeighted.High),
			ClothConfig->VolumeStiffness,
			ClothConfig->bUseThinShellVolumeConstraints,
			TVec2<FRealSingle>(ClothConfig->TetherStiffness.Low, ClothConfig->TetherStiffness.High),  // Animatable
			TVec2<FRealSingle>(ClothConfig->TetherScale.Low, ClothConfig->TetherScale.High),  // Animatable
			ClothConfig->bUseGeodesicDistance ? FClothingSimulationCloth::ETetherMode::Geodesic : FClothingSimulationCloth::ETetherMode::Euclidean,
			/*MaxDistancesMultiplier =*/ 1.f,  // Animatable
			TVec2<FRealSingle>(ClothConfig->AnimDriveStiffness.Low, ClothConfig->AnimDriveStiffness.High),  // Animatable
			TVec2<FRealSingle>(ClothConfig->AnimDriveDamping.Low, ClothConfig->AnimDriveDamping.High),  // Animatable
			ClothConfig->ShapeTargetStiffness,
			false, // bUseXPBDEdgeSprings
			false, // bUseXPBDBendingElements
			false, // bUseXPBDAreaSprings
			ClothConfig->GravityScale,
			ClothConfig->bUseGravityOverride,
			ClothConfig->Gravity,
			ClothConfig->LinearVelocityScale,
			ClothConfig->AngularVelocityScale,
			ClothConfig->FictitiousAngularScale,
			TVec2<FRealSingle>(ClothConfig->Drag.Low, ClothConfig->Drag.High),  // Animatable
			TVec2<FRealSingle>(ClothConfig->Lift.Low, ClothConfig->Lift.High),  // Animatable
			ClothConfig->bUsePointBasedWindModel,
			TVec2<FRealSingle>(ClothConfig->Pressure.Low, ClothConfig->Pressure.High),  // Animatable
			ClothConfig->DampingCoefficient,
			ClothConfig->LocalDampingCoefficient,
			ClothConfig->CollisionThickness,
			ClothConfig->FrictionCoefficient,
			ClothConfig->bUseCCD,
			ClothConfig->bUseSelfCollisions,
			ClothConfig->SelfCollisionThickness,
			ClothConfig->SelfCollisionFriction,
			ClothConfig->bUseSelfIntersections,
			ClothConfig->bUseLegacyBackstop,
			/*bUseLODIndexOverride =*/ false,
			/*LODIndexOverride =*/ INDEX_NONE);

		// Re-add cloth to the solver
		Solver->AddCloth(Cloth.Get());

		// Update stats
		UpdateStats(Cloth.Get());
	}
	UE_LOG(LogChaosCloth, VeryVerbose, TEXT("RefreshClothConfig, all constraints and self-collisions have been updated for all clothing assets and LODs."));
}

void FClothingSimulation::RefreshPhysicsAsset()
{
	// A collider update cannot be re-triggered for now, refresh all cloths from the solver instead
	Solver->RefreshCloths();

	UE_LOG(LogChaosCloth, VeryVerbose, TEXT("RefreshPhysicsAsset, all collisions have been re-added for all clothing assets"));
}

void FClothingSimulation::SetGravityOverride(const FVector& InGravityOverride)
{
	bUseGravityOverride = true;
	GravityOverride = InGravityOverride;
}

void FClothingSimulation::DisableGravityOverride()
{
	bUseGravityOverride = false;
}

FClothingSimulationCloth* Chaos::FClothingSimulation::GetCloth(int32 ClothId)
{
	TUniquePtr<FClothingSimulationCloth>* const Cloth = Cloths.FindByPredicate(
		[ClothId](TUniquePtr<FClothingSimulationCloth>& InCloth)
		{
			return InCloth->GetGroupId() == ClothId;
		});

	return Cloth ? Cloth->Get(): nullptr;
}

#if WITH_EDITOR
void FClothingSimulation::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(DebugClothMaterial);
}

void FClothingSimulation::DebugDrawPhysMeshShaded(FPrimitiveDrawInterface* PDI) const
{
	if (!DebugClothMaterial)
	{
		return;
	}

	FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
	int32 VertexIndex = 0;

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const TConstArrayView<TVec3<int32>> Elements = Cloth->GetTriangleMesh(Solver.Get()).GetElements();
		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());
		const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver.Get());
		check(InvMasses.Num() == Positions.Num());

		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex, VertexIndex += 3)
		{
			const TVec3<int32>& Element = Elements[ElementIndex];

			const FVector3f Pos0(Positions[Element.X - Offset]); // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
			const FVector3f Pos1(Positions[Element.Y - Offset]);
			const FVector3f Pos2(Positions[Element.Z - Offset]);

			const FVector3f Normal = FVector3f::CrossProduct(Pos2 - Pos0, Pos1 - Pos0).GetSafeNormal();
			const FVector3f Tangent = ((Pos1 + Pos2) * 0.5f - Pos0).GetSafeNormal();

			const bool bIsKinematic0 = (InvMasses[Element.X - Offset] == (Softs::FSolverReal)0.);
			const bool bIsKinematic1 = (InvMasses[Element.Y - Offset] == (Softs::FSolverReal)0.);
			const bool bIsKinematic2 = (InvMasses[Element.Z - Offset] == (Softs::FSolverReal)0.);

			MeshBuilder.AddVertex(FDynamicMeshVertex(Pos0, Tangent, Normal, FVector2f(0.f, 0.f), bIsKinematic0 ? FColor::Purple : FColor::White));
			MeshBuilder.AddVertex(FDynamicMeshVertex(Pos1, Tangent, Normal, FVector2f(0.f, 1.f), bIsKinematic1 ? FColor::Purple : FColor::White));
			MeshBuilder.AddVertex(FDynamicMeshVertex(Pos2, Tangent, Normal, FVector2f(1.f, 1.f), bIsKinematic2 ? FColor::Purple : FColor::White));
			MeshBuilder.AddTriangle(VertexIndex, VertexIndex + 1, VertexIndex + 2);
		}
	}

	FMatrix LocalSimSpaceToWorld(FMatrix::Identity);
	LocalSimSpaceToWorld.SetOrigin(Solver->GetLocalSpaceLocation());
	MeshBuilder.Draw(PDI, LocalSimSpaceToWorld, DebugClothMaterial->GetRenderProxy(), SDPG_World, false, false);
}

static void DrawText(FCanvas* Canvas, const FSceneView* SceneView, const FVector& Pos, const FText& Text, const FLinearColor& Color)
{
	FVector2D PixelLocation;
	if (SceneView->WorldToPixel(Pos, PixelLocation))
	{
		FCanvasTextItem TextItem(PixelLocation, Text, GEngine->GetSmallFont(), Color);
		TextItem.Scale = FVector2D::UnitVector;
		TextItem.EnableShadow(FLinearColor::Black);
		TextItem.Draw(Canvas);
	}
}

void FClothingSimulation::DebugDrawParticleIndices(FCanvas* Canvas, const FSceneView* SceneView) const
{
	static const FLinearColor DynamicColor = FColor::White;
	static const FLinearColor KinematicColor = FColor::Purple;

	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());
		const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver.Get());
		check(InvMasses.Num() == Positions.Num());

		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			const FVector Position = LocalSpaceLocation + FVector(Positions[Index]);

			const FText Text = FText::AsNumber(Offset + Index);
			DrawText(Canvas, SceneView, Position, Text, InvMasses[Index] == (Softs::FSolverReal)0. ? KinematicColor : DynamicColor);
		}
	}
}

void FClothingSimulation::DebugDrawElementIndices(FCanvas* Canvas, const FSceneView* SceneView) const
{
	static const FLinearColor DynamicColor = FColor::White;
	static const FLinearColor KinematicColor = FColor::Purple;

	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const TArray<TVec3<int32>>& Elements = Cloth->GetTriangleMesh(Solver.Get()).GetElements();
		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());
		const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver.Get());
		check(InvMasses.Num() == Positions.Num());

		for (int32 Index = 0; Index < Elements.Num(); ++Index)
		{
			const TVec3<int32>& Element = Elements[Index];
			const FVector Position = LocalSpaceLocation + FVector(Positions[Element[0]] + Positions[Element[1]] + Positions[Element[2]]) / (FReal)3.;

			const bool bIsKinematic0 = (InvMasses[Element.X - Offset] == (Softs::FSolverReal)0.);
			const bool bIsKinematic1 = (InvMasses[Element.Y - Offset] == (Softs::FSolverReal)0.);
			const bool bIsKinematic2 = (InvMasses[Element.Z - Offset] == (Softs::FSolverReal)0.);
			const FLinearColor& Color = (bIsKinematic0 && bIsKinematic1 && bIsKinematic2) ? KinematicColor : DynamicColor;
			const FText Text = FText::AsNumber(Index);
			DrawText(Canvas, SceneView, Position, Text, Color);
		}
	}
}

void FClothingSimulation::DebugDrawMaxDistanceValues(FCanvas* Canvas, const FSceneView* SceneView) const
{
	static const FLinearColor DynamicColor = FColor::White;
	static const FLinearColor KinematicColor = FColor::Purple;

	FNumberFormattingOptions NumberFormattingOptions;
	NumberFormattingOptions.AlwaysSign = false;
	NumberFormattingOptions.UseGrouping = false;
	NumberFormattingOptions.RoundingMode = ERoundingMode::HalfFromZero;
	NumberFormattingOptions.MinimumIntegralDigits = 1;
	NumberFormattingOptions.MaximumIntegralDigits = 6;
	NumberFormattingOptions.MinimumFractionalDigits = 2;
	NumberFormattingOptions.MaximumFractionalDigits = 2;

	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const TConstArrayView<FRealSingle>& MaxDistances = Cloth->GetWeightMaps(Solver.Get())[(int32)EChaosWeightMapTarget::MaxDistance];
		if (!MaxDistances.Num())
		{
			continue;
		}

		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetAnimationPositions(Solver.Get());
		const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver.Get());
		check(MaxDistances.Num() == Positions.Num());
		check(MaxDistances.Num() == InvMasses.Num());

		for (int32 Index = 0; Index < MaxDistances.Num(); ++Index)
		{
			const FReal MaxDistance = MaxDistances[Index];
			const FVector Position = LocalSpaceLocation + FVector(Positions[Index]);

			const FText Text = FText::AsNumber(MaxDistance, &NumberFormattingOptions);
			DrawText(Canvas, SceneView, Position, Text, InvMasses[Index] == (Softs::FSolverReal)0. ? KinematicColor : DynamicColor);
		}
	}
}
#endif  // #if WITH_EDITOR

#if WITH_EDITOR || CHAOS_DEBUG_DRAW
static void DrawPoint(FPrimitiveDrawInterface* PDI, const FVector& Pos, const FLinearColor& Color, UMaterial* DebugClothMaterialVertex, const float Thickness = 1.f)  // Use color or material
{
#if CHAOS_DEBUG_DRAW
	if (!PDI)
	{
		FDebugDrawQueue::GetInstance().DrawDebugPoint(Pos, Color.ToFColor(true), false, KINDA_SMALL_NUMBER, SDPG_Foreground, Thickness);
		return;
	}
#endif
#if WITH_EDITOR
	if (DebugClothMaterialVertex)
	{
		const FMatrix& ViewMatrix = PDI->View->ViewMatrices.GetViewMatrix();
		const FVector XAxis = ViewMatrix.GetColumn(0); // Just using transpose here (orthogonal transform assumed)
		const FVector YAxis = ViewMatrix.GetColumn(1);
		DrawDisc(PDI, Pos, XAxis, YAxis, Color.ToFColor(true), 0.5f, 10, DebugClothMaterialVertex->GetRenderProxy(), SDPG_World);
	}
	else
	{
		PDI->DrawPoint(Pos, Color, Thickness, SDPG_World);
	}
#endif
}

static void DrawLine(FPrimitiveDrawInterface* PDI, const FVector& Pos0, const FVector& Pos1, const FLinearColor& Color)
{
#if CHAOS_DEBUG_DRAW
	if (!PDI)
	{
		FDebugDrawQueue::GetInstance().DrawDebugLine(Pos0, Pos1, Color.ToFColor(true), false, KINDA_SMALL_NUMBER, SDPG_Foreground, 0.f);
		return;
	}
#endif
#if WITH_EDITOR
	PDI->DrawLine(Pos0, Pos1, Color, SDPG_World, 0.0f, 0.001f);
#endif
}

static void DrawArc(FPrimitiveDrawInterface* PDI, const FVector& Base, const FVector& X, const FVector& Y, FReal MinAngle, FReal MaxAngle, FReal Radius, const FLinearColor& Color)
{
	static const int32 Sections = 10;
	const FReal AngleStep = FMath::DegreesToRadians((MaxAngle - MinAngle) / (FReal)Sections);
	FReal CurrentAngle = FMath::DegreesToRadians(MinAngle);
	FVector LastVertex = Base + Radius * (FMath::Cos(CurrentAngle) * X + FMath::Sin(CurrentAngle) * Y);

	for(int32 i = 0; i < Sections; i++)
	{
		CurrentAngle += AngleStep;
		const FVector ThisVertex = Base + Radius * (FMath::Cos(CurrentAngle) * X + FMath::Sin(CurrentAngle) * Y);
		DrawLine(PDI, LastVertex, ThisVertex, Color);
		LastVertex = ThisVertex;
	}
}

static void DrawSphere(FPrimitiveDrawInterface* PDI, const TSphere<FReal, 3>& Sphere, const FQuat& Rotation, const FVector& Position, const FLinearColor& Color)
{
	const FReal Radius = Sphere.GetRadius();
	const FVec3 Center = Position + Rotation.RotateVector(Sphere.GetCenter());
#if CHAOS_DEBUG_DRAW
	if (!PDI)
	{
		FDebugDrawQueue::GetInstance().DrawDebugSphere(Center, Radius, 12, Color.ToFColor(true), false, KINDA_SMALL_NUMBER, SDPG_Foreground, 0.f);
		return;
	}
#endif
#if WITH_EDITOR
	const FTransform Transform(Rotation, Center);
	DrawWireSphere(PDI, Transform, Color, Radius, 12, SDPG_World, 0.0f, 0.001f, false);
#endif
}

static void DrawBox(FPrimitiveDrawInterface* PDI, const FAABB3& Box, const FQuat& Rotation, const FVector& Position, const FLinearColor& Color)
{
#if CHAOS_DEBUG_DRAW
	if (!PDI)
	{
		const FVec3 Center = Position + Rotation.RotateVector(Box.GetCenter());
		FDebugDrawQueue::GetInstance().DrawDebugBox(Center, Box.Extents() * 0.5f, Rotation, Color.ToFColor(true), false, KINDA_SMALL_NUMBER, SDPG_Foreground, 0.f);
		return;
	}
#endif
#if WITH_EDITOR
	const FMatrix BoxToWorld = FTransform(Rotation, Position).ToMatrixNoScale();
	DrawWireBox(PDI, BoxToWorld, FBox(Box.Min(), Box.Max()), Color, SDPG_World, 0.0f, 0.001f, false);
#endif
}

static void DrawCapsule(FPrimitiveDrawInterface* PDI, const FCapsule& Capsule, const FQuat& Rotation, const FVector& Position, const FLinearColor& Color)
{
	const FReal Radius = Capsule.GetRadius();
	const FReal HalfHeight = Capsule.GetHeight() * 0.5f + Radius;
	const FVec3 Center = Position + Rotation.RotateVector(Capsule.GetCenter());
#if CHAOS_DEBUG_DRAW
	if (!PDI)
	{
		const FQuat Orientation = FQuat::FindBetweenNormals(FVec3::UpVector, Capsule.GetAxis());
		FDebugDrawQueue::GetInstance().DrawDebugCapsule(Center, HalfHeight, Radius, Rotation * Orientation, Color.ToFColor(true), false, KINDA_SMALL_NUMBER, SDPG_Foreground, 0.f);
		return;
	}
#endif
#if WITH_EDITOR
	const FVec3 Up = Capsule.GetAxis();
	FVec3 Forward, Right;
	Up.FindBestAxisVectors(Forward, Right);
	const FVector X = Rotation.RotateVector(Forward);
	const FVector Y = Rotation.RotateVector(Right);
	const FVector Z = Rotation.RotateVector(Up);
	DrawWireCapsule(PDI, Center, X, Y, Z, Color, Radius, HalfHeight, 12, SDPG_World, 0.0f, 0.001f, false);
#endif
}

static void DrawTaperedCylinder(FPrimitiveDrawInterface* PDI, const FTaperedCylinder& TaperedCylinder, const FQuat& Rotation, const FVector& Position, const FLinearColor& Color)
{
	const FReal HalfHeight = TaperedCylinder.GetHeight() * 0.5f;
	const FReal Radius1 = TaperedCylinder.GetRadius1();
	const FReal Radius2 = TaperedCylinder.GetRadius2();
	const FVector Position1 = Position + Rotation.RotateVector(TaperedCylinder.GetX1());
	const FVector Position2 = Position + Rotation.RotateVector(TaperedCylinder.GetX2());
	const FQuat Q = (Position2 - Position1).ToOrientationQuat();
	const FVector I = Q.GetRightVector();
	const FVector J = Q.GetUpVector();

	static const int32 NumSides = 12;
	static const FReal	AngleDelta = (FReal)2. * (FReal)PI / NumSides;
	FVector	LastVertex1 = Position1 + I * Radius1;
	FVector	LastVertex2 = Position2 + I * Radius2;

	for (int32 SideIndex = 1; SideIndex <= NumSides; ++SideIndex)
	{
		const FReal Angle = AngleDelta * FReal(SideIndex);
		const FVector ArcPos = I * FMath::Cos(Angle) + J * FMath::Sin(Angle);
		const FVector Vertex1 = Position1 + ArcPos * Radius1;
		const FVector Vertex2 = Position2 + ArcPos * Radius2;

		DrawLine(PDI, LastVertex1, Vertex1, Color);
		DrawLine(PDI, LastVertex2, Vertex2, Color);
		DrawLine(PDI, LastVertex1, LastVertex2, Color);

		LastVertex1 = Vertex1;
		LastVertex2 = Vertex2;
	}
}

static void DrawConvex(FPrimitiveDrawInterface* PDI, const FConvex& Convex, const FQuat& Rotation, const FVector& Position, const FLinearColor& Color)
{
	const TArray<FConvex::FPlaneType>& Planes = Convex.GetFaces();
	for (int32 PlaneIndex1 = 0; PlaneIndex1 < Planes.Num(); ++PlaneIndex1)
	{
		const FConvex::FPlaneType& Plane1 = Planes[PlaneIndex1];

		for (int32 PlaneIndex2 = PlaneIndex1 + 1; PlaneIndex2 < Planes.Num(); ++PlaneIndex2)
		{
			const FConvex::FPlaneType& Plane2 = Planes[PlaneIndex2];

			// Find the two surface points that belong to both Plane1 and Plane2
			uint32 ParticleIndex1 = INDEX_NONE;

			const TArray<FConvex::FVec3Type>& Vertices = Convex.GetVertices();
			for (int32 ParticleIndex = 0; ParticleIndex < Vertices.Num(); ++ParticleIndex)
			{
				const FConvex::FVec3Type& X = Vertices[ParticleIndex];

				if (FMath::Square(Plane1.SignedDistance(X)) < KINDA_SMALL_NUMBER && 
					FMath::Square(Plane2.SignedDistance(X)) < KINDA_SMALL_NUMBER)
				{
					if (ParticleIndex1 != INDEX_NONE)
					{
						const FVector X1(Vertices[ParticleIndex1]);
						const FVector X2(X);
						const FVector Position1 = Position + Rotation.RotateVector(X1);
						const FVector Position2 = Position + Rotation.RotateVector(X2);
						DrawLine(PDI, Position1, Position2, Color);
						break;
					}
					ParticleIndex1 = ParticleIndex;
				}
			}
		}
	}
}

static void DrawCoordinateSystem(FPrimitiveDrawInterface* PDI, const FQuat& Rotation, const FVector& Position)
{
	const FVector X = Rotation.RotateVector(FVector::ForwardVector) * 10.f;
	const FVector Y = Rotation.RotateVector(FVector::RightVector) * 10.f;
	const FVector Z = Rotation.RotateVector(FVector::UpVector) * 10.f;

	DrawLine(PDI, Position, Position + X, FLinearColor::Red);
	DrawLine(PDI, Position, Position + Y, FLinearColor::Green);
	DrawLine(PDI, Position, Position + Z, FLinearColor::Blue);
}

#if CHAOS_DEBUG_DRAW
void FClothingSimulation::DebugDrawBounds() const
{
	check(Solver);

	// Calculate World space bounds
	const FBoxSphereBounds Bounds = Solver->CalculateBounds();

	// Draw bounds
	DrawBox(nullptr, FAABB3(-Bounds.BoxExtent, Bounds.BoxExtent), FQuat::Identity, Bounds.Origin, FLinearColor(FColor::Purple));
	DrawSphere(nullptr, TSphere<FReal, 3>(FVector::ZeroVector, Bounds.SphereRadius), FQuat::Identity, Bounds.Origin, FLinearColor(FColor::Orange));

	// Draw individual cloth bounds
	static const FLinearColor Color = FLinearColor(FColor::Purple).Desaturate(0.5);
	for (const TUniquePtr<FClothingSimulationCloth>& Cloth : Cloths)
	{
		if (Cloth->GetOffset(Solver.Get()) == INDEX_NONE)
		{
			continue;
		}

		const FAABB3 BoundingBox = Cloth->CalculateBoundingBox(Solver.Get());
		DrawBox(nullptr, BoundingBox, FQuat::Identity, FVector::ZeroVector, Color);  // TODO: Express bounds in local coordinates for LWC
	}
}

void FClothingSimulation::DebugDrawGravity() const
{
	check(Solver);

	// Draw gravity
	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		if (Cloth->GetOffset(Solver.Get()) == INDEX_NONE)
		{
			continue;
		}

		const FAABB3 Bounds = Cloth->CalculateBoundingBox(Solver.Get());

		const FVector Pos0 = Bounds.Center();
		const FVector Pos1 = Pos0 + FVector(Cloth->GetGravity(Solver.Get()));
		DrawLine(nullptr, Pos0, Pos1, FLinearColor::Red);
	}
}
#endif  // #if CHAOS_DEBUG_DRAW

void FClothingSimulation::DebugDrawPhysMeshWired(FPrimitiveDrawInterface* PDI) const
{
	check(Solver);

	static const FLinearColor DynamicColor = FColor::White;
	static const FLinearColor KinematicColor = FColor::Purple;

	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const TConstArrayView<TVec3<int32>> Elements = Cloth->GetTriangleMesh(Solver.Get()).GetElements();
		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());
		const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver.Get());
		check(InvMasses.Num() == Positions.Num());

		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
		{
			const auto& Element = Elements[ElementIndex];

			const FVector Pos0 = LocalSpaceLocation + FVector(Positions[Element.X - Offset]); // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
			const FVector Pos1 = LocalSpaceLocation + FVector(Positions[Element.Y - Offset]);
			const FVector Pos2 = LocalSpaceLocation + FVector(Positions[Element.Z - Offset]);

			const bool bIsKinematic0 = (InvMasses[Element.X - Offset] == (Softs::FSolverReal)0.);
			const bool bIsKinematic1 = (InvMasses[Element.Y - Offset] == (Softs::FSolverReal)0.);
			const bool bIsKinematic2 = (InvMasses[Element.Z - Offset] == (Softs::FSolverReal)0.);

			DrawLine(PDI, Pos0, Pos1, bIsKinematic0 && bIsKinematic1 ? KinematicColor : DynamicColor);
			DrawLine(PDI, Pos1, Pos2, bIsKinematic1 && bIsKinematic2 ? KinematicColor : DynamicColor);
			DrawLine(PDI, Pos2, Pos0, bIsKinematic2 && bIsKinematic0 ? KinematicColor : DynamicColor);
		}
	}
}

void FClothingSimulation::DebugDrawAnimMeshWired(FPrimitiveDrawInterface* PDI) const
{
	check(Solver);

	static const FLinearColor KinematicColor = FColor::Purple;

	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const TConstArrayView<TVec3<int32>> Elements = Cloth->GetTriangleMesh(Solver.Get()).GetElements();
		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetAnimationPositions(Solver.Get());

		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
		{
			const auto& Element = Elements[ElementIndex];

			const FVector Pos0 = LocalSpaceLocation + FVector(Positions[Element.X - Offset]); // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
			const FVector Pos1 = LocalSpaceLocation + FVector(Positions[Element.Y - Offset]);
			const FVector Pos2 = LocalSpaceLocation + FVector(Positions[Element.Z - Offset]);

			DrawLine(PDI, Pos0, Pos1, KinematicColor);
			DrawLine(PDI, Pos1, Pos2, KinematicColor);
			DrawLine(PDI, Pos2, Pos0, KinematicColor);
		}
	}
}

void FClothingSimulation::DebugDrawAnimNormals(FPrimitiveDrawInterface* PDI) const
{
	check(Solver);

	static const FLinearColor KinematicColor = FColor::Magenta;
	constexpr FReal NormalLength = (FReal)20.;

	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetAnimationPositions(Solver.Get());
		const TConstArrayView<Softs::FSolverVec3> Normals = Cloth->GetAnimationNormals(Solver.Get());
		check(Normals.Num() == Positions.Num());

		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			const FVector Pos0 = LocalSpaceLocation + FVector(Positions[Index]);
			const FVector Pos1 = Pos0 + FVector(Normals[Index]) * NormalLength;

			DrawLine(PDI, Pos0, Pos1, KinematicColor);
		}
	}
}

void FClothingSimulation::DebugDrawPointNormals(FPrimitiveDrawInterface* PDI) const
{
	check(Solver);
	static const FLinearColor DynamicColor = FColor::White;
	static const FLinearColor KinematicColor = FColor::Purple;
	constexpr FReal NormalLength = (FReal)20.;

	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());
		const TConstArrayView<Softs::FSolverVec3> Normals = Cloth->GetParticleNormals(Solver.Get());
		const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver.Get());
		check(Normals.Num() == Positions.Num());
		check(InvMasses.Num() == Positions.Num());

		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			const bool bIsKinematic = (InvMasses[Index] == (Softs::FSolverReal)0.);
			const FVector Pos0 = LocalSpaceLocation + FVector(Positions[Index]);
			const FVector Pos1 = Pos0 + FVector(Normals[Index]) * NormalLength;

			DrawLine(PDI, Pos0, Pos1, bIsKinematic ? KinematicColor : DynamicColor);
		}
	}
}

void FClothingSimulation::DebugDrawPointVelocities(FPrimitiveDrawInterface* PDI) const
{
	check(Solver);
	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());
		const TConstArrayView<Softs::FSolverVec3> Velocities = Cloth->GetParticleVelocities(Solver.Get());
		check(Velocities.Num() == Positions.Num());

		const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver.Get());
		check(InvMasses.Num() == Positions.Num());

		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			constexpr FReal DefaultFPS = (FReal)60.;   // TODO: A CVAR would be nice for this
			const bool bIsKinematic = (InvMasses[Index] == 0.f);

			const FVec3 Pos0 = LocalSpaceLocation + Positions[Index];
			const FVec3 Pos1 = Pos0 + FVec3(Velocities[Index]) / DefaultFPS;  // Velocity per frame if running at DefaultFPS

			DrawLine(PDI, Pos0, Pos1, bIsKinematic ? FLinearColor::Black : FLinearColor::Yellow);
		}
	}
}

void FClothingSimulation::DebugDrawInversedPointNormals(FPrimitiveDrawInterface* PDI) const
{
	check(Solver);
	static const FLinearColor DynamicColor = FColor::White;
	static const FLinearColor KinematicColor = FColor::Purple;
	constexpr FReal NormalLength = (FReal)20.;

	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());
		const TConstArrayView<Softs::FSolverVec3> Normals = Cloth->GetParticleNormals(Solver.Get());
		const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver.Get());
		check(Normals.Num() == Positions.Num());
		check(InvMasses.Num() == Positions.Num());

		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			const bool bIsKinematic = (InvMasses[Index] == (Softs::FSolverReal)0.);
			const FVector Pos0 = LocalSpaceLocation + FVector(Positions[Index]);
			const FVector Pos1 = Pos0 - FVector(Normals[Index]) * NormalLength;

			DrawLine(PDI, Pos0, Pos1, bIsKinematic ? KinematicColor : DynamicColor);
		}
	}
}

void FClothingSimulation::DebugDrawCollision(FPrimitiveDrawInterface* PDI) const
{
	check(Solver);
	auto DrawCollision =
		[this, PDI](const FClothingSimulationCollider* Collider, const FClothingSimulationCloth* Cloth, FClothingSimulationCollider::ECollisionDataType CollisionDataType)
		{
			static const FLinearColor GlobalColor(FColor::Cyan);
			static const FLinearColor DynamicColor(FColor::Orange);
			static const FLinearColor LODsColor(FColor::Silver);
			static const FLinearColor CollidedColor(FColor::Red);

			const FLinearColor TypeColor =
				(CollisionDataType == FClothingSimulationCollider::ECollisionDataType::LODless) ? GlobalColor :
				(CollisionDataType == FClothingSimulationCollider::ECollisionDataType::External) ? DynamicColor : LODsColor;

			const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

			const TConstArrayView<TUniquePtr<FImplicitObject>> CollisionGeometries = Collider->GetCollisionGeometries(Solver.Get(), Cloth, CollisionDataType);
			const TConstArrayView<Softs::FSolverVec3> Translations = Collider->GetCollisionTranslations(Solver.Get(), Cloth, CollisionDataType);
			const TConstArrayView<Softs::FSolverRotation3> Rotations = Collider->GetCollisionRotations(Solver.Get(), Cloth, CollisionDataType);
			const TConstArrayView<bool> CollisionStatus = Collider->GetCollisionStatus(Solver.Get(), Cloth, CollisionDataType);
			check(CollisionGeometries.Num() == Translations.Num());
			check(CollisionGeometries.Num() == Rotations.Num());

			for (int32 Index = 0; Index < CollisionGeometries.Num(); ++Index)
			{
				if (const FImplicitObject* const Object = CollisionGeometries[Index].Get())
				{
					const FLinearColor Color = CollisionStatus[Index] ? CollidedColor : TypeColor;
					const FVec3 Position = LocalSpaceLocation + FVec3(Translations[Index]);
					const FRotation3 Rotation(Rotations[Index]);

					switch (Object->GetType())
					{
					case ImplicitObjectType::Sphere:
						DrawSphere(PDI, Object->GetObjectChecked<TSphere<FReal, 3>>(), Rotation, Position, Color);
						break;

					case ImplicitObjectType::Box:
						DrawBox(PDI, Object->GetObjectChecked<TBox<FReal, 3>>().BoundingBox(), Rotation, Position, Color);
						break;

					case ImplicitObjectType::Capsule:
						DrawCapsule(PDI, Object->GetObjectChecked<FCapsule>(), Rotation, Position, Color);
						break;

					case ImplicitObjectType::Union:  // Union only used as old style tapered capsules
						for (const TUniquePtr<FImplicitObject>& SubObjectPtr : Object->GetObjectChecked<FImplicitObjectUnion>().GetObjects())
						{
							if (const FImplicitObject* const SubObject = SubObjectPtr.Get())
							{
								switch (SubObject->GetType())
								{
								case ImplicitObjectType::Sphere:
									DrawSphere(PDI, SubObject->GetObjectChecked<TSphere<FReal, 3>>(), Rotation, Position, Color);
									break;

								case ImplicitObjectType::TaperedCylinder:
									DrawTaperedCylinder(PDI, SubObject->GetObjectChecked<FTaperedCylinder>(), Rotation, Position, Color);
									break;

								default:
									break;
								}
							}
						}
						break;

					case ImplicitObjectType::TaperedCapsule:  // New collision tapered capsules implicit type that replaces the union
						{
							const FTaperedCapsule& TaperedCapsule = Object->GetObjectChecked<FTaperedCapsule>();
							const FVec3 X1 = TaperedCapsule.GetX1();
							const FVec3 X2 = TaperedCapsule.GetX2();
							const FReal Radius1 = TaperedCapsule.GetRadius1();
							const FReal Radius2 = TaperedCapsule.GetRadius2();
							DrawSphere(PDI, TSphere<FReal, 3>(X1, Radius1), Rotation, Position, Color);
							DrawSphere(PDI, TSphere<FReal, 3>(X2, Radius2), Rotation, Position, Color);
							DrawTaperedCylinder(PDI, FTaperedCylinder(X1, X2, Radius1, Radius2), Rotation, Position, Color);
						}
						break;

					case ImplicitObjectType::Convex:
						DrawConvex(PDI, Object->GetObjectChecked<FConvex>(), Rotation, Position, Color);
						break;

					default:
						DrawCoordinateSystem(PDI, Rotation, Position);  // Draw everything else as a coordinate for now
						break;
					}
				}
			}
		};

	// Draw collisions
	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		for (const FClothingSimulationCollider* const Collider : Cloth->GetColliders())
		{
			DrawCollision(Collider, Cloth, FClothingSimulationCollider::ECollisionDataType::LODless);
			DrawCollision(Collider, Cloth, FClothingSimulationCollider::ECollisionDataType::External);
			DrawCollision(Collider, Cloth, FClothingSimulationCollider::ECollisionDataType::LODs);
		}
	}

	// Draw contacts
	check(Solver->GetCollisionContacts().Num() == Solver->GetCollisionNormals().Num());
	constexpr FReal NormalLength = (FReal)10.;

	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();
	for (int32 i = 0; i < Solver->GetCollisionContacts().Num(); ++i)
	{
		const FVec3 Pos0 = LocalSpaceLocation + FVec3(Solver->GetCollisionContacts()[i]);
		const FVec3 Normal = FVec3(Solver->GetCollisionNormals()[i]);

		// Draw contact
		FVec3 TangentU, TangentV;
		Normal.FindBestAxisVectors(TangentU, TangentV);

		DrawLine(PDI, Pos0 + TangentU, Pos0 + TangentV, FLinearColor::Black);
		DrawLine(PDI, Pos0 + TangentU, Pos0 - TangentV, FLinearColor::Black);
		DrawLine(PDI, Pos0 - TangentU, Pos0 - TangentV, FLinearColor::Black);
		DrawLine(PDI, Pos0 - TangentU, Pos0 + TangentV, FLinearColor::Black);

		// Draw normal
		static const FLinearColor Brown(0.1f, 0.05f, 0.f);
		const FVec3 Pos1 = Pos0 + NormalLength * Normal;
		DrawLine(PDI, Pos0, Pos1, Brown);
	}
}

void FClothingSimulation::DebugDrawBackstops(FPrimitiveDrawInterface* PDI) const
{
	auto DrawBackstop = [PDI](const FVector& Position, const FVector& Normal, FReal Radius, const FVector& Axis, const FLinearColor& Color)
	{
		static const FReal MaxCosAngle = (FReal)0.99;
		if (FMath::Abs(FVector::DotProduct(Normal, Axis)) < MaxCosAngle)
		{
			static const FReal ArcLength = (FReal)5.; // Arch length in cm
			const FReal ArcAngle = (FReal)360. * ArcLength / FMath::Max((Radius * (FReal)2. * (FReal)PI), ArcLength);
			DrawArc(PDI, Position, Normal, FVector::CrossProduct(Axis, Normal).GetSafeNormal(), -ArcAngle / (FReal)2., ArcAngle / (FReal)2., Radius, Color);
		}
	};

	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	uint8 ColorSeed = 0;

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(Offset);
		if (const Softs::FPBDSphericalBackstopConstraint* const BackstopConstraint = ClothConstraints.GetBackstopConstraints().Get())
		{
			const bool bUseLegacyBackstop = BackstopConstraint->UseLegacyBackstop();
			const TConstArrayView<FRealSingle>& BackstopDistances = Cloth->GetWeightMaps(Solver.Get())[(int32)EChaosWeightMapTarget::BackstopDistance];
			const TConstArrayView<FRealSingle>& BackstopRadiuses = Cloth->GetWeightMaps(Solver.Get())[(int32)EChaosWeightMapTarget::BackstopRadius];
			const TConstArrayView<Softs::FSolverVec3> AnimationPositions = Cloth->GetAnimationPositions(Solver.Get());
			const TConstArrayView<Softs::FSolverVec3> AnimationNormals = Cloth->GetAnimationNormals(Solver.Get());
			const TConstArrayView<Softs::FSolverVec3> ParticlePositions = Cloth->GetParticlePositions(Solver.Get());

			for (int32 Index = 0; Index < AnimationPositions.Num(); ++Index)
			{
				ColorSeed += 157;  // Prime number that gives a good spread of colors without getting too similar as a rand might do.
				const FLinearColor ColorLight = FLinearColor::MakeFromHSV8(ColorSeed, 160, 128);
				const FLinearColor ColorDark = FLinearColor::MakeFromHSV8(ColorSeed, 160, 64);

				const FReal BackstopRadius = BackstopRadiuses[Index] * BackstopConstraint->GetSphereRadiiMultiplier();
				const FReal BackstopDistance = BackstopDistances[Index];

				const FVector AnimationNormal(AnimationNormals[Index]);

				// Draw a line to show the current distance to the sphere
				const FVector Pos0 = LocalSpaceLocation + FVector(AnimationPositions[Index]);
				const FVector Pos1 = Pos0 - (bUseLegacyBackstop ? BackstopDistance - BackstopRadius : BackstopDistance) * AnimationNormal;
				const FVector Pos2 = LocalSpaceLocation + FVector(ParticlePositions[Index]);
				DrawLine(PDI, Pos1, Pos2, ColorLight);

				// Draw the sphere
				if (BackstopRadius > 0.f)
				{
					const FVector Center = Pos0 - (bUseLegacyBackstop ? BackstopDistance : BackstopRadius + BackstopDistance) * AnimationNormal;
					DrawBackstop(Center, AnimationNormal, BackstopRadius, FVector::ForwardVector, ColorDark);
					DrawBackstop(Center, AnimationNormal, BackstopRadius, FVector::UpVector, ColorDark);
					DrawBackstop(Center, AnimationNormal, BackstopRadius, FVector::RightVector, ColorDark);
				}
			}
		}
	}
}

void FClothingSimulation::DebugDrawBackstopDistances(FPrimitiveDrawInterface* PDI) const
{
	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	uint8 ColorSeed = 0;

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(Offset);
		if (const Softs::FPBDSphericalBackstopConstraint* const BackstopConstraint = ClothConstraints.GetBackstopConstraints().Get())
		{
			const bool bUseLegacyBackstop = BackstopConstraint->UseLegacyBackstop();
			const TConstArrayView<FRealSingle>& BackstopDistances = Cloth->GetWeightMaps(Solver.Get())[(int32)EChaosWeightMapTarget::BackstopDistance];
			const TConstArrayView<FRealSingle>& BackstopRadiuses = Cloth->GetWeightMaps(Solver.Get())[(int32)EChaosWeightMapTarget::BackstopRadius];
			const TConstArrayView<Softs::FSolverVec3> AnimationPositions = Cloth->GetAnimationPositions(Solver.Get());
			const TConstArrayView<Softs::FSolverVec3> AnimationNormals = Cloth->GetAnimationNormals(Solver.Get());

			for (int32 Index = 0; Index < AnimationPositions.Num(); ++Index)
			{
				ColorSeed += 157;  // Prime number that gives a good spread of colors without getting too similar as a rand might do.
				const FLinearColor ColorLight = FLinearColor::MakeFromHSV8(ColorSeed, 160, 128);
				const FLinearColor ColorDark = FLinearColor::MakeFromHSV8(ColorSeed, 160, 64);

				const FReal BackstopRadius = BackstopRadiuses[Index] * BackstopConstraint->GetSphereRadiiMultiplier();
				const FReal BackstopDistance = BackstopDistances[Index];

				const FVector AnimationNormal(AnimationNormals[Index]);

				// Draw a line to the sphere boundary
				const FVector Pos0 = LocalSpaceLocation + FVector(AnimationPositions[Index]);
				const FVector Pos1 = Pos0 - (bUseLegacyBackstop ? BackstopDistance - BackstopRadius : BackstopDistance) * AnimationNormal;
				DrawLine(PDI, Pos0, Pos1, ColorDark);
			}
		}
	}
}

void FClothingSimulation::DebugDrawMaxDistances(FPrimitiveDrawInterface* PDI) const
{
	check(Solver);
	
	// Draw max distances
	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const TConstArrayView<FRealSingle>& MaxDistances = Cloth->GetWeightMaps(Solver.Get())[(int32)EChaosWeightMapTarget::MaxDistance];
		if (!MaxDistances.Num())
		{
			continue;
		}

		const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver.Get());
		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetAnimationPositions(Solver.Get());
		const TConstArrayView<Softs::FSolverVec3> Normals = Cloth->GetAnimationNormals(Solver.Get());
		check(Normals.Num() == Positions.Num());
		check(MaxDistances.Num() == Positions.Num());
		check(InvMasses.Num() == Positions.Num());

		for (int32 Index = 0; Index < MaxDistances.Num(); ++Index)
		{
			const FReal MaxDistance = (FReal)MaxDistances[Index];
			const FVector Position = LocalSpaceLocation + FVector(Positions[Index]);
			if (InvMasses[Index] == (Softs::FSolverReal)0.)
			{
#if WITH_EDITOR
				DrawPoint(PDI, Position, FLinearColor::Red, DebugClothMaterialVertex);
#else
				DrawPoint(nullptr, Position, FLinearColor::Red, nullptr);
#endif
			}
			else
			{
				DrawLine(PDI, Position, Position + FVector(Normals[Index]) * MaxDistance, FLinearColor::White);
			}
		}
	}
}

void FClothingSimulation::DebugDrawAnimDrive(FPrimitiveDrawInterface* PDI) const
{
	check(Solver);

	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(Offset);
		if (const Softs::FPBDAnimDriveConstraint* const AnimDriveConstraint = ClothConstraints.GetAnimDriveConstraints().Get())
		{
			const TConstArrayView<FRealSingle>& AnimDriveStiffnessMultipliers = Cloth->GetWeightMaps(Solver.Get())[(int32)EChaosWeightMapTarget::AnimDriveStiffness];
			const TConstArrayView<Softs::FSolverVec3> AnimationPositions = Cloth->GetAnimationPositions(Solver.Get());
			const TConstArrayView<Softs::FSolverVec3> ParticlePositions = Cloth->GetParticlePositions(Solver.Get());
			check(ParticlePositions.Num() == AnimationPositions.Num());

			const FVec2 AnimDriveStiffness = AnimDriveConstraint->GetStiffness();
			const FRealSingle StiffnessOffset = AnimDriveStiffness[0];
			const FRealSingle StiffnessRange = AnimDriveStiffness[1] - AnimDriveStiffness[0];

			for (int32 Index = 0; Index < ParticlePositions.Num(); ++Index)
			{
				const FRealSingle Stiffness = AnimDriveStiffnessMultipliers.IsValidIndex(Index) ?
					StiffnessOffset + AnimDriveStiffnessMultipliers[Index] * StiffnessRange :
					StiffnessOffset;

				const FVector AnimationPosition = LocalSpaceLocation + FVector(AnimationPositions[Index]);
				const FVector ParticlePosition = LocalSpaceLocation + FVector(ParticlePositions[Index]);
				DrawLine(PDI, AnimationPosition, ParticlePosition, FLinearColor(FColor::Cyan) * Stiffness);
			}
		}
	}
}

static void DebugDrawSpringConstraintColors(FPrimitiveDrawInterface* PDI, const TConstArrayView<Softs::FSolverVec3>& Positions, const FVec3& LocalSpaceLocation, const Softs::FPBDSpringConstraints* const SpringConstraints)
{
	check(SpringConstraints);

	const TArray<TVec2<int32>>& Constraints = SpringConstraints->GetConstraints();
	const TArray<int32>& ConstraintsPerColorStartIndex = SpringConstraints->GetConstraintsPerColorStartIndex();
	if (ConstraintsPerColorStartIndex.Num() > 1)
	{
		const int32 ConstraintColorNum = ConstraintsPerColorStartIndex.Num() - 1;
		const uint8 HueOffset = 196 / ConstraintColorNum;
		auto ConstraintColor =
			[HueOffset](int32 ColorIndex)->FLinearColor
		{
			return FLinearColor::MakeFromHSV8(ColorIndex * HueOffset, 255, 255);
		};

		for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
		{
			const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
			const int32 ColorEnd = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1];
			const FLinearColor DrawColor = ConstraintColor(ConstraintColorIndex);
			for (int32 ConstraintIndex = ColorStart; ConstraintIndex < ColorEnd; ++ConstraintIndex)
			{
				// Draw line
				const TVec2<int32>& Constraint = Constraints[ConstraintIndex];
				const FVec3 Pos0 = FVec3(Positions[Constraint[0]]) + LocalSpaceLocation;
				const FVec3 Pos1 = FVec3(Positions[Constraint[1]]) + LocalSpaceLocation;
				DrawLine(PDI, Pos0, Pos1, DrawColor);
			}
		}
	}
	else
	{
		for (const TVec2<int32>& Constraint : Constraints)
		{
			// Draw line
			const FVec3 Pos0 = FVec3(Positions[Constraint[0]]) + LocalSpaceLocation;
			const FVec3 Pos1 = FVec3(Positions[Constraint[1]]) + LocalSpaceLocation;

			DrawLine(PDI, Pos0, Pos1, FLinearColor::Black);
		}
	}
}

void FClothingSimulation::DebugDrawEdgeConstraint(FPrimitiveDrawInterface* PDI) const
{
	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		// Draw constraints
		const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(Offset);

		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());

		if (const Softs::FPBDSpringConstraints* const EdgeConstraints = ClothConstraints.GetEdgeConstraints().Get())
		{
			DebugDrawSpringConstraintColors(PDI, Positions, LocalSpaceLocation, EdgeConstraints);
		}
	}
}

void FClothingSimulation::DebugDrawBendingConstraint(FPrimitiveDrawInterface* PDI) const
{
	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		// Draw constraints
		const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(Offset);

		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());

		if (const Softs::FPBDSpringConstraints* const BendingConstraints = ClothConstraints.GetBendingConstraints().Get())
		{
			DebugDrawSpringConstraintColors(PDI, Positions, LocalSpaceLocation, BendingConstraints);
		}

		if (const Softs::FPBDBendingConstraints* const BendingConstraints = ClothConstraints.GetBendingElementConstraints().Get())
		{
			const TArray<TVec4<int32>>& Constraints = BendingConstraints->GetConstraints();
			const TArray<bool>& IsBuckled = BendingConstraints->GetIsBuckled();

			// Color constraint edge with red or blue: Red = Buckled, Blue = Not Buckled. 
			for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
			{
				const Softs::FSolverVec3& P1 = Positions[Constraints[ConstraintIndex][0]];
				const Softs::FSolverVec3& P2 = Positions[Constraints[ConstraintIndex][1]];
				
				const bool bIsBuckled = IsBuckled[ConstraintIndex];

				const FVec3 Pos0 = FVec3(P1) + LocalSpaceLocation;
				const FVec3 Pos1 = FVec3(P2) + LocalSpaceLocation;
				DrawLine(PDI, Pos0, Pos1, bIsBuckled ? FLinearColor::Red : FLinearColor::Blue);
			}
		}

		if (const Softs::FXPBDBendingConstraints* const BendingConstraints = ClothConstraints.GetXBendingElementConstraints().Get())
		{
			const TArray<TVec4<int32>>& Constraints = BendingConstraints->GetConstraints();
			const TArray<bool>& IsBuckled = BendingConstraints->GetIsBuckled();

			// Color constraint edge with red or blue: Red = Buckled, Blue = Not Buckled.
			for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
			{
				const Softs::FSolverVec3& P1 = Positions[Constraints[ConstraintIndex][0]];
				const Softs::FSolverVec3& P2 = Positions[Constraints[ConstraintIndex][1]];

				const bool bIsBuckled = IsBuckled[ConstraintIndex];

				const FVec3 Pos0 = FVec3(P1) + LocalSpaceLocation;
				const FVec3 Pos1 = FVec3(P2) + LocalSpaceLocation;
				DrawLine(PDI, Pos0, Pos1, bIsBuckled ? FLinearColor::Red : FLinearColor::Blue);
			}
		}
	}
}

void FClothingSimulation::DebugDrawLongRangeConstraint(FPrimitiveDrawInterface* PDI) const
{
	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	auto PseudoRandomColor =
		[](int32 NumColorRotations) -> FLinearColor
		{
			static const uint8 Spread = 157;  // Prime number that gives a good spread of colors without getting too similar as a rand might do.
			uint8 Seed = Spread;
			for (int32 i = 0; i < NumColorRotations; ++i)
			{
				Seed += Spread;
			}
			return FLinearColor::MakeFromHSV8(Seed, 160, 128);
		};

	auto Darken =
		[](const FLinearColor& Color) -> FLinearColor
		{
			FLinearColor ColorHSV = Color.LinearRGBToHSV();
			ColorHSV.B *= .5f;
			return ColorHSV.HSVToLinearRGB();
		};

	int32 ColorOffset = 0;

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		// Draw constraints
		const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(Offset);
		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());

		if (const Softs::FPBDLongRangeConstraints* const LongRangeConstraints = ClothConstraints.GetLongRangeConstraints().Get())
		{
			const TArray<TConstArrayView<Softs::FPBDLongRangeConstraints::FTether>>& Tethers = LongRangeConstraints->GetTethers();

			for (int32 BatchIndex = 0; BatchIndex < Tethers.Num(); ++BatchIndex)
			{
				const FLinearColor Color = PseudoRandomColor(ColorOffset + BatchIndex);
				const FLinearColor DarkenedColor = Darken(Color);

				const TConstArrayView<Softs::FPBDLongRangeConstraints::FTether>& TetherBatch = Tethers[BatchIndex];

				// Draw tethers
				for (const Softs::FPBDLongRangeConstraints::FTether& Tether : TetherBatch)
				{
					const int32 KinematicIndex = LongRangeConstraints->GetStartIndex(Tether);
					const int32 DynamicIndex = LongRangeConstraints->GetEndIndex(Tether);
					const FReal TargetLength = LongRangeConstraints->GetTargetLength(Tether);

					const FVec3 Pos0 = FVec3(Positions[KinematicIndex]) + LocalSpaceLocation;
					const FVec3 Pos1 = FVec3(Positions[DynamicIndex]) + LocalSpaceLocation;

					DrawLine(PDI, Pos0, Pos1, Color);
#if WITH_EDITOR
					DrawPoint(PDI, Pos1, Color, DebugClothMaterialVertex);
#else
					DrawPoint(nullptr, Pos1, Color, nullptr);
#endif
					FVec3 Direction = Pos1 - Pos0;
					const float Length = Direction.SafeNormalize();
					if (Length > SMALL_NUMBER)
					{
						const FVec3 Pos2 = Pos1 + Direction * (TargetLength - Length);
						DrawLine(PDI, Pos1, Pos2, DarkenedColor);
					}
				}
			}

			// Rotate the colors for each cloth
			ColorOffset += Tethers.Num();
		}
	}
}

void FClothingSimulation::DebugDrawWindAndPressureForces(FPrimitiveDrawInterface* PDI) const
{
	check(Solver);

	constexpr FReal ForceLength = (FReal)10.;
	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const Softs::FVelocityAndPressureField& VelocityField = Solver->GetWindVelocityAndPressureField(Cloth->GetGroupId());

		const TConstArrayView<TVec3<int32>>& Elements = VelocityField.GetElements();
		const TConstArrayView<Softs::FSolverVec3> Forces = VelocityField.GetForces();
		const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());
		const TConstArrayView<Softs::FSolverReal> InvMasses = Cloth->GetParticleInvMasses(Solver.Get());
		check(InvMasses.Num() == Positions.Num());

		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
		{
			const TVec3<int32>& Element = Elements[ElementIndex];
			const FVec3 Position = LocalSpaceLocation + (
				FVec3(Positions[Element.X - Offset]) +
				FVec3(Positions[Element.Y - Offset]) +
				FVec3(Positions[Element.Z - Offset])) / (FReal)3.;

			const bool bIsKinematic0 = !InvMasses[Element.X - Offset];
			const bool bIsKinematic1 = !InvMasses[Element.Y - Offset];
			const bool bIsKinematic2 = !InvMasses[Element.Z - Offset];
			const bool bIsKinematic = bIsKinematic0 || bIsKinematic1 || bIsKinematic2;

			const FVec3 Force = FVec3(Forces[ElementIndex]) * ForceLength;
			DrawLine(PDI, Position, Position + Force, bIsKinematic ? FColor::Cyan : FColor::Green);
		}
	}
}

void FClothingSimulation::DebugDrawLocalSpace(FPrimitiveDrawInterface* PDI) const
{
	check(Solver);

	// Draw local space
	DrawCoordinateSystem(PDI, FQuat::Identity, Solver->GetLocalSpaceLocation());

	// Draw reference spaces
	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		if (Cloth->GetOffset(Solver.Get()) == INDEX_NONE)
		{
			continue;
		}
		const FRigidTransform3& ReferenceSpaceTransform = Cloth->GetReferenceSpaceTransform();
		DrawCoordinateSystem(PDI, ReferenceSpaceTransform.GetRotation(), ReferenceSpaceTransform.GetLocation());
	}
}

void FClothingSimulation::DebugDrawSelfCollision(FPrimitiveDrawInterface* PDI) const
{
	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		// Draw constraints
		const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(Offset);

		if (const Softs::FPBDCollisionSpringConstraints* const SelfCollisionConstraints = ClothConstraints.GetSelfCollisionConstraints().Get())
		{
			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());
			const TArray<TVec4<int32>>& Constraints = SelfCollisionConstraints->GetConstraints();
			const TArray<Softs::FSolverVec3>& Barys = SelfCollisionConstraints->GetBarys();
			const FReal Thickness = (FReal)SelfCollisionConstraints->GetThickness();
			const FReal Height = Thickness + Thickness;
			const TArray<bool>& FlipNormals = SelfCollisionConstraints->GetFlipNormals();

			for (int32 Index = 0; Index < Constraints.Num(); ++Index)
			{
				const TVec4<int32>& Constraint = Constraints[Index];
				const FVec3 Bary(Barys[Index]);

				// Constraint index includes Offset, but so does Positions.
				const FVector P = LocalSpaceLocation + FVector(Positions[Constraint[0] - Offset]);
				const FVector P0 = LocalSpaceLocation + FVector(Positions[Constraint[1] - Offset]);
				const FVector P1 = LocalSpaceLocation + FVector(Positions[Constraint[2] - Offset]);
				const FVector P2 = LocalSpaceLocation + FVector(Positions[Constraint[3] - Offset]);

				const FVector Pos0 = P0 * Bary[0] + P1 * Bary[1] + P2 * Bary[2];

				static const FLinearColor Brown(0.1f, 0.05f, 0.f);
				static const FLinearColor Red(0.3f, 0.f, 0.f);
				const FTriangle Triangle(P0, P1, P2);
				const FVector Normal = FlipNormals[Index] ? -Triangle.GetNormal() : Triangle.GetNormal();

				// Draw point to surface line (=normal)
				const FVector Pos1 = Pos0 + Height * Normal;
				DrawPoint(PDI, Pos0, Brown, nullptr, 2.f);
				DrawLine(PDI, Pos0, Pos1, FlipNormals[Index] ? Red : Brown);

				// Draw pushup to point
				static const FLinearColor Orange(0.3f, 0.15f, 0.f);
				DrawPoint(PDI, P, Orange, nullptr, 2.f);
				DrawLine(PDI, Pos1, P, Orange);
			}
		}
	}
}

void FClothingSimulation::DebugDrawSelfIntersection(FPrimitiveDrawInterface* PDI) const
{
	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();

	for (const FClothingSimulationCloth* const Cloth : Solver->GetCloths())
	{
		const int32 Offset = Cloth->GetOffset(Solver.Get());
		if (Offset == INDEX_NONE)
		{
			continue;
		}

		const FClothConstraints& ClothConstraints = Solver->GetClothConstraints(Offset);

		static const FLinearColor Red(1.f, 0.f, 0.f);
		static const FLinearColor White(1.f, 1.f, 1.f);
		static const FLinearColor Black(0.f, 0.f, 0.f);
		static const FLinearColor Teal(0.f, 0.5f, 0.5f);
		static const FLinearColor Green(0.f, 1.f, 0.f);

		if (const Softs::FPBDTriangleMeshCollisions* const SelfCollisionInit = ClothConstraints.GetSelfCollisionInit().Get())
		{
			const TConstArrayView<Softs::FSolverVec3> Positions = Cloth->GetParticlePositions(Solver.Get());
			const FTriangleMesh& TriangleMesh = Cloth->GetTriangleMesh(Solver.Get());

			// Draw contours
			const TArray<TArray<Softs::FPBDTriangleMeshCollisions::FBarycentricPoint>>& ContourPoints = SelfCollisionInit->GetIntersectionContourPoints();
			const TArray<Softs::FPBDTriangleMeshCollisions::FContourType>& ContourTypes = SelfCollisionInit->GetIntersectionContourTypes();
			check(ContourPoints.Num() == ContourTypes.Num());

			static const FLinearColor ColorsForType[(int8)Softs::FPBDTriangleMeshCollisions::FContourType::Count] =
			{
				Teal,
				Red,
				White,
				Black
			};
			for( int32 ContourIndex = 0; ContourIndex < ContourPoints.Num(); ++ContourIndex)
			{
				const TArray<Softs::FPBDTriangleMeshCollisions::FBarycentricPoint>& Contour = ContourPoints[ContourIndex];
				const FLinearColor& ContourColor = ColorsForType[(int8)ContourTypes[ContourIndex]];
				for (int32 PointIdx = 0; PointIdx < Contour.Num() - 1; ++PointIdx)
				{
					const Softs::FPBDTriangleMeshCollisions::FBarycentricPoint& Point0 = Contour[PointIdx];
					const FVector EndPoint0 = LocalSpaceLocation + (1.f - Point0.Bary[0] - Point0.Bary[1]) * Positions[Point0.Vertices[0] - Offset] + Point0.Bary[0] * Positions[Point0.Vertices[1] - Offset] + Point0.Bary[1] * Positions[Point0.Vertices[2] - Offset];
					const Softs::FPBDTriangleMeshCollisions::FBarycentricPoint& Point1 = Contour[PointIdx+1];
					const FVector EndPoint1 = LocalSpaceLocation + (1.f - Point1.Bary[0] - Point1.Bary[1]) * Positions[Point1.Vertices[0] - Offset] + Point1.Bary[0] * Positions[Point1.Vertices[1] - Offset] + Point1.Bary[1] * Positions[Point1.Vertices[2] - Offset];
					DrawLine(PDI, EndPoint0, EndPoint1, ContourColor);
					DrawPoint(PDI, EndPoint0, ContourColor, nullptr, 1.f);
					DrawPoint(PDI, EndPoint1, ContourColor, nullptr, 1.f);
				}
			}

			// Draw GIA colors
			const TConstArrayView<Softs::FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors = SelfCollisionInit->GetVertexGIAColors();
			static const FLinearColor Gray(0.5f, 0.5f, 0.5f);
			if (VertexGIAColors.Num())
			{
				const TArray<TVec3<int32>>& Elements = TriangleMesh.GetElements();
				for (int32 ParticleIdx = Offset; ParticleIdx < VertexGIAColors.Num(); ++ParticleIdx)
				{
					if (VertexGIAColors[ParticleIdx].ContourIndexBits)
					{
						const bool bIsLoop = VertexGIAColors[ParticleIdx].IsLoop();
						const bool bAnyWhite = (VertexGIAColors[ParticleIdx].ContourIndexBits & ~VertexGIAColors[ParticleIdx].ColorBits);
						const bool bAnyBlack = (VertexGIAColors[ParticleIdx].ContourIndexBits & VertexGIAColors[ParticleIdx].ColorBits);
						const FLinearColor& VertColor = bIsLoop ? Red : (bAnyWhite && bAnyBlack) ? Gray : bAnyWhite ? White : Black;
						
						DrawPoint(PDI, LocalSpaceLocation + Positions[ParticleIdx - Offset], VertColor, nullptr, 5.f);
					}
				}
			}
			const TArray<Softs::FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors = SelfCollisionInit->GetTriangleGIAColors();
			if (TriangleGIAColors.Num() == TriangleMesh.GetNumElements())
			{
				const TArray<TVec3<int32>>& Elements = TriangleMesh.GetElements();
				for (int32 TriangleIdx = 0; TriangleIdx < TriangleGIAColors.Num(); ++TriangleIdx)
				{
					if (TriangleGIAColors[TriangleIdx].ContourIndexBits)
					{
						const bool bIsLoop = TriangleGIAColors[TriangleIdx].IsLoop();
						const bool bAnyWhite = (TriangleGIAColors[TriangleIdx].ContourIndexBits & ~TriangleGIAColors[TriangleIdx].ColorBits);
						const bool bAnyBlack = (TriangleGIAColors[TriangleIdx].ContourIndexBits & TriangleGIAColors[TriangleIdx].ColorBits);
						const FLinearColor& TriColor = bIsLoop ? Red : (bAnyWhite && bAnyBlack) ? Gray : bAnyWhite ? White : Black;
						DrawLine(PDI, LocalSpaceLocation + Positions[Elements[TriangleIdx][0] - Offset], LocalSpaceLocation + Positions[Elements[TriangleIdx][1] - Offset], TriColor);
						DrawLine(PDI, LocalSpaceLocation + Positions[Elements[TriangleIdx][1] - Offset], LocalSpaceLocation + Positions[Elements[TriangleIdx][2] - Offset], TriColor);
						DrawLine(PDI, LocalSpaceLocation + Positions[Elements[TriangleIdx][0] - Offset], LocalSpaceLocation + Positions[Elements[TriangleIdx][2] - Offset], TriColor);
					}
				}
			}

			// Draw contour minimization gradients
			const TArray<Softs::FPBDTriangleMeshCollisions::FContourMinimizationIntersection>& ContourMinimizationIntersections = SelfCollisionInit->GetContourMinimizationIntersections();
			constexpr FReal MaxDrawImpulse = 1.;
			constexpr FReal RegularizeEpsilonSq = 1.;
			for (const Softs::FPBDTriangleMeshCollisions::FContourMinimizationIntersection& Intersection : ContourMinimizationIntersections)
			{
				Softs::FSolverReal GradientLength;
				Softs::FSolverVec3 GradientDir;
				Intersection.GlobalGradientVector.ToDirectionAndLength(GradientDir, GradientLength);
				const FVector Delta = FVector(GradientDir) * MaxDrawImpulse * GradientLength * FMath::InvSqrt(GradientLength * GradientLength + RegularizeEpsilonSq);

				const FVector EdgeCenter = LocalSpaceLocation + .5 * (Positions[Intersection.EdgeVertices[0] - Offset] + Positions[Intersection.EdgeVertices[1] - Offset]);
				const FVector TriCenter = LocalSpaceLocation + (Positions[Intersection.FaceVertices[0] - Offset] + Positions[Intersection.FaceVertices[1] - Offset] + Positions[Intersection.FaceVertices[2] - Offset]) / 3.;

				DrawPoint(PDI, EdgeCenter, Green, nullptr, 2.f);
				DrawLine(PDI, EdgeCenter, EdgeCenter + Delta, Green);
				DrawPoint(PDI, TriCenter, Green, nullptr, 2.f);
				DrawLine(PDI, TriCenter, TriCenter - Delta, Green);
			}
		}
	}
}

#endif  // #if WITH_EDITOR || CHAOS_DEBUG_DRAW?
}  // End namespace Chaos

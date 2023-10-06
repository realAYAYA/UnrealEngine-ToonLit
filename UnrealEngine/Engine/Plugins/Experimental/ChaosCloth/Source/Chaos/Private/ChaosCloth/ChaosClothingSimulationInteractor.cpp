// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosCloth/ChaosClothingSimulationInteractor.h"
#include "ChaosCloth/ChaosClothingSimulationCloth.h"
#include "ChaosCloth/ChaosClothingSimulation.h"
#include "ChaosCloth/ChaosClothingSimulationSolver.h"
#include "ChaosCloth/ChaosClothingSimulationConfig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosClothingSimulationInteractor)

namespace ChaosClothingInteractor
{
	static const float InvStiffnessLogBase = 1.f / FMath::Loge(1.e3f);  // Log base for updating old linear stiffnesses to the new stiffness exponentiation
}

void UChaosClothingInteractor::Sync(IClothingSimulation* Simulation)
{
	check(Simulation);

	if (Chaos::FClothingSimulationCloth* const Cloth = static_cast<Chaos::FClothingSimulation*>(Simulation)->GetCloth(ClothingId))
	{
		for (FChaosClothingInteractorCommand& Command : Commands)
		{
			Command.Execute(Cloth);
		}
		Commands.Reset();
		for (FChaosClothingInteractorConfigCommand& ConfigCommand : ConfigCommands)
		{
			check(Cloth->GetConfig()->IsLegacySingleLOD());
			ConfigCommand.Execute(Cloth->GetConfig(), 0);
		}
		ConfigCommands.Reset();
	}

	// Call to base class' sync
	UClothingInteractor::Sync(Simulation);
}

void UChaosClothingInteractor::SetMaterialLinear(float EdgeStiffnessLinear, float BendingStiffnessLinear, float AreaStiffnessLinear)
{
	const FVector2f EdgeStiffness((FMath::Clamp(FMath::Loge(EdgeStiffnessLinear) * ChaosClothingInteractor::InvStiffnessLogBase + 1.f, 0.f, 1.f)), 1.f);
	const FVector2f BendingStiffness((FMath::Clamp(FMath::Loge(BendingStiffnessLinear) * ChaosClothingInteractor::InvStiffnessLogBase + 1.f, 0.f, 1.f)), 1.f);
	const FVector2f AreaStiffness((FMath::Clamp(FMath::Loge(AreaStiffnessLinear) * ChaosClothingInteractor::InvStiffnessLogBase + 1.f, 0.f, 1.f)), 1.f);

	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([EdgeStiffness, BendingStiffness, AreaStiffness](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("EdgeSpringStiffness"), EdgeStiffness);
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("BendingSpringStiffness"), BendingStiffness);
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("AreaSpringStiffness"), AreaStiffness);
	}));
}

void UChaosClothingInteractor::SetMaterial(FVector2D EdgeStiffness, FVector2D BendingStiffness, FVector2D AreaStiffness)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([EdgeStiffness, BendingStiffness, AreaStiffness](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("EdgeSpringStiffness"), FVector2f(EdgeStiffness));
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("BendingSpringStiffness"), FVector2f(BendingStiffness));
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("AreaSpringStiffness"), FVector2f(AreaStiffness));
	}));
}

void UChaosClothingInteractor::SetLongRangeAttachmentLinear(float TetherStiffnessLinear, float TetherScale)
{
	// Deprecated
	const FVector2f TetherStiffness((FMath::Clamp(FMath::Loge(TetherStiffnessLinear) * ChaosClothingInteractor::InvStiffnessLogBase + 1.f, 0.f, 1.f)), 1.f);
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([TetherStiffness, TetherScale](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("TetherStiffness"), TetherStiffness);
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("TetherScale"), FVector2f(TetherScale));
	}));
}

void UChaosClothingInteractor::SetLongRangeAttachment(FVector2D TetherStiffness, FVector2D TetherScale)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([TetherStiffness, TetherScale](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("TetherStiffness"), FVector2f(TetherStiffness));
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("TetherScale"), FVector2f(TetherScale));
	}));
}

void UChaosClothingInteractor::SetCollision(float CollisionThickness, float FrictionCoefficient, bool bUseCCD, float SelfCollisionThickness)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([CollisionThickness, FrictionCoefficient, bUseCCD, SelfCollisionThickness](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetValue(TEXT("CollisionThickness"), CollisionThickness);
		Config->GetProperties(LODIndex).SetValue(TEXT("FrictionCoefficient"), FrictionCoefficient);
		Config->GetProperties(LODIndex).SetValue(TEXT("UseCCD"), bUseCCD);
		Config->GetProperties(LODIndex).SetValue(TEXT("SelfCollisionThickness"), SelfCollisionThickness);
	}));
}

void UChaosClothingInteractor::SetBackstop(bool bEnabled)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([bEnabled](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetEnabled(TEXT("BackstopRadius"), bEnabled);  // BackstopRadius controls whether the backstop is enabled or not
	}));
}
void UChaosClothingInteractor::SetDamping(float DampingCoefficient, float LocalDampingCoefficient)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([DampingCoefficient, LocalDampingCoefficient](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetValue(TEXT("DampingCoefficient"), DampingCoefficient);
		Config->GetProperties(LODIndex).SetValue(TEXT("LocalDampingCoefficient"), LocalDampingCoefficient);
	}));
}

void UChaosClothingInteractor::SetAerodynamics(float DragCoefficient, float LiftCoefficient, FVector WindVelocity)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([DragCoefficient, LiftCoefficient, WindVelocity](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		constexpr float AirDensity = 1.225f;
		constexpr float WorldScale = 100.f;  // Unreal's world unit is the cm
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("Drag"), FVector2f(DragCoefficient));
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("Lift"), FVector2f(LiftCoefficient));
		Config->GetProperties(LODIndex).SetValue(TEXT("FluidDensity"), AirDensity);
		Config->GetProperties(LODIndex).SetValue(TEXT("WindVelocity"), FVector3f(WindVelocity) / WorldScale);
	}));
}

void UChaosClothingInteractor::SetWind(FVector2D Drag, FVector2D Lift, float AirDensity, FVector WindVelocity)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([Drag, Lift, AirDensity, WindVelocity](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		constexpr float WorldScale = 100.f;  // Unreal's world unit is the cm
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("Drag"), FVector2f(Drag));
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("Lift"), FVector2f(Lift));
		Config->GetProperties(LODIndex).SetValue(TEXT("FluidDensity"), (float)AirDensity * FMath::Cube(WorldScale));  // AirDensity is here in kg/cm^3 for legacy reason but must be in kg/m^3 in the config UI
		Config->GetProperties(LODIndex).SetValue(TEXT("WindVelocity"), FVector3f(WindVelocity) / WorldScale);
	}));
}

void UChaosClothingInteractor::SetPressure(FVector2D Pressure)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([Pressure](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("Pressure"), FVector2f(Pressure));
	}));
}

void UChaosClothingInteractor::SetGravity(float GravityScale, bool bIsGravityOverridden, FVector GravityOverride)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([GravityScale, bIsGravityOverridden, GravityOverride](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetValue(TEXT("GravityScale"), GravityScale);
		Config->GetProperties(LODIndex).SetValue(TEXT("UseGravityOverride"), bIsGravityOverridden);
		Config->GetProperties(LODIndex).SetValue(TEXT("GravityOverride"), FVector3f(GravityOverride));
	}));
}

void UChaosClothingInteractor::SetAnimDriveLinear(float AnimDriveStiffnessLinear)
{
	// Deprecated
	const FVector2f AnimDriveStiffness(0.f, FMath::Clamp(FMath::Loge(AnimDriveStiffnessLinear) * ChaosClothingInteractor::InvStiffnessLogBase + 1.f, 0.f, 1.f));
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([AnimDriveStiffness](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		// The Anim Drive stiffness Low value needs to be 0 in order to keep backward compatibility with existing mask (this wouldn't be an issue if this property had no legacy mask)
		static const FVector2f AnimDriveDamping(0.f, 1.f);
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("AnimDriveStiffness"), AnimDriveStiffness);
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("AnimDriveDamping"), AnimDriveDamping);
	}));
}

void UChaosClothingInteractor::SetAnimDrive(FVector2D AnimDriveStiffness, FVector2D AnimDriveDamping)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([AnimDriveStiffness, AnimDriveDamping](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("AnimDriveStiffness"), FVector2f(AnimDriveStiffness));
		Config->GetProperties(LODIndex).SetWeightedFloatValue(TEXT("AnimDriveDamping"), FVector2f(AnimDriveDamping));
	}));
}

void UChaosClothingInteractor::SetVelocityScale(FVector LinearVelocityScale, float AngularVelocityScale, float FictitiousAngularScale)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([LinearVelocityScale, AngularVelocityScale, FictitiousAngularScale](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetValue(TEXT("LinearVelocityScale"), FVector3f(LinearVelocityScale));
		Config->GetProperties(LODIndex).SetValue(TEXT("AngularVelocityScale"), AngularVelocityScale);
		Config->GetProperties(LODIndex).SetValue(TEXT("FictitiousAngularScale"), FictitiousAngularScale);
	}));
}

void UChaosClothingInteractor::ResetAndTeleport(bool bReset, bool bTeleport)
{
	if (bReset)
	{
		Commands.Add(FChaosClothingInteractorCommand::CreateLambda([](Chaos::FClothingSimulationCloth* Cloth)
		{
			Cloth->Reset();
		}));
	}
	if (bTeleport)
	{
		Commands.Add(FChaosClothingInteractorCommand::CreateLambda([](Chaos::FClothingSimulationCloth* Cloth)
		{
			Cloth->Teleport();
		}));
	}
}

void UChaosClothingSimulationInteractor::Sync(IClothingSimulation* Simulation, IClothingSimulationContext* Context)
{
	check(Simulation);
	check(Context);

	for (FChaosClothingSimulationInteractorCommand& Command : Commands)
	{
		Command.Execute(static_cast<Chaos::FClothingSimulation*>(Simulation), static_cast<Chaos::FClothingSimulationContext*>(Context));
	}
	Commands.Reset();
	for (FChaosClothingInteractorConfigCommand& ConfigCommand : ConfigCommands)
	{
		ensure(static_cast<Chaos::FClothingSimulation*>(Simulation)->GetSolver()->GetConfig()->IsLegacySingleLOD());
		ConfigCommand.Execute(static_cast<Chaos::FClothingSimulation*>(Simulation)->GetSolver()->GetConfig(), 0);
	}
	ConfigCommands.Reset();

	// Call base class' sync 
	UClothingSimulationInteractor::Sync(Simulation, Context);
}

void UChaosClothingSimulationInteractor::PhysicsAssetUpdated()
{
	Commands.Add(FChaosClothingSimulationInteractorCommand::CreateLambda([](Chaos::FClothingSimulation* Simulation, Chaos::FClothingSimulationContext* /*Context*/)
	{
		Simulation->RefreshPhysicsAsset();
	}));
}

void UChaosClothingSimulationInteractor::ClothConfigUpdated()
{
	Commands.Add(FChaosClothingSimulationInteractorCommand::CreateLambda([](Chaos::FClothingSimulation* Simulation, Chaos::FClothingSimulationContext* Context)
	{
		Simulation->RefreshClothConfig(Context);
	}));
}

void UChaosClothingSimulationInteractor::SetAnimDriveSpringStiffness(float Stiffness)
{
	// Set the anim drive stiffness through the ChaosClothInteractor to allow the value to be overridden by the cloth interactor if needed
	for (const auto& ClothingInteractor : UClothingSimulationInteractor::ClothingInteractors)
	{
		if (UChaosClothingInteractor* const ChaosClothingInteractor = Cast<UChaosClothingInteractor>(ClothingInteractor.Value))
		{
			ChaosClothingInteractor->SetAnimDriveLinear(Stiffness);
		}
	}
}

void UChaosClothingSimulationInteractor::EnableGravityOverride(const FVector& Gravity)
{
	Commands.Add(FChaosClothingSimulationInteractorCommand::CreateLambda([Gravity](Chaos::FClothingSimulation* Simulation, Chaos::FClothingSimulationContext* /*Context*/)
	{
		Simulation->SetGravityOverride(Gravity);
	}));
}

void UChaosClothingSimulationInteractor::DisableGravityOverride()
{
	Commands.Add(FChaosClothingSimulationInteractorCommand::CreateLambda([](Chaos::FClothingSimulation* Simulation, Chaos::FClothingSimulationContext* /*Context*/)
	{
		Simulation->DisableGravityOverride();
	}));
}

void UChaosClothingSimulationInteractor::SetNumIterations(int32 NumIterations)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([NumIterations](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetValue(TEXT("NumIterations"), NumIterations);
	}));
}

void UChaosClothingSimulationInteractor::SetMaxNumIterations(int32 MaxNumIterations)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([MaxNumIterations](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetValue(TEXT("MaxNumIterations"), MaxNumIterations);
	}));
}

void UChaosClothingSimulationInteractor::SetNumSubsteps(int32 NumSubsteps)
{
	ConfigCommands.Add(FChaosClothingInteractorConfigCommand::CreateLambda([NumSubsteps](Chaos::FClothingSimulationConfig* Config, int32 LODIndex)
	{
		Config->GetProperties(LODIndex).SetValue(TEXT("NumSubsteps"), NumSubsteps);
	}));
}

UClothingInteractor* UChaosClothingSimulationInteractor::CreateClothingInteractor()
{
	return NewObject<UChaosClothingInteractor>(this);
}


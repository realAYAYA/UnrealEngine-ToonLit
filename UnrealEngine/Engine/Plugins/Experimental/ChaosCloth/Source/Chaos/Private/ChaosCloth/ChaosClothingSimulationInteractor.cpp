// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosCloth/ChaosClothingSimulationInteractor.h"
#include "ChaosCloth/ChaosClothingSimulationCloth.h"
#include "ChaosCloth/ChaosClothingSimulation.h"

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
	}

	// Call to base class' sync
	UClothingInteractor::Sync(Simulation);
}

void UChaosClothingInteractor::SetMaterialLinear(float EdgeStiffnessLinear, float BendingStiffnessLinear, float AreaStiffnessLinear)
{
	const Chaos::TVec2<Chaos::FRealSingle> EdgeStiffness((FMath::Clamp(FMath::Loge(EdgeStiffnessLinear) * ChaosClothingInteractor::InvStiffnessLogBase + 1.f, 0.f, 1.f)), 1.f);
	const Chaos::TVec2<Chaos::FRealSingle> BendingStiffness((FMath::Clamp(FMath::Loge(BendingStiffnessLinear) * ChaosClothingInteractor::InvStiffnessLogBase + 1.f, 0.f, 1.f)), 1.f);
	const Chaos::TVec2<Chaos::FRealSingle> AreaStiffness((FMath::Clamp(FMath::Loge(AreaStiffnessLinear) * ChaosClothingInteractor::InvStiffnessLogBase + 1.f, 0.f, 1.f)), 1.f);

	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([EdgeStiffness, BendingStiffness, AreaStiffness](Chaos::FClothingSimulationCloth* Cloth)
	{
		Cloth->SetMaterialProperties(EdgeStiffness, BendingStiffness, AreaStiffness);
	}));
}

void UChaosClothingInteractor::SetMaterial(FVector2D EdgeStiffness, FVector2D BendingStiffness, FVector2D AreaStiffness)
{
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([EdgeStiffness, BendingStiffness, AreaStiffness](Chaos::FClothingSimulationCloth* Cloth)
	{
		Cloth->SetMaterialProperties(Chaos::TVec2<Chaos::FRealSingle>(EdgeStiffness[0], EdgeStiffness[1]), Chaos::TVec2<Chaos::FRealSingle>(BendingStiffness[0], BendingStiffness[1]), Chaos::TVec2<Chaos::FRealSingle>(AreaStiffness[0], AreaStiffness[1]));
	}));
}

void UChaosClothingInteractor::SetLongRangeAttachmentLinear(float TetherStiffnessLinear, float TetherScale)
{
	// Deprecated
	const Chaos::TVec2<Chaos::FRealSingle> TetherStiffness((FMath::Clamp(FMath::Loge(TetherStiffnessLinear) * ChaosClothingInteractor::InvStiffnessLogBase + 1.f, 0.f, 1.f)), 1.f);
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([TetherStiffness, TetherScale](Chaos::FClothingSimulationCloth* Cloth)
	{
		Cloth->SetLongRangeAttachmentProperties(TetherStiffness, Chaos::TVec2<Chaos::FRealSingle>(TetherScale, TetherScale));
	}));
}

void UChaosClothingInteractor::SetLongRangeAttachment(FVector2D TetherStiffness, FVector2D TetherScale)
{
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([TetherStiffness, TetherScale](Chaos::FClothingSimulationCloth* Cloth)
	{
		Cloth->SetLongRangeAttachmentProperties(
			Chaos::TVec2<Chaos::FRealSingle>(TetherStiffness[0], TetherStiffness[1]),
			Chaos::TVec2<Chaos::FRealSingle>(TetherScale[0], TetherScale[1]));
	}));
}

void UChaosClothingInteractor::SetCollision(float CollisionThickness, float FrictionCoefficient, bool bUseCCD, float SelfCollisionThickness)
{
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([CollisionThickness, FrictionCoefficient, bUseCCD, SelfCollisionThickness](Chaos::FClothingSimulationCloth* Cloth)
	{
		Cloth->SetCollisionProperties(CollisionThickness, FrictionCoefficient, bUseCCD, SelfCollisionThickness);
	}));
}

void UChaosClothingInteractor::SetBackstop(bool bEnabled)
{
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([bEnabled](Chaos::FClothingSimulationCloth* Cloth)
	{
		Cloth->SetBackstopProperties(bEnabled);
	}));
}
void UChaosClothingInteractor::SetDamping(float DampingCoefficient, float LocalDampingCoefficient)
{
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([DampingCoefficient, LocalDampingCoefficient](Chaos::FClothingSimulationCloth* Cloth)
	{
		Cloth->SetDampingProperties(DampingCoefficient, LocalDampingCoefficient);
	}));
}

void UChaosClothingInteractor::SetAerodynamics(float DragCoefficient, float LiftCoefficient, FVector WindVelocity)
{
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([DragCoefficient, LiftCoefficient, WindVelocity](Chaos::FClothingSimulationCloth* Cloth)
	{
		constexpr Chaos::FRealSingle AirDensity = 1.225e-6f;
		Cloth->SetAerodynamicsProperties(Chaos::TVec2<Chaos::FRealSingle>(DragCoefficient, DragCoefficient), Chaos::TVec2<Chaos::FRealSingle>(LiftCoefficient, LiftCoefficient), AirDensity, WindVelocity);
	}));
}

void UChaosClothingInteractor::SetWind(FVector2D Drag, FVector2D Lift, float AirDensity, FVector WindVelocity)
{
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([Drag, Lift, AirDensity, WindVelocity](Chaos::FClothingSimulationCloth* Cloth)
	{
		Cloth->SetAerodynamicsProperties(Chaos::TVec2<Chaos::FRealSingle>(Drag[0], Drag[1]), Chaos::TVec2<Chaos::FRealSingle>(Lift[0], Lift[1]), AirDensity, WindVelocity);
	}));
}

void UChaosClothingInteractor::SetPressure(FVector2D Pressure)
{
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([Pressure](Chaos::FClothingSimulationCloth* Cloth)
	{
		Cloth->SetPressureProperties(Chaos::TVec2<Chaos::FRealSingle>(Pressure[0], Pressure[1]));
	}));
}

void UChaosClothingInteractor::SetGravity(float GravityScale, bool bIsGravityOverridden, FVector GravityOverride)
{
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([GravityScale, bIsGravityOverridden, GravityOverride](Chaos::FClothingSimulationCloth* Cloth)
	{
		Cloth->SetGravityProperties(GravityScale, bIsGravityOverridden, GravityOverride);
	}));
}

void UChaosClothingInteractor::SetAnimDriveLinear(float AnimDriveStiffnessLinear)
{
	// Deprecated
	const Chaos::TVec2<Chaos::FRealSingle> AnimDriveStiffness(0.f, FMath::Clamp(FMath::Loge(AnimDriveStiffnessLinear) * ChaosClothingInteractor::InvStiffnessLogBase + 1.f, 0.f, 1.f));
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([AnimDriveStiffness](Chaos::FClothingSimulationCloth* Cloth)
	{
		// The Anim Drive stiffness Low value needs to be 0 in order to keep backward compatibility with existing mask (this wouldn't be an issue if this property had no legacy mask)
		static const Chaos::TVec2<Chaos::FRealSingle> AnimDriveDamping(0.f, 1.f);
		Cloth->SetAnimDriveProperties(AnimDriveStiffness, AnimDriveDamping);
	}));
}

void UChaosClothingInteractor::SetAnimDrive(FVector2D AnimDriveStiffness, FVector2D AnimDriveDamping)
{
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([AnimDriveStiffness, AnimDriveDamping](Chaos::FClothingSimulationCloth* Cloth)
	{
		Cloth->SetAnimDriveProperties(Chaos::TVec2<Chaos::FRealSingle>(AnimDriveStiffness.X, AnimDriveStiffness.Y), Chaos::TVec2<Chaos::FRealSingle>(AnimDriveDamping.X, AnimDriveDamping.Y));
	}));
}

void UChaosClothingInteractor::SetVelocityScale(FVector LinearVelocityScale, float AngularVelocityScale, float FictitiousAngularScale)
{
	Commands.Add(FChaosClothingInteractorCommand::CreateLambda([LinearVelocityScale, AngularVelocityScale, FictitiousAngularScale](Chaos::FClothingSimulationCloth* Cloth)
	{
		Cloth->SetVelocityScaleProperties(LinearVelocityScale, AngularVelocityScale, FictitiousAngularScale);
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
	Commands.Add(FChaosClothingSimulationInteractorCommand::CreateLambda([NumIterations](Chaos::FClothingSimulation* Simulation, Chaos::FClothingSimulationContext* /*Context*/)
	{
		Simulation->SetNumIterations(NumIterations);
	}));
}

void UChaosClothingSimulationInteractor::SetMaxNumIterations(int32 MaxNumIterations)
{
	Commands.Add(FChaosClothingSimulationInteractorCommand::CreateLambda([MaxNumIterations](Chaos::FClothingSimulation* Simulation, Chaos::FClothingSimulationContext* /*Context*/)
	{
		Simulation->SetMaxNumIterations(MaxNumIterations);
	}));
}

void UChaosClothingSimulationInteractor::SetNumSubsteps(int32 NumSubsteps)
{
	Commands.Add(FChaosClothingSimulationInteractorCommand::CreateLambda([NumSubsteps](Chaos::FClothingSimulation* Simulation, Chaos::FClothingSimulationContext* /*Context*/)
	{
		Simulation->SetNumSubsteps(NumSubsteps);
	}));
}

UClothingInteractor* UChaosClothingSimulationInteractor::CreateClothingInteractor()
{
	return NewObject<UChaosClothingInteractor>(this);
}


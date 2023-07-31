// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Simulation/RigUnit_PointSimulation.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_PointSimulation)

FRigUnit_PointSimulation_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;

	float DeltaTime = Context.DeltaTime;

	FCRSimPointContainer& Simulation = WorkData.Simulation;
	TArray<FCachedRigElement>& BoneIndices = WorkData.BoneIndices;

	if (Context.State == EControlRigState::Init ||
		Simulation.Points.Num() != Points.Num() ||
		Simulation.Springs.Num() != Links.Num() ||
		Simulation.Forces.Num() != Forces.Num() ||
		Simulation.CollisionVolumes.Num() != CollisionVolumes.Num() ||
		BoneIndices.Num() != BoneTargets.Num())
	{
		Simulation.Reset();
		BoneIndices.Reset();
		return;
	}

	if(Simulation.AccumulatedTime < SMALL_NUMBER)
	{
		Simulation.TimeStep = 1.0f / FMath::Clamp<float>(SimulatedStepsPerSecond, 1.f, 120.f);

		bool bFoundErrors = false;
		for (const FCRSimLinearSpring& Link : Links)
		{
			if (Link.SubjectA < 0 || Link.SubjectA >= Points.Num() ||
				Link.SubjectB < 0 || Link.SubjectB >= Points.Num() ||
				Link.SubjectA == Link.SubjectB)
			{
				UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Link has invalid subjects: %d and %d."), Link.SubjectA, Link.SubjectB);
				bFoundErrors = true;
			}
		}

		for (const FRigUnit_PointSimulation_BoneTarget& BoneTarget : BoneTargets)
		{
			if (BoneTarget.TranslationPoint >= Points.Num())
			{
				UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Bone Target has invalid translation point: %d."), BoneTarget.TranslationPoint);
				bFoundErrors = true;
			}
			if (BoneTarget.PrimaryAimPoint >= Points.Num())
			{
				UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Bone Target has invalid primary aim point: %d."), BoneTarget.PrimaryAimPoint);
				bFoundErrors = true;
			}
			if (BoneTarget.SecondaryAimPoint >= Points.Num())
			{
				UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Bone Target has invalid secondary aim point: %d."), BoneTarget.SecondaryAimPoint);
				bFoundErrors = true;
			}
		}

		if (bFoundErrors)
		{
			return;
		}

		Simulation.Points.SetNum(Points.Num());
		Simulation.Springs.SetNum(Links.Num());
		Simulation.Forces.SetNum(Forces.Num());
		Simulation.CollisionVolumes.SetNum(CollisionVolumes.Num());

		if (Hierarchy)
		{
			for (const FRigUnit_PointSimulation_BoneTarget& BoneTarget : BoneTargets)
			{
				BoneIndices.Add(FCachedRigElement());
			}
		}

		DeltaTime = 0.f;
	}

	if (Simulation.Points.Num() == 0)
	{
		return;
	}

	for (int32 PointIndex = 0; PointIndex < Points.Num(); PointIndex++)
	{
		Simulation.Points[PointIndex].Mass = Points[PointIndex].Mass;
		Simulation.Points[PointIndex].Size = Points[PointIndex].Size;
		if (Simulation.AccumulatedTime < SMALL_NUMBER || Points[PointIndex].Mass < SMALL_NUMBER)
		{
			Simulation.Points[PointIndex] = Points[PointIndex];
		}
		else if (Points[PointIndex].InheritMotion > SMALL_NUMBER)
		{
			Simulation.Points[PointIndex].InheritMotion = FMath::Clamp<float>(Points[PointIndex].InheritMotion * Simulation.TimeStep, 0.f, 1.f);
			Simulation.Points[PointIndex].Position = FMath::Lerp<FVector>(Simulation.Points[PointIndex].Position, Points[PointIndex].Position, Simulation.Points[PointIndex].InheritMotion);
		}
	}
	for (int32 LinkIndex = 0; LinkIndex < Links.Num(); LinkIndex++)
	{
		if (Simulation.AccumulatedTime < SMALL_NUMBER)
		{
			Simulation.Springs[LinkIndex] = Links[LinkIndex];
			if (Simulation.Springs[LinkIndex].Equilibrium < -SMALL_NUMBER)
			{
				const FCRSimPoint SubjectA = Simulation.Points[Simulation.Springs[LinkIndex].SubjectA];
				const FCRSimPoint SubjectB = Simulation.Points[Simulation.Springs[LinkIndex].SubjectB];
				Simulation.Springs[LinkIndex].Equilibrium = (SubjectA.Position - SubjectB.Position).Size();
			}
		}
		else
		{
			Simulation.Springs[LinkIndex].Coefficient = Links[LinkIndex].Coefficient;
			if (Links[LinkIndex].Equilibrium >= 0.f)
			{
				Simulation.Springs[LinkIndex].Equilibrium = Links[LinkIndex].Equilibrium;
			}
		}
	}
	for (int32 ForceIndex = 0; ForceIndex < Forces.Num(); ForceIndex++)
	{
		Simulation.Forces[ForceIndex] = Forces[ForceIndex];
	}
	for (int32 VolumeIndex = 0; VolumeIndex < CollisionVolumes.Num(); VolumeIndex++)
	{
		Simulation.CollisionVolumes[VolumeIndex] = CollisionVolumes[VolumeIndex];
	}

	if (DeltaTime > SMALL_NUMBER)
	{
		switch (IntegratorType)
		{
		case ECRSimPointIntegrateType::Verlet:
		{
			Simulation.StepVerlet(DeltaTime, VerletBlend);
			break;
		}
		case ECRSimPointIntegrateType::SemiExplicitEuler:
		{
			Simulation.StepSemiExplicitEuler(DeltaTime);
			break;
		}
		}
	}

	if (Hierarchy)
	{
		TArray<FVector> Positions;
		Positions.SetNumUninitialized(Simulation.Points.Num());
		for (int32 PointIndex = 0; PointIndex < Positions.Num(); PointIndex++)
		{
			Positions[PointIndex] = Simulation.GetPointInterpolated(PointIndex).Position;
		}

		for (int32 TargetIndex = 0; TargetIndex < BoneTargets.Num(); TargetIndex++)
		{
			const FRigElementKey Key(BoneTargets[TargetIndex].Bone, ERigElementType::Bone);
			if (BoneIndices[TargetIndex].UpdateCache(Key, Hierarchy))
			{
				const FRigUnit_PointSimulation_BoneTarget& BoneTarget = BoneTargets[TargetIndex];
				FTransform Transform = Hierarchy->GetGlobalTransform(BoneIndices[TargetIndex]);

				if (BoneTarget.TranslationPoint >= 0 && BoneTarget.TranslationPoint < Simulation.Points.Num())
				{
					Transform.SetLocation(Positions[BoneTarget.TranslationPoint]);
				}

				if (bLimitLocalPosition)
				{
					FRigElementKey ParentKey = Hierarchy->GetFirstParent(BoneIndices[TargetIndex].GetKey());
					if (ParentKey.IsValid())
					{
						FTransform InitialTransform = Hierarchy->GetInitialGlobalTransform(BoneIndices[TargetIndex]);
						FTransform ParentInitialTransform = Hierarchy->GetGlobalTransform(ParentKey, true);
						FTransform ParentTransform = Hierarchy->GetGlobalTransform(ParentKey);
						float ExpectedDistance = (InitialTransform.GetLocation() - ParentInitialTransform.GetLocation()).Size();
						if (ExpectedDistance > SMALL_NUMBER)
						{
							FVector Direction = Transform.GetLocation() - ParentTransform.GetLocation();
							if (!Direction.IsNearlyZero())
							{
								Transform.SetLocation(ParentTransform.GetLocation() + Direction.GetSafeNormal() * ExpectedDistance);
								if (BoneTarget.TranslationPoint >= 0 && BoneTarget.TranslationPoint < Simulation.Points.Num())
								{
									Positions[BoneTarget.TranslationPoint] = Transform.GetLocation();
								}
							}
						}
					}
				}

				Hierarchy->SetGlobalTransform(BoneIndices[TargetIndex], Transform, false);
			}
		}

		for (int32 TargetIndex = 0; TargetIndex < BoneTargets.Num(); TargetIndex++)
		{
			if (BoneIndices[TargetIndex] != INDEX_NONE)
			{
				const FRigUnit_PointSimulation_BoneTarget& BoneTarget = BoneTargets[TargetIndex];
				FTransform Transform = Hierarchy->GetGlobalTransform(BoneIndices[TargetIndex]);

				FVector PrimaryAxis = Transform.TransformVectorNoScale(PrimaryAimAxis);

				if (BoneTarget.PrimaryAimPoint >= 0 && BoneTarget.PrimaryAimPoint < Simulation.Points.Num())
				{
					FVector PrimaryAimPosition = Positions[BoneTarget.PrimaryAimPoint];
					FVector Target = PrimaryAimPosition - Transform.GetLocation();

					if (!Target.IsNearlyZero() && !PrimaryAxis.IsNearlyZero())
					{
						PrimaryAxis = PrimaryAxis.GetSafeNormal();
						Target = Target.GetSafeNormal();
						FQuat Rotation = FQuat::FindBetweenNormals(PrimaryAxis, Target);
						Transform.SetRotation((Rotation * Transform.GetRotation()).GetNormalized());
					}
				}

				if (BoneTarget.SecondaryAimPoint >= 0 && BoneTarget.SecondaryAimPoint < Simulation.Points.Num())
				{
					FVector SecondaryAimPosition = Positions[BoneTarget.SecondaryAimPoint];
					FVector Target = SecondaryAimPosition - Transform.GetLocation();
					Target = Target - FVector::DotProduct(Target, PrimaryAxis) * PrimaryAxis;
					FVector SecondaryAxis = Transform.TransformVectorNoScale(SecondaryAimAxis);

					if (!Target.IsNearlyZero() && !SecondaryAxis.IsNearlyZero())
					{
						SecondaryAxis = SecondaryAxis.GetSafeNormal();
						Target = Target.GetSafeNormal();
						FQuat Rotation = FQuat::FindBetweenNormals(SecondaryAxis, Target);
						Transform.SetRotation((Rotation * Transform.GetRotation()).GetNormalized());
					}
				}

				Hierarchy->SetGlobalTransform(BoneIndices[TargetIndex], Transform, bPropagateToChildren);
			}
		}
	}

	if (Simulation.Points.Num() > 3)
	{
		Bezier.A = Simulation.GetPointInterpolated(0).Position;
		Bezier.B = Simulation.GetPointInterpolated(1).Position;
		Bezier.C = Simulation.GetPointInterpolated(2).Position;
		Bezier.D = Simulation.GetPointInterpolated(3).Position;
	}

	if (Context.DrawInterface != nullptr && DebugSettings.bEnabled)
	{
		Context.DrawInterface->DrawPointSimulation(DebugSettings.WorldOffset, Simulation, DebugSettings.Color, DebugSettings.Scale, DebugSettings.CollisionScale, DebugSettings.bDrawPointsAsSpheres);
	}
}

FRigVMStructUpgradeInfo FRigUnit_PointSimulation::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}


// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosCloth/ChaosClothingSimulationConfig.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Chaos/PBDLongRangeConstraints.h"  // For Tether modes
#include "GeometryCollection/ManagedArrayCollection.h"

namespace Chaos
{
	FClothingSimulationConfig::FClothingSimulationConfig()
	{
	}

	FClothingSimulationConfig::FClothingSimulationConfig(const TSharedPtr<const FManagedArrayCollection>& InPropertyCollection)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Initialize(InPropertyCollection);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	FClothingSimulationConfig::FClothingSimulationConfig(const TArray<TSharedPtr<const FManagedArrayCollection>>& InPropertyCollections)
	{
		Initialize(InPropertyCollections);
	}

	FClothingSimulationConfig::~FClothingSimulationConfig() = default;

	void FClothingSimulationConfig::Initialize(const UChaosClothConfig* ClothConfig, const UChaosClothSharedSimConfig* ClothSharedConfig, bool bUseLegacyConfig)
	{
		using namespace ::Chaos::Softs;
		constexpr ECollectionPropertyFlags NonAnimatablePropertyFlags =
			ECollectionPropertyFlags::Enabled |
			ECollectionPropertyFlags::Legacy;  // Indicates a property set from a pre-property collection config (e.g. that can be overriden in Dataflow without warning)
		constexpr ECollectionPropertyFlags AnimatablePropertyFlags = NonAnimatablePropertyFlags |
			ECollectionPropertyFlags::Animatable;

		// Clear all properties
		PropertyCollections.Reset(1);
		Properties.Reset(1);

		bIsLegacySingleLOD = true;
		TSharedPtr<FManagedArrayCollection>& PropertyCollection = PropertyCollections.Add_GetRef(MakeShared<FManagedArrayCollection>());
		TUniquePtr<Softs::FCollectionPropertyMutableFacade>& Property = Properties.Add_GetRef(MakeUnique<Softs::FCollectionPropertyMutableFacade>(PropertyCollection));
		Property->DefineSchema();

		// Solver properties
		if (ClothSharedConfig)
		{
			Property->AddValue(TEXT("NumIterations"), ClothSharedConfig->IterationCount, AnimatablePropertyFlags);
			Property->AddValue(TEXT("MaxNumIterations"), ClothSharedConfig->MaxIterationCount, AnimatablePropertyFlags);
			Property->AddValue(TEXT("NumSubsteps"), ClothSharedConfig->SubdivisionCount, AnimatablePropertyFlags);
		}

		// Cloth properties
		if (ClothConfig)
		{
			// Mass
			{
				float MassValue;
				switch (ClothConfig->MassMode)
				{
				case EClothMassMode::TotalMass:
					MassValue = ClothConfig->TotalMass;
					break;
				case EClothMassMode::UniformMass:
					MassValue = ClothConfig->UniformMass;
					break;
				default:
				case EClothMassMode::Density:
					MassValue = ClothConfig->Density;
					break;
				}
				Property->AddValue(TEXT("MassMode"), (int32)ClothConfig->MassMode, NonAnimatablePropertyFlags | ECollectionPropertyFlags::Intrinsic);
				Property->AddValue(TEXT("MassValue"), MassValue, NonAnimatablePropertyFlags | ECollectionPropertyFlags::Intrinsic);
				Property->AddValue(TEXT("MinPerParticleMass"), ClothConfig->MinPerParticleMass, NonAnimatablePropertyFlags | ECollectionPropertyFlags::Intrinsic);
			}

			// Edge constraint
			if (ClothConfig->EdgeStiffnessWeighted.Low > 0.f || ClothConfig->EdgeStiffnessWeighted.High > 0.f)
			{
				const int32 EdgeSpringStiffnessIndex = Property->AddProperty(TEXT("EdgeSpringStiffness"), AnimatablePropertyFlags);
				Property->SetWeightedValue(EdgeSpringStiffnessIndex, ClothConfig->EdgeStiffnessWeighted.Low, ClothConfig->EdgeStiffnessWeighted.High);
				Property->SetStringValue(EdgeSpringStiffnessIndex, TEXT("EdgeStiffness"));
			}

			// Bending constraint
			if (ClothConfig->BendingStiffnessWeighted.Low > 0.f || ClothConfig->BendingStiffnessWeighted.High > 0.f ||
				(ClothConfig->bUseBendingElements && (ClothConfig->BucklingStiffnessWeighted.Low > 0.f || ClothConfig->BucklingStiffnessWeighted.High > 0.f)))
			{
				if (ClothConfig->bUseBendingElements)
				{
					const int32 BendingElementStiffnessIndex = Property->AddProperty(TEXT("BendingElementStiffness"), AnimatablePropertyFlags);
					Property->SetWeightedValue(BendingElementStiffnessIndex, ClothConfig->BendingStiffnessWeighted.Low, ClothConfig->BendingStiffnessWeighted.High);
					Property->SetStringValue(BendingElementStiffnessIndex, TEXT("BendingStiffness"));

					Property->AddValue(TEXT("BucklingRatio"), ClothConfig->BucklingRatio, NonAnimatablePropertyFlags);

					if (ClothConfig->BucklingStiffnessWeighted.Low > 0.f || ClothConfig->BucklingStiffnessWeighted.High > 0.f)
					{
						const int32 BucklingStiffnessIndex = Property->AddProperty(TEXT("BucklingStiffness"), AnimatablePropertyFlags);
						Property->SetWeightedValue(BucklingStiffnessIndex, ClothConfig->BucklingStiffnessWeighted.Low, ClothConfig->BucklingStiffnessWeighted.High);
						Property->SetStringValue(BucklingStiffnessIndex, TEXT("BucklingStiffness"));
					}
				}
				else  // Not using bending elements
				{
					const int32 BendingSpringStiffnessIndex = Property->AddProperty(TEXT("BendingSpringStiffness"), AnimatablePropertyFlags);
					Property->SetWeightedValue(BendingSpringStiffnessIndex, ClothConfig->BendingStiffnessWeighted.Low, ClothConfig->BendingStiffnessWeighted.High);
					Property->SetStringValue(BendingSpringStiffnessIndex, TEXT("BendingStiffness"));
				}
			}

			// Area constraint
			if (ClothConfig->AreaStiffnessWeighted.Low > 0.f || ClothConfig->AreaStiffnessWeighted.High > 0.f)
			{
				const int32 AreaSpringStiffnessIndex = Property->AddProperty(TEXT("AreaSpringStiffness"), AnimatablePropertyFlags);
				Property->SetWeightedValue(AreaSpringStiffnessIndex, ClothConfig->AreaStiffnessWeighted.Low, ClothConfig->AreaStiffnessWeighted.High);
				Property->SetStringValue(AreaSpringStiffnessIndex, TEXT("AreaStiffness"));
			}

			// Long range attachment
			if (ClothConfig->TetherStiffness.Low > 0.f || ClothConfig->TetherStiffness.High > 0.f)
			{
				Property->AddValue(TEXT("UseGeodesicTethers"), ClothConfig->bUseGeodesicDistance, NonAnimatablePropertyFlags);

				const int32 TetherStiffnessIndex = Property->AddProperty(TEXT("TetherStiffness"), AnimatablePropertyFlags);
				Property->SetWeightedValue(TetherStiffnessIndex, ClothConfig->TetherStiffness.Low, ClothConfig->TetherStiffness.High);
				Property->SetStringValue(TetherStiffnessIndex, TEXT("TetherStiffness"));

				const int32 TetherScaleIndex = Property->AddProperty(TEXT("TetherScale"), AnimatablePropertyFlags);
				Property->SetWeightedValue(TetherScaleIndex, ClothConfig->TetherScale.Low, ClothConfig->TetherScale.High);
				Property->SetStringValue(TetherScaleIndex, TEXT("TetherScale"));
			}

			// AnimDrive
			if (ClothConfig->AnimDriveStiffness.Low > 0.f || ClothConfig->AnimDriveStiffness.High > 0.f)
			{
				const int32 AnimDriveStiffnessIndex = Property->AddProperty(TEXT("AnimDriveStiffness"), AnimatablePropertyFlags);
				Property->SetWeightedValue(AnimDriveStiffnessIndex, ClothConfig->AnimDriveStiffness.Low, ClothConfig->AnimDriveStiffness.High);
				Property->SetStringValue(AnimDriveStiffnessIndex, TEXT("AnimDriveStiffness"));

				const int32 AnimDriveDampingIndex = Property->AddProperty(TEXT("AnimDriveDamping"), AnimatablePropertyFlags);
				Property->SetWeightedValue(AnimDriveDampingIndex, ClothConfig->AnimDriveDamping.Low, ClothConfig->AnimDriveDamping.High);
				Property->SetStringValue(AnimDriveDampingIndex, TEXT("AnimDriveDamping"));
			}

			// Gravity
			{
				Property->AddValue(TEXT("GravityScale"), ClothConfig->GravityScale, AnimatablePropertyFlags);
				Property->AddValue(TEXT("UseGravityOverride"), ClothConfig->bUseGravityOverride, AnimatablePropertyFlags);
				Property->AddValue(TEXT("GravityOverride"), FVector3f(ClothConfig->Gravity), AnimatablePropertyFlags);
			}

			// Velocity scale
			{
				Property->AddValue(TEXT("LinearVelocityScale"), FVector3f(ClothConfig->LinearVelocityScale), AnimatablePropertyFlags);
				Property->AddValue(TEXT("AngularVelocityScale"), ClothConfig->AngularVelocityScale, AnimatablePropertyFlags);
				Property->AddValue(TEXT("FictitiousAngularScale"), ClothConfig->FictitiousAngularScale, AnimatablePropertyFlags);
			}

			// Aerodynamics
			Property->AddValue(TEXT("UsePointBasedWindModel"), ClothConfig->bUsePointBasedWindModel, NonAnimatablePropertyFlags);
			{
				const int32 DragIndex = Property->AddProperty(TEXT("Drag"), AnimatablePropertyFlags);
				Property->SetWeightedValue(DragIndex, ClothConfig->Drag.Low, ClothConfig->Drag.High);
				Property->SetStringValue(DragIndex, TEXT("Drag"));

				const int32 LiftIndex = Property->AddProperty(TEXT("Lift"), AnimatablePropertyFlags);
				Property->SetWeightedValue(LiftIndex, ClothConfig->Lift.Low, ClothConfig->Lift.High);
				Property->SetStringValue(LiftIndex, TEXT("Lift"));

				constexpr float AirDensity = 1.225f;  // Air density in kg/m^3
				Property->AddValue(TEXT("FluidDensity"), AirDensity, AnimatablePropertyFlags);

				Property->AddValue(TEXT("WindVelocity"), FVector3f(0.f), AnimatablePropertyFlags);  // Wind velocity must exist to be animatable
			}

			// Pressure
			{
				const int32 PressureIndex = Property->AddProperty(TEXT("Pressure"), AnimatablePropertyFlags);
				Property->SetWeightedValue(PressureIndex, ClothConfig->Pressure.Low, ClothConfig->Pressure.High);
				Property->SetStringValue(PressureIndex, TEXT("Pressure"));
			}

			// Damping
			Property->AddValue(TEXT("DampingCoefficient"), ClothConfig->DampingCoefficient, AnimatablePropertyFlags);
			Property->AddValue(TEXT("LocalDampingCoefficient"), ClothConfig->LocalDampingCoefficient, AnimatablePropertyFlags);

			// Collision
			Property->AddValue(TEXT("CollisionThickness"), ClothConfig->CollisionThickness, AnimatablePropertyFlags);
			Property->AddValue(TEXT("FrictionCoefficient"), ClothConfig->FrictionCoefficient, AnimatablePropertyFlags);
			Property->AddValue(TEXT("UseCCD"), ClothConfig->bUseCCD, AnimatablePropertyFlags);
			Property->AddValue(TEXT("UseSelfCollisions"), ClothConfig->bUseSelfCollisions, NonAnimatablePropertyFlags);
			Property->AddValue(TEXT("SelfCollisionThickness"), ClothConfig->SelfCollisionThickness, NonAnimatablePropertyFlags);
			Property->AddValue(TEXT("UseSelfIntersections"), ClothConfig->bUseSelfIntersections, NonAnimatablePropertyFlags);
			Property->AddValue(TEXT("SelfCollisionFriction"), ClothConfig->SelfCollisionFriction, NonAnimatablePropertyFlags);

			if (ClothConfig->bUseSelfCollisionSpheres)
			{
				Property->AddValue(TEXT("SelfCollisionSphereRadius"), ClothConfig->SelfCollisionSphereRadius, NonAnimatablePropertyFlags);
				Property->AddValue(TEXT("SelfCollisionSphereStiffness"), ClothConfig->SelfCollisionSphereStiffness, AnimatablePropertyFlags);
				static const FString SelfCollisionSphereSetName(TEXT("SelfCollisionSphereSetName"));
				Property->AddStringValue(SelfCollisionSphereSetName, SelfCollisionSphereSetName, NonAnimatablePropertyFlags);
			}

			// Max distance
			{
				const int32 MaxDistanceIndex = Property->AddProperty(TEXT("MaxDistance"), AnimatablePropertyFlags);
				Property->SetWeightedValue(MaxDistanceIndex, 0.f, 1.f);  // Backward compatibility with legacy mask must use a unit range since the multiplier is in the mask
				Property->SetStringValue(MaxDistanceIndex, TEXT("MaxDistance"));
			}

			// Backstop
			{
				const int32 BackstopDistanceIndex = Property->AddProperty(TEXT("BackstopDistance"), AnimatablePropertyFlags);
				Property->SetWeightedValue(BackstopDistanceIndex, 0.f, 1.f);  // Backward compatibility with legacy mask must use a unit range since the multiplier is in the mask
				Property->SetStringValue(BackstopDistanceIndex, TEXT("BackstopDistance"));

				const int32 BackstopRadiusIndex = Property->AddProperty(TEXT("BackstopRadius"), AnimatablePropertyFlags);
				Property->SetWeightedValue(BackstopRadiusIndex, 0.f, 1.f);  // Backward compatibility with legacy mask must use a unit range since the multiplier is in the mask
				Property->SetStringValue(BackstopRadiusIndex, TEXT("BackstopRadius"));

				Property->AddValue(TEXT("UseLegacyBackstop"), ClothConfig->bUseLegacyBackstop, NonAnimatablePropertyFlags);
			}
		}

		// Mark this as a potential legacy config, but leave the behavior control to the client code (usually means constraint are removed with 0 stiffness, or missing weight maps)
		Property->AddValue(TEXT("UseLegacyConfig"), bUseLegacyConfig, NonAnimatablePropertyFlags);
	}

	void FClothingSimulationConfig::Initialize(const TArray<TSharedPtr<const FManagedArrayCollection>>& InPropertyCollections)
	{
		PropertyCollections.Reset(InPropertyCollections.Num());
		Properties.Reset(InPropertyCollections.Num());

		for (const TSharedPtr<const FManagedArrayCollection>& InPropertyCollection : InPropertyCollections)
		{
			TSharedPtr<FManagedArrayCollection>& PropertyCollection = PropertyCollections.Add_GetRef(MakeShared<FManagedArrayCollection>());
			TUniquePtr<Softs::FCollectionPropertyMutableFacade>& Property = Properties.Add_GetRef(MakeUnique<Softs::FCollectionPropertyMutableFacade>(PropertyCollection));
			Property->Copy(*InPropertyCollection);
		}
	}
	const Softs::FCollectionPropertyConstFacade& FClothingSimulationConfig::GetProperties(int32 LODIndex) const
	{
		return bIsLegacySingleLOD ? *Properties[0] : *Properties[LODIndex];
	}
	Softs::FCollectionPropertyFacade& FClothingSimulationConfig::GetProperties(int32 LODIndex)
	{
		return bIsLegacySingleLOD ? *Properties[0] : *Properties[LODIndex];
	}
}  // End namespace Chaos

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Example/MassVelocityRandomizerTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "Engine/World.h"
#include "MassMovementFragments.h"


//----------------------------------------------------------------------//
//  UMassVelocityRandomizerTrait
//----------------------------------------------------------------------//
void UMassVelocityRandomizerTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassVelocityFragment& VelocityTemplate = BuildContext.AddFragment_GetRef<FMassVelocityFragment>();
	// This is a small @hack to support sending parameters to initializer.
	// A proper solution will allow users to specify a "lambda" initializer that will be used during creation
	VelocityTemplate.Value.X = MinSpeed;
	VelocityTemplate.Value.Y = MaxSpeed;
	VelocityTemplate.Value.Z = bSetZComponent ? 1.f : 0.f;
}

//----------------------------------------------------------------------//
//  UMassRandomVelocityInitializer
//----------------------------------------------------------------------//
UMassRandomVelocityInitializer::UMassRandomVelocityInitializer()
	: EntityQuery(*this)
{
	ObservedType = FMassVelocityFragment::StaticStruct();
	Operation = EMassObservedOperation::Add;
}

void UMassRandomVelocityInitializer::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
}

void UMassRandomVelocityInitializer::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// note: the author is aware that the vectors produced below are not distributed uniformly, but it's good enough
	EntityQuery.ForEachEntityChunk(EntityManager, Context, ([this](FMassExecutionContext& Context)
		{
			const TArrayView<FMassVelocityFragment> VelocitiesList = Context.GetMutableFragmentView<FMassVelocityFragment>();
			for (FMassVelocityFragment& VelocityFragment : VelocitiesList)
			{
				// the given VelocityFragment's value is encoding the initialization parameters, as per comment in
				// UMassVelocityRandomizerTrait::BuildTemplate
				const FVector RandomVector = VelocityFragment.Value.Z != 0.f 
					? FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)).GetSafeNormal()
					: FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0).GetSafeNormal();

				VelocityFragment.Value = RandomVector * FMath::FRandRange(VelocityFragment.Value.X, VelocityFragment.Value.Y);
			}
		}));
}

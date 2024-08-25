// Copyright Epic Games, Inc. All Rights Reserved.

#include "Kismet/KismetMathLibrary.h"
#include "Misc/AutomationTest.h"
#include "Modifiers/ActorModifierCoreStack.h"
#include "Modifiers/AvaLookAtModifier.h"
#include "Subsystems/ActorModifierCoreSubsystem.h"
#include "Tests/Framework/AvaModifiersTestUtils.h"
#include "Tests/Framework/AvaTestDynamicMeshActor.h"
#include "Tests/Framework/AvaTestUtils.h"

BEGIN_DEFINE_SPEC(AvalancheModifiersLookAt, "Avalanche.Modifiers.LookAt",
                  EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)

	AAvaTestDynamicMeshActor* ModifiedActor;
	AAvaTestDynamicMeshActor* ReferenceActor;
	double RandomMin = -2000;
	double RandomMax = 2000;

	FTransform InitialModifiedState;
	FTransform InitialReferenceSate;

	UAvaLookAtModifier* LookAtModifier;

	TSharedPtr<FAvaTestUtils> TestUtils = MakeShared<FAvaTestUtils>();
	TSharedPtr<FAvaModifierTestUtils> ModifierTestUtils = MakeShared<FAvaModifierTestUtils>(TestUtils);

END_DEFINE_SPEC(AvalancheModifiersLookAt);

void AvalancheModifiersLookAt::Define()
{
	BeforeEach([this]
	{
		TestUtils->Init();

		// Spawn actors
		ModifiedActor = ModifierTestUtils->SpawnTestDynamicMeshActor(FTransform(FVector(
			FMath::RandRange(RandomMin, RandomMax),
			FMath::RandRange(RandomMin, RandomMax),
			FMath::RandRange(RandomMin, RandomMax))));
		ReferenceActor = ModifierTestUtils->SpawnTestDynamicMeshActor(FTransform(FVector(
			FMath::RandRange(RandomMin, RandomMax),
			FMath::RandRange(RandomMin, RandomMax),
			FMath::RandRange(RandomMin, RandomMax))));

		// Store initial actors' state
		InitialModifiedState = ModifiedActor->GetActorTransform();
		InitialReferenceSate = ReferenceActor->GetActorTransform();

		// Set up modifier
		const UActorModifierCoreSubsystem* ModifierSubsystem = UActorModifierCoreSubsystem::Get();
		const FName ModifierLookAtName = ModifierTestUtils->GetModifierName(UAvaLookAtModifier::StaticClass());
		FActorModifierCoreStackInsertOp InsertOp = ModifierTestUtils->GenerateInsertOp(ModifierLookAtName);
		UActorModifierCoreStack* ModifierStack = ModifierTestUtils->GenerateModifierStackForActor(ModifiedActor);
		LookAtModifier = Cast<UAvaLookAtModifier>(ModifierSubsystem->InsertModifier(ModifierStack, InsertOp));
		LookAtModifier->SetReferenceActor(FAvaSceneTreeActor(ReferenceActor));
	});

	AfterEach([this]
	{
		TestUtils->Destroy();
	});

	Describe("When LookAt modifier is applied to an actor", [this]
	{
		It("Should rotate modified actor towards the reference actor location", [this]
		{
			const FVector CurrentModifiedLocation = ModifiedActor->GetActorLocation();
			const FVector CurrentReferenceLocation = ReferenceActor->GetActorLocation();
			const FRotator ExpectedRotator = UKismetMathLibrary::FindLookAtRotation(
				CurrentReferenceLocation, CurrentModifiedLocation);

			TestEqual("Modified actor didn't change location",
			          CurrentModifiedLocation, InitialModifiedState.GetLocation());
			TestEqual("Reference actor didn't change location",
			          CurrentReferenceLocation, InitialReferenceSate.GetLocation());
			TestEqual("Rotator value wasn't changed for the Reference actor",
			          ReferenceActor->GetActorRotation(), InitialReferenceSate.Rotator());

			if (!TestNearlyEqual("Modified actor Rotator value is valid",
			                     ModifiedActor->GetActorRotation(), ExpectedRotator))
			{
				UE_LOG(LogAvaModifiersTest, Display, TEXT("Current modified location: %s"),
				       *CurrentModifiedLocation.ToString());
				UE_LOG(LogAvaModifiersTest, Display, TEXT("Current reference  location: %s"),
				       *CurrentReferenceLocation.ToString());
			}
		});
	});
}

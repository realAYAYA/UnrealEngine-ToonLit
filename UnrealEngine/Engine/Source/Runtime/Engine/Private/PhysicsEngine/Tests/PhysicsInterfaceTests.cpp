// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Physics/Tests/PhysicsTestHelpers.h"
#include "Physics/PhysicsInterfaceCore.h"
#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicsInterfaceTest, "System.Physics.Interface.ObjectCreation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysicsInterfaceTest::RunTest(const FString& Parameters)
{
	FPhysicsActorHandle ActorHandle;
	FPhysicsInterface::CreateActor(FActorCreationParams(), ActorHandle);
	// TODO: Make some assertions
	FPhysicsInterface::ReleaseActor(ActorHandle);
	// TODO: Make some more assertions
	return true;
}


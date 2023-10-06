// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MapTestSpawner.h"
#include "Commands/TestCommands.h"

#include "Tests/AutomationCommon.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"

void FMapTestSpawner::AddWaitUntilLoadedCommand(FAutomationTestBase* TestRunner)
{
#if WITH_AUTOMATION_TESTS
	check(PieWorld == nullptr);

	const FString FileName = FString::Printf(TEXT("%s.%s"), *MapName, *MapName);
	const FString Path = FPaths::Combine(MapDirectory, FileName);
	bool bOpened = AutomationOpenMap(Path);
	check(bOpened);

	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(*TestRunner, [&]() -> bool {
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (((Context.WorldType == EWorldType::PIE) || (Context.WorldType == EWorldType::Game)) && (Context.World() != nullptr))
			{
				PieWorld = Context.World();
				return true;
			}
		}

		return false;
	}));
#else
	checkf(false, TEXT("AddWaitUntilLoadedCommand can't call AutomationOpenMap if WITH_AUTOMATION_TESTS=false"));
#endif
}

UWorld* FMapTestSpawner::CreateWorld()
{
	checkf(PieWorld, TEXT("Must call AddWaitUntilLoadedCommand in BEFORE_TEST"));
	return PieWorld;
}

APawn* FMapTestSpawner::FindFirstPlayerPawn()
{
	return GetWorld().GetFirstPlayerController()->GetPawn();
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor/UnrealEd/Public/Editor.h" 
#include "Editor/UnrealEd/Public/Selection.h"
#include "FileHelpers.h"

// Asynchronous function to select the input actor
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBSelectActor, AActor**, TestAsset);
bool FBSelectActor::Update()
{
	constexpr bool bInSelected = true;
	constexpr bool bNotify = true;

	GEditor->SelectActor(*TestAsset, bInSelected, bNotify);

	return true;
}

// Asynchronous function that checks whether the input actor is selected or not
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FBVerifyActorSelection, FAutomationTestBase*, Test, AActor**, TestAsset);
bool FBVerifyActorSelection::Update()
{
	if (*TestAsset != nullptr)
	{
		bool bIsSelected = GEditor->GetSelectedActors()->IsSelected(*TestAsset);
		FString ActorName = (*TestAsset)->GetName();

		// Check if the actor is selected
		if (bIsSelected)
		{
			UE_LOG(LogTemp, Display, TEXT("The target actor: '%s' is successfully selected."), *ActorName);
		}
		else
		{
			Test->AddError(FString::Printf(TEXT("The target actor: '%s' is NOT selected."), *ActorName));
		}
	}
	else
	{
		Test->AddError(TEXT("The target actor is a null pointer."));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetActorTest, "Editor.Workflows.Selection.Actor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAssetActorTest::RunTest(const FString& Parameters)
{
	UWorld** TestWorld = (UWorld**)FMemory::Malloc(sizeof(UWorld*)); 
	AActor** TestAsset = (AActor**)FMemory::Malloc(sizeof(AActor*));

	const FString EditorTestName = TEXT("Editor.Workflows.Selection.Actor");
	const TCHAR* TestMapPath = TEXT("/Game/Tests/Selection/HierarchicalISMSelectionTestMap");

	// Setup
	AddCommand(new FCloseAllAssetEditorsCommand());
	AddCommand(new FFunctionLatentCommand([EditorTestName] {
		TRACE_BOOKMARK(*(EditorTestName + TEXT(" ProfileBegin")));

		return true;
		}));

	// Issue Load request
	// Spawn a latent command to wait for map loading to complete
	AddCommand(new FOpenEditorForAssetCommand(TestMapPath));
	AddCommand(new FFunctionLatentCommand([TestWorld]() {
		*TestWorld = GEditor->GetEditorWorldContext().World();

		return *TestWorld != nullptr;
		}));

	AddCommand(new FWaitForEngineFramesCommand(20));

	AddCommand(new FFunctionLatentCommand([TestAsset, TestWorld, this, EditorTestName] {
		// Spawn the test asset actor
		*TestAsset = (*TestWorld)->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (*TestAsset == nullptr)
		{
			AddError(EditorTestName + TEXT(": Failed to spawn the test asset."));
		}

		return true;
		}));

	ADD_LATENT_AUTOMATION_COMMAND(FBSelectActor(TestAsset));

	ADD_LATENT_AUTOMATION_COMMAND(FBVerifyActorSelection(this, TestAsset));

	AddCommand(new FWaitForEngineFramesCommand(20));

	// Teardown
	AddCommand(new FFunctionLatentCommand([TestAsset, TestWorld, EditorTestName] {
		FMemory::Free(TestWorld);
		FMemory::Free(TestAsset);

		TRACE_BOOKMARK(*(EditorTestName + TEXT(" ProfileEnd")));

		return true;
		}));

	return true;
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor/UnrealEd/Public/Editor.h" 
#include "Editor/UnrealEd/Public/Selection.h" 
#include "Components/InstancedStaticMeshComponent.h"
#include "FileHelpers.h"
#include "EngineUtils.h"

// Asynchronous function to select the ISM component
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBSelectISMComponent, UInstancedStaticMeshComponent**, ISMComponent);
bool FBSelectISMComponent::Update()
{
	constexpr bool bInSelected = true;
	constexpr bool bNotify = true;
	constexpr bool bSelectEvenIfHidden = true;

	GEditor->SelectComponent(*ISMComponent, bInSelected, bNotify, bSelectEvenIfHidden);

	return true;
}

// Asynchronous function that checks whether the input ISM component is selected or not
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FBVerifyISMComponentSelection, FAutomationTestBase*, Test, UInstancedStaticMeshComponent**, ISMComponent);
bool FBVerifyISMComponentSelection::Update()
{
	if (*ISMComponent != nullptr)
	{
		bool bIsSelected = GEditor->GetSelectedComponents()->IsSelected(*ISMComponent);
		FString ComponentName = (*ISMComponent)->GetName();

		// Check if the ISM component is selected
		if (bIsSelected)
		{
			UE_LOG(LogTemp, Display, TEXT("The target ISM Component: '%s' is successfully selected."), *ComponentName);
		}
		else
		{
			Test->AddError(FString::Printf(TEXT("The target ISM Component: '%s' is NOT selected."), *ComponentName));
		}
	}
	else
	{
		Test->AddError(TEXT("The target ISM component is a null pointer."));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetHierarchicalISMSelectionTest, "Editor.Workflows.Selection.HierarchicalISM", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAssetHierarchicalISMSelectionTest::RunTest(const FString& Parameters)
{
	UWorld** TestWorld = (UWorld**)FMemory::Malloc(sizeof(UWorld*));
	UInstancedStaticMeshComponent** ISMComponent = (UInstancedStaticMeshComponent**)FMemory::Malloc(sizeof(UInstancedStaticMeshComponent*));

	const FString EditorTestName = TEXT("Editor.Workflows.Selection.HierarchicalISM");
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

	AddCommand(new FFunctionLatentCommand([ISMComponent, TestWorld, this, EditorTestName] {
		// Get the target actor in the test world
		AActor* TestAsset = nullptr;
		for (TActorIterator<AActor> ActorIt(*TestWorld); ActorIt; ++ActorIt)
		{
			if (ActorIt->GetActorLabel() == "BP_BadPerfSelectingISM")
			{
				TestAsset = *ActorIt;
				break;
			}
		}
		if (TestAsset != nullptr)
		{
			*ISMComponent = TestAsset->FindComponentByClass<UInstancedStaticMeshComponent>();
		}
		else
		{
			AddError(EditorTestName + TEXT(": Test asset not found in test map."));
		}

		return true;
		}));

	ADD_LATENT_AUTOMATION_COMMAND(FBSelectISMComponent(ISMComponent));

	ADD_LATENT_AUTOMATION_COMMAND(FBVerifyISMComponentSelection(this, ISMComponent));

	AddCommand(new FWaitForEngineFramesCommand(20));

	// Teardown
	AddCommand(new FFunctionLatentCommand([ISMComponent, TestWorld, EditorTestName] {
		FMemory::Free(TestWorld);
		FMemory::Free(ISMComponent);

		TRACE_BOOKMARK(*(EditorTestName + TEXT(" ProfileEnd")));

		return true;
		}));

	return true;
}
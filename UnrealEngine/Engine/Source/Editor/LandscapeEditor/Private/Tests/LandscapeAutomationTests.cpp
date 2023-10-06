// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Engine/DirectionalLight.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/AutomationTest.h"
#include "Misc/EngineVersion.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelEditorViewport.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "LandscapeEditorObject.h"
#include "LandscapeInfoMap.h"

#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

#include "LandscapeEditorDetailCustomization_NewLandscape.h"
#include "LandscapeEditorDetailCustomization_ImportExport.h"

#if WITH_DEV_AUTOMATION_TESTS

#define LOCTEXT_NAMESPACE "LandscapeEditor.NewLandscape"

DEFINE_LOG_CATEGORY_STATIC(LogLandscapeAutomationTests, Log, All);

/**
* Landscape test helper functions
*/
namespace LandscapeTestUtils
{
	/**
	* Finds the viewport to use for the landscape tool
	*/
	static FLevelEditorViewportClient* FindSelectedViewport()
	{
		FLevelEditorViewportClient* SelectedViewport = NULL;

		for(FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
		{
			if (!ViewportClient->IsOrtho())
			{
				SelectedViewport = ViewportClient;
			}
		}

		return SelectedViewport;
	}

	struct LandscapeTestCommands
	{
		static void Import(const FString& HeightMapFilenamee);
	};

	void LandscapeTestCommands::Import(const FString& HeightMapFilename)
	{
		//Switch to the Landscape tool
		GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Landscape);
		FEdModeLandscape* LandscapeEdMode = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);

		TSharedPtr<FLandscapeEditorDetailCustomization_ImportExport> Customization_ImportExportLandscape = MakeShareable(new FLandscapeEditorDetailCustomization_ImportExport);

		LandscapeEdMode->UISettings->ImportLandscape_HeightmapFilename = HeightMapFilename;
		LandscapeEdMode->UISettings->bHeightmapSelected = true;
		LandscapeEdMode->UISettings->ImportExportMode = ELandscapeImportExportMode::All;
		LandscapeEdMode->UISettings->ImportType = ELandscapeImportTransformType::Resample;

		Customization_ImportExportLandscape->OnOriginResetButtonClicked();

		Customization_ImportExportLandscape->OnImportExportButtonClicked();
	}
}

/**
* Latent command to create a new landscape
*/
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FCreateLandscapeCommand,int32,NumComponents,int32, QuadsPerComponent);
bool FCreateLandscapeCommand::Update()
{
	//Switch to the Landscape tool
	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Landscape);
	FEdModeLandscape* LandscapeEdMode = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);

	//Modify the "Section size"
	LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection = QuadsPerComponent;

	LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = NumComponents;
	LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = NumComponents;
	LandscapeEdMode->UISettings->NewLandscape_ClampSize();

	//Create the landscape
	TSharedPtr<FLandscapeEditorDetailCustomization_NewLandscape> Customization_NewLandscape = MakeShareable(new FLandscapeEditorDetailCustomization_NewLandscape);
	Customization_NewLandscape->OnCreateButtonClicked();

	if (LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
	{
		UE_LOG(LogLandscapeAutomationTests, Display, TEXT("Created a new landscape"));
	}
	else
	{
		UE_LOG(LogLandscapeAutomationTests, Error, TEXT("Failed to create a new landscape"));
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FImportLandscapeCommand, FString, HeightMapFilename);
bool FImportLandscapeCommand::Update()
{
	LandscapeTestUtils::LandscapeTestCommands::Import(HeightMapFilename);
	return true;
}


DEFINE_LATENT_AUTOMATION_COMMAND(FAddDirectionalLight);
bool FAddDirectionalLight::Update()
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	
	ADirectionalLight*  DirectionalLight = World->SpawnActor<ADirectionalLight>();

	return true;
}

/**
* Latent command to start using the sculpting tool
*/
DEFINE_LATENT_AUTOMATION_COMMAND(FBeginModifyLandscapeCommand);
bool FBeginModifyLandscapeCommand::Update()
{
	//Find the landscape
	FEdModeLandscape* LandscapeEdMode = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);

	//Find a location on the edge of the landscape along the x axis so the default camera can see it in the distance.
	FVector LandscapeSizePerComponent = LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection * LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_Scale;
	FVector TargetLoctaion(0);
	TargetLoctaion.X = -LandscapeSizePerComponent.X * (LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X / 2.f);

	ALandscapeProxy* Proxy = LandscapeEdMode->CurrentToolTarget.LandscapeInfo.Get()->GetCurrentLevelLandscapeProxy(true);
	if (Proxy)
	{
		TargetLoctaion = Proxy->LandscapeActorToWorld().InverseTransformPosition(TargetLoctaion);
	}

	//Begin using the sculpting tool
	FLevelEditorViewportClient* SelectedViewport = LandscapeTestUtils::FindSelectedViewport();
	LandscapeEdMode->CurrentTool->BeginTool(SelectedViewport, LandscapeEdMode->CurrentToolTarget, TargetLoctaion);
	SelectedViewport->Invalidate();

	UE_LOG(LogLandscapeAutomationTests, Display, TEXT("Modified the landscape using the sculpt tool"));

	return true;
}

/**
*  Latent command stop using the sculpting tool
*/
DEFINE_LATENT_AUTOMATION_COMMAND(FEndModifyLandscapeCommand);
bool FEndModifyLandscapeCommand::Update()
{
	//Find the landscape
	FEdModeLandscape* LandscapeEdMode = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);

	//End using the sculpting tool
	FLevelEditorViewportClient* SelectedViewport = LandscapeTestUtils::FindSelectedViewport();
	LandscapeEdMode->CurrentTool->EndTool(SelectedViewport);
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND(FCheckHeight);
bool FCheckHeight::Update()
{
	FAutomationTestBase* CurrentTest = FAutomationTestFramework::Get().GetCurrentTest();
	if (!CurrentTest)
	{
		return true;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return true;
	}

	
	for (const auto& InfoMapPair : ULandscapeInfoMap::GetLandscapeInfoMap(World).Map)
	{
		TOptional<float> Height = InfoMapPair.Value->GetLandscapeProxy()->GetHeightAtLocation(FVector(0, 0, 0), EHeightfieldSource::Editor);
		CurrentTest->TestEqual("Has Height Value at 0,0", Height.IsSet(), true);
		CurrentTest->TestNearlyEqual("Height Value at 0,0 is 0", Height.GetValue(), 100.0f, 1e-4f);
		break;
	}
	return true;
}

/**
* Landscape creation / edit test
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLandscapeEditorTest, "Editor.Landscape CreateAndModify", EAutomationTestFlags::EditorContext | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::EngineFilter);
bool FLandscapeEditorTest::RunTest(const FString& Parameters)
{
	//New level
	UWorld* NewMap = FAutomationEditorCommonUtils::CreateNewMap();
	if (NewMap)
	{
		UE_LOG(LogLandscapeAutomationTests, Display, TEXT("Created an empty level"));
	}
	else
	{
		UE_LOG(LogLandscapeAutomationTests, Error, TEXT("Failed to create an empty level"));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FCreateLandscapeCommand(/* NumComponents = */ 8, /* QuadsPerComponent = */ 7));

	//For some reason the heightmap component takes a few ticks to register with the nav system.  We crash if we try to modify the heightmap before then.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeginModifyLandscapeCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FEndModifyLandscapeCommand());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLandscapeEditorTestFooBar, "Editor.Landscape Import", EAutomationTestFlags::EditorContext | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::EngineFilter);
bool FLandscapeEditorTestFooBar::RunTest(const FString& Parameters)
{
	//New level
	UWorld* NewMap = FAutomationEditorCommonUtils::CreateNewMap();
	if (NewMap)
	{
		UE_LOG(LogLandscapeAutomationTests, Display, TEXT("Created an empty level"));
	}
	else
	{
		UE_LOG(LogLandscapeAutomationTests, Error, TEXT("Failed to create an empty level"));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FCreateLandscapeCommand(/* NumComponents = */ 8, /* QuadsPerComponent = */ 63));

	//For some reason the heightmap component takes a few ticks to register with the nav system.  We crash if we try to modify the heightmap before then.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));

	FString HeightMapFilename = FPaths::Combine(FPaths::EngineContentDir(), FString(TEXT("FunctionalTesting\\height-505-flat.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FImportLandscapeCommand(HeightMapFilename));

	ADD_LATENT_AUTOMATION_COMMAND(FAddDirectionalLight());

	ADD_LATENT_AUTOMATION_COMMAND(FCheckHeight());

	return true;
}



#undef LOCTEXT_NAMESPACE

#endif //WITH_DEV_AUTOMATION_TESTS

// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScreenshotFunctionalTest.h"

#include "Engine/GameViewportClient.h"
#include "AutomationBlueprintFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Misc/AutomationTest.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Slate/SceneViewport.h"
#include "UObject/AutomationObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ScreenshotFunctionalTest)

AScreenshotFunctionalTest::AScreenshotFunctionalTest( const FObjectInitializer& ObjectInitializer )
	: AScreenshotFunctionalTestBase(ObjectInitializer)
	, bCameraCutOnScreenshotPrep(true)
{
}

void AScreenshotFunctionalTest::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FAutomationObjectVersion::GUID);

	if (Ar.CustomVer(FAutomationObjectVersion::GUID) < FAutomationObjectVersion::DefaultToScreenshotCameraCutAndFixedTonemapping)
	{
		bCameraCutOnScreenshotPrep = true;
	}
}

void AScreenshotFunctionalTest::PrepareTest()
{
	// Pre-prep flush to allow rendering to temporary targets and other test resources
	UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot();

	Super::PrepareTest();

	// Apply a camera cut if requested
	if (bCameraCutOnScreenshotPrep)
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

		if (PlayerController && PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->SetGameCameraCutThisFrame();
			if (ScreenshotCamera)
			{
				ScreenshotCamera->NotifyCameraCut();
			}
		}
	}

	// Post-prep flush deal with any temporary resources allocated during prep before the main test
	UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot();
}

void AScreenshotFunctionalTest::RequestScreenshot()
{
	Super::RequestScreenshot();

	if(IsMobilePlatform(GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel]))
	{
		// For mobile, use the high res screenshot API to ensure a fixed resolution screenshot is produced.
		// This means screenshot comparisons can compare with the output from any device.
		FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
		FIntPoint ScreenshotViewportSize = UAutomationBlueprintFunctionLibrary::GetAutomationScreenshotSize(ScreenshotOptions);
		if (Config.SetResolution(ScreenshotViewportSize.X, ScreenshotViewportSize.Y, 1.0f))
		{
			GEngine->GameViewport->GetGameViewport()->TakeHighResScreenShot();
		}
	}
	else
	{
		// Screenshots in Unreal Engine work in this way:
		// 1. Call FScreenshotRequest::RequestScreenshot to ask the system to take a screenshot. The screenshot
		//    will have the same resolution as the current viewport;
		// 2. Register a callback to UGameViewportClient::OnScreenshotCaptured() delegate. The call back will be
		//    called with screenshot pixel data when the shot is taken;
		// 3. Wait till the next frame or call FSceneViewport::Invalidate to force a redraw. Screenshot is not
		//    taken until next draw where UGameViewportClient::ProcessScreenshots or
		//    FEditorViewportClient::ProcessScreenshots is called to read pixels back from the viewport. It also
		//    trigger the callback function registered in step 2.

		bool bShowUI = false;
		FScreenshotRequest::RequestScreenshot(bShowUI);
	}
}


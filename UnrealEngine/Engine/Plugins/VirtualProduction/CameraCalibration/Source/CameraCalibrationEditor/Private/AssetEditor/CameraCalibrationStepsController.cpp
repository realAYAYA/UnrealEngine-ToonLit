// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraCalibrationStepsController.h"

#include "AssetRegistry/AssetData.h"
#include "CalibrationPointComponent.h"
#include "Camera/CameraActor.h"
#include "CameraCalibrationEditorLog.h"
#include "CameraCalibrationSettings.h"
#include "CameraCalibrationStep.h"
#include "CameraCalibrationSubsystem.h"
#include "CameraCalibrationToolkit.h"
#include "CameraCalibrationTypes.h"
#include "CineCameraActor.h"
#include "CompElementEditorModule.h"
#include "Components/SceneCaptureComponent2D.h"
#include "CompositingCaptureBase.h"
#include "CompositingElement.h"
#include "CompositingElements/CompositingElementInputs.h"
#include "CompositingElements/CompositingElementOutputs.h"
#include "CompositingElements/CompositingElementPasses.h"
#include "CompositingElements/CompositingElementTransforms.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserDefinedEnum.h"
#include "EngineUtils.h"
#include "ICompElementManager.h"
#include "Input/Events.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "LensComponent.h"
#include "LensFile.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "Misc/MessageDialog.h"
#include "Models/SphericalLensModel.h"
#include "Modules/ModuleManager.h"
#include "Profile/IMediaProfileManager.h"
#include "Profile/MediaProfile.h"
#include "SCameraCalibrationSteps.h"
#include "TextureResource.h"
#include "TimeSynchronizableMediaSource.h"
#include "UnrealClient.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWidget.h"


#define LOCTEXT_NAMESPACE "CameraCalibrationStepsController"

namespace CameraCalibrationStepsController
{
	/** Returns the ICompElementManager object */
	ICompElementManager* GetCompElementManager()
	{
		static ICompElementEditorModule* EditorModule = FModuleManager::Get().GetModulePtr<ICompElementEditorModule>("ComposureLayersEditor");

		TSharedPtr<ICompElementManager> CompElementManager;

		if (EditorModule && (CompElementManager = EditorModule->GetCompElementManager()).IsValid())
		{
			return CompElementManager.Get();
		}
		return nullptr;
	}
}


FCameraCalibrationStepsController::FCameraCalibrationStepsController(TWeakPtr<FCameraCalibrationToolkit> InCameraCalibrationToolkit, ULensFile* InLensFile)
	: CameraCalibrationToolkit(InCameraCalibrationToolkit)
	, LensFile(TWeakObjectPtr<ULensFile>(InLensFile))
{
	check(CameraCalibrationToolkit.IsValid());
	check(LensFile.IsValid());

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FCameraCalibrationStepsController::OnTick), 0.0f);
}

FCameraCalibrationStepsController::~FCameraCalibrationStepsController()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}

	Cleanup();
}

void FCameraCalibrationStepsController::CreateSteps()
{
	// Ask subsystem for the registered calibration steps.

	const UCameraCalibrationSubsystem* Subsystem = GEngine->GetEngineSubsystem<UCameraCalibrationSubsystem>();

	if (!Subsystem)
	{
		UE_LOG(LogCameraCalibrationEditor, Error, TEXT("Could not find UCameraCalibrationSubsystem"));
		return;
	}

	const TArray<FName> StepNames = Subsystem->GetCameraCalibrationSteps();

	// Create the steps

	for (const FName& StepName : StepNames)
	{
		const TSubclassOf<UCameraCalibrationStep> StepClass = Subsystem->GetCameraCalibrationStep(StepName);

		UCameraCalibrationStep* const Step = NewObject<UCameraCalibrationStep>(
			GetTransientPackage(),
			StepClass,
			MakeUniqueObjectName(GetTransientPackage(), StepClass),
			RF_Transactional);

		check(Step);

		Step->Initialize(SharedThis(this));

		CalibrationSteps.Add(TStrongObjectPtr<UCameraCalibrationStep>(Step));
	}

	// Sort them according to prerequisites
	//
	// We iterate from the left and move to the right-most existing prerequisite, leaving a null behind
	// At the end we remove all the nulls that were left behind.

	for (int32 StepIdx = 0; StepIdx < CalibrationSteps.Num(); ++StepIdx)
	{
		int32 InsertIdx = StepIdx;

		for (int32 PrereqIdx = StepIdx+1; PrereqIdx < CalibrationSteps.Num(); ++PrereqIdx)
		{
			if (CalibrationSteps[StepIdx]->DependsOnStep(CalibrationSteps[PrereqIdx].Get()))
			{
				InsertIdx = PrereqIdx + 1;
			}
		}

		if (InsertIdx != StepIdx)
		{
			const TStrongObjectPtr<UCameraCalibrationStep> DependentStep = CalibrationSteps[StepIdx];
			CalibrationSteps.Insert(DependentStep, InsertIdx);

			// Invalidate the pointer left behind. This entry will be removed a bit later.
			CalibrationSteps[StepIdx].Reset();
		}
	}

	// Remove the nulled out ones

	for (int32 StepIdx = 0; StepIdx < CalibrationSteps.Num(); ++StepIdx)
	{
		if (!CalibrationSteps[StepIdx].IsValid())
		{
			CalibrationSteps.RemoveAt(StepIdx);
			StepIdx--;
		}
	}
}

TSharedPtr<SWidget> FCameraCalibrationStepsController::BuildUI()
{
	return SNew(SCameraCalibrationSteps, SharedThis(this));
}

bool FCameraCalibrationStepsController::OnTick(float DeltaTime)
{
	// Update the lens file evaluation inputs
	LensFileEvaluationInputs.bIsValid = false;
	if (const ULensComponent* const LensComponent = FindLensComponent())
	{
		LensFileEvaluationInputs = LensComponent->GetLensFileEvaluationInputs();
	}

	// Update the output resolution of the comp to match the resolution of the media source (if it has changed)
	const bool bCompResized = UpdateCompResolution();

	// Update the output resolution of the CG Layer if either the Comp was resized or if the camera's aspect ratio has changed
	UpdateCGResolution(bCompResized);

	// Tick each of the calibration steps
	for (TStrongObjectPtr<UCameraCalibrationStep>& Step : CalibrationSteps)
	{
		if (Step.IsValid())
		{
			Step->Tick(DeltaTime);
		}
	}

	return true;
}

bool FCameraCalibrationStepsController::UpdateCompResolution()
{
	const UMediaPlayer* const MediaPlayerPtr = MediaPlayer.Get();

	if (!MediaPlayerPtr)
	{
		return false;
	}

	const FIntPoint MediaDimensions = MediaPlayerPtr->GetVideoTrackDimensions(INDEX_NONE, INDEX_NONE);
	const FIntPoint CompRenderResolution = GetCompRenderResolution();

	bool bCompResized = false;

	// If no track was found, the dimensions might be (0, 0)
	if (MediaDimensions.X != 0 && MediaDimensions.Y != 0)
	{
		if (MediaDimensions != CompRenderResolution)
		{
			// Resize the output resolution of the top-level comp to match the incoming media dimensions
			if (ACompositingElement* CompPtr = Comp.Get())
			{
				Comp->SetRenderResolution(MediaDimensions);
			}

			// Resize the output render targets to match the incoming media dimensions
			if (UTextureRenderTarget2D* RenderTargetPtr = RenderTarget.Get())
			{
				RenderTargetPtr->ResizeTarget(MediaDimensions.X, MediaDimensions.Y);
			}

			if (UTextureRenderTarget2D* MediaPlateRenderTargetPtr = MediaPlateRenderTarget.Get())
			{
				MediaPlateRenderTargetPtr->ResizeTarget(MediaDimensions.X, MediaDimensions.Y);
			}

			if (UTextureRenderTarget2D* ToolOverlayRenderTargetPtr = ToolOverlayRenderTarget.Get())
			{
				ToolOverlayRenderTargetPtr->ResizeTarget(MediaDimensions.X, MediaDimensions.Y);
			}

			if (UTextureRenderTarget2D* UserOverlayRenderTargetPtr = UserOverlayRenderTarget.Get())
			{
				UserOverlayRenderTargetPtr->ResizeTarget(MediaDimensions.X, MediaDimensions.Y);
			}

			bCompResized = true;
		}
	}

	// Update the LensFile SimulcamInfo if the Media Dimensions have changed
	if (ULensFile* const LensFilePtr = GetLensFile())
	{
		if (LensFilePtr->SimulcamInfo.MediaResolution != MediaDimensions)
		{
			LensFilePtr->SimulcamInfo.MediaResolution = MediaDimensions;
			LensFilePtr->SimulcamInfo.MediaPlateAspectRatio = (MediaDimensions.Y != 0) ? (float)MediaDimensions.X / (float)MediaDimensions.Y : 0.0f;
		}
	}

	return bCompResized;
}

void FCameraCalibrationStepsController::UpdateCGResolution(bool bCompResized)
{
	const UCineCameraComponent* const CineCameraComponentPtr = CineCameraComponent.Get();
	const ACompositingElement* const CompPtr = Comp.Get();
	ACompositingElement* const CGLayerPtr = CGLayer.Get();
	ULensFile* const LensFilePtr = GetLensFile();

	if (!CineCameraComponentPtr || !CompPtr || !CGLayerPtr || !LensFilePtr)
	{
		return;
	}

	// There are two cases where the CG Layer's output resolution might need to be resized:
	// 1) If the aspect ratio of the camera source's filmback changed, to avoid any stretching of the CG
	// 2) If the Comp's output resolution changed, to ensure that the CG layer's resolution is not larger than the Comp, and not unnecessarily smaller

	const FIntPoint CGResolution = CGLayerPtr->GetRenderResolution();
	const float CGLayerAspectRatio = (CGResolution.Y != 0) ? (float)CGResolution.X / (float)CGResolution.Y : 0.0f;
	const float FilmbackAspectRatio = CineCameraComponentPtr->AspectRatio;

	// Early-out if the filmback is somehow 0.0f (which shouldn't be possible)
	if (FMath::IsNearlyEqual(FilmbackAspectRatio, 0.0f))
	{
		return;
	}

	constexpr float Tolerance = 0.01f;
	const bool bFilmbackAspectRatioChanged = !FMath::IsNearlyEqual(FilmbackAspectRatio, CGLayerAspectRatio, Tolerance);

	if (bFilmbackAspectRatioChanged || bCompResized)
	{
		const FIntPoint CompResolution = GetCompRenderResolution();
		const float CompAspectRatio = (CompResolution.Y != 0) ? (float)CompResolution.X / (float)CompResolution.Y : 0.0f;

		// Compute the camera feed dimensions that would fit the aspect ratio of the camera within the comp
		// If the filmback aspect ratio is wider than the comp, then the width of the camera feed will equal the width of the comp, and the height will be scaled
		// If the filmback aspect ratio is narrower than the comp, then the height of the camera feed will equal the height of the comp, and the width will be scaled

		FIntPoint NewCGResolution = CompResolution;

		if (FilmbackAspectRatio > CompAspectRatio)
		{
			NewCGResolution.Y = CompResolution.X / FilmbackAspectRatio;
		}
		else if (CompAspectRatio > FilmbackAspectRatio)
		{
			NewCGResolution.X = CompResolution.Y * FilmbackAspectRatio;
		}

		CGLayerPtr->SetRenderResolution(NewCGResolution);

		LensFilePtr->SimulcamInfo.CGLayerAspectRatio = FilmbackAspectRatio;

		if (!LensFilePtr->CameraFeedInfo.IsOverridden())
		{
			constexpr bool bMarkAsOverridden = false;
			LensFilePtr->CameraFeedInfo.SetDimensions(NewCGResolution, bMarkAsOverridden);
		}

		UpdateAspectRatioCorrection();
	}
}

void FCameraCalibrationStepsController::SetCameraFeedDimensionsFromMousePosition(FVector2D MousePosition)
{
	// Compute the size of the camera feed, using the MousePosition as one of its corners.
	// We assume that the camera feed will always be centered in the media. If that assumption proves false, this math should be updated in the future.
	const FIntPoint CompRenderResolution = GetCompRenderResolution();

	FIntPoint CameraFeedDimensions = FIntPoint(0, 0);
	CameraFeedDimensions.X = FMath::Abs((CompRenderResolution.X / 2) - FMath::Floor(MousePosition.X)) * 2;
	CameraFeedDimensions.Y = FMath::Abs((CompRenderResolution.Y / 2) - FMath::Floor(MousePosition.Y)) * 2;

	// Early-out if the dimensions are invalid
	if ((CameraFeedDimensions.X == 0) || (CameraFeedDimensions.Y == 0))
	{
		return;
	}

	if (ULensFile* const LensFilePtr = GetLensFile())
	{
		// Compare the aspect ratio of the selected camera to the cg layer's aspect ratio to determine if the cg layer needs to be resized
		if (CineCameraComponent.IsValid())
		{
			const float CameraFeedAspectRatio = CameraFeedDimensions.X / (float)CameraFeedDimensions.Y;
			const float CameraAspectRatio = CineCameraComponent->AspectRatio;

			// If the two aspect ratios are within the acceptable tolerance, attempt to minimize the aspect ratio difference by adjusting the camera feed dimensions
			constexpr float AspectRatioNudgeTolerance = 0.1f;
			if (FMath::IsNearlyEqual(CameraAspectRatio, CameraFeedAspectRatio, AspectRatioNudgeTolerance))
			{
				MinimizeAspectRatioError(CameraFeedDimensions, CameraAspectRatio);
			}

			// Update the camera feed dimensions and mark as overridden because it was changed as the result of user interaction
			constexpr bool bMarkAsOverridden = true;
			LensFilePtr->CameraFeedInfo.SetDimensions(CameraFeedDimensions, bMarkAsOverridden);

			UpdateAspectRatioCorrection();
		}
	}
}

void FCameraCalibrationStepsController::SetCameraFeedDimensions(FIntPoint Dimensions, bool bMarkAsOverridden)
{
	if (ULensFile* const LensFilePtr = GetLensFile())
	{
		LensFile->CameraFeedInfo.SetDimensions(Dimensions, bMarkAsOverridden);
		UpdateAspectRatioCorrection();
	}
}

void FCameraCalibrationStepsController::UpdateAspectRatioCorrection()
{
	const ULensFile* const LensFilePtr = LensFile.Get();
	const ACompositingElement* const CompPtr = Comp.Get();
	UCompositingElementMaterialPass* const MaterialPassPtr = MaterialPass.Get();

	if (!MaterialPassPtr || !CompPtr || !LensFilePtr)
	{
		return;
	}

	float AspectRatioCorrectionX = 1.0f;
	float AspectRatioCorrectionY = 1.0f;

	// If the camera feed dimensions are valid, use them and the dimensions of the media to set the aspect ratio correction material parameters
	if (LensFilePtr->CameraFeedInfo.IsValid())
	{
		const FIntPoint CompRenderResolution = CompPtr->GetRenderResolution();
		const FIntPoint CameraFeedDimensions = LensFilePtr->CameraFeedInfo.GetDimensions();

		AspectRatioCorrectionX = CompRenderResolution.X / (float)CameraFeedDimensions.X;
		AspectRatioCorrectionY = CompRenderResolution.Y / (float)CameraFeedDimensions.Y;
	}

	// If the camera feed dimensions are invalid, reset the correction parameters
	MaterialPassPtr->Material.SetScalarOverride(TEXT("AspectRatioCorrection_H"), AspectRatioCorrectionX);
	MaterialPassPtr->Material.SetScalarOverride(TEXT("AspectRatioCorrection_V"), AspectRatioCorrectionY);
}

void FCameraCalibrationStepsController::MinimizeAspectRatioError(FIntPoint& CameraFeedDimensions, float CameraAspectRatio)
{
	float CameraFeedAspectRatio = (CameraFeedDimensions.Y > 0) ? CameraFeedDimensions.X / (float)CameraFeedDimensions.Y : 0.0f;

	// Track the number of iterations to ensure that the minimization does not continue infinitely
	int32 NumIterations = 0;
	constexpr int32 MaxIterations = 20;

	constexpr float AspectRatioErrorTolerance = 0.001f;
	while ((!FMath::IsNearlyEqual(CameraAspectRatio, CameraFeedAspectRatio, AspectRatioErrorTolerance)) && NumIterations < MaxIterations)
	{
		// The camera feed needs to be wider and/or shorter
		if (CameraFeedAspectRatio < CameraAspectRatio)
		{
			// Use modulus to determine whether to alter the width or height each iteration
			if (NumIterations % 2 == 0)
			{
				CameraFeedDimensions.X += 2;
			}
			else
			{
				CameraFeedDimensions.Y -= 2;
			}
		}
		else // The camera feed needs to be narrower or taller
		{
			// Use modulus to determine whether to change the width or height each iteration
			if (NumIterations % 2 == 0)
			{
				CameraFeedDimensions.X -= 2;
			}
			else
			{
				CameraFeedDimensions.Y += 2;
			}
		}

		CameraFeedAspectRatio = (CameraFeedDimensions.Y > 0) ? CameraFeedDimensions.X / (float)CameraFeedDimensions.Y : 0.0f;

		NumIterations++;
	}
}

TWeakObjectPtr<ACompositingElement> FCameraCalibrationStepsController::AddElement(ACompositingElement* Parent, FString& ClassPath, FString& ElementName) const
{
	UClass* ElementClass = StaticLoadClass(ACompositingElement::StaticClass(), nullptr, *ClassPath, nullptr, LOAD_None, nullptr);

	if (!ElementClass)
	{
		return nullptr;
	}

	ICompElementManager* CompElementManager = CameraCalibrationStepsController::GetCompElementManager();

	if (!CompElementManager)
	{
		return nullptr;
	}

	TWeakObjectPtr<ACompositingElement> Element = CompElementManager->CreateElement(
		*ElementName, ElementClass, nullptr, EObjectFlags::RF_Transient | EObjectFlags::RF_DuplicateTransient);

	if (!Element.IsValid())
	{
		return nullptr;
	}

	if (Parent)
	{
		// Attach layer to parent
		Parent->AttachAsChildLayer(Element.Get());

		// Place element under Parent to keep things organized.
		Element->AttachToActor(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	return Element;
}


ACompositingElement* FCameraCalibrationStepsController::FindElement(const FString& Name) const
{
	// The motivation of finding them is if the level was saved with these objects.
	// A side effect is that we'll destroy them when closing the calibrator.

	const EActorIteratorFlags Flags = EActorIteratorFlags::SkipPendingKill;

	for (TActorIterator<AActor> It(GetWorld(), ACompositingElement::StaticClass(), Flags); It; ++It)
	{
		if (It->GetName() == Name)
		{
			return CastChecked<ACompositingElement>(*It);
		}
	}

	return nullptr;
}

void FCameraCalibrationStepsController::Cleanup()
{
	for (TStrongObjectPtr<UCameraCalibrationStep>& Step : CalibrationSteps)
	{
		if (Step.IsValid())
		{
			Step->Shutdown();
			Step.Reset();
		}
	}

	if (MediaPlayer.IsValid())
	{
		MediaPlayer->Close();
		MediaPlayer.Reset();
	}

	if (Comp.IsValid()) // It may have been destroyed manually by the user.
	{
		Comp->Destroy();
		Comp.Reset();
	}

	if (CGLayer.IsValid())
	{
		CGLayer->Destroy();
		CGLayer.Reset();
	}

	if (MediaPlate.IsValid())
	{
		MediaPlate->Destroy();
		MediaPlate.Reset();
	}

	MaterialPass.Reset();
	Camera.Reset();

	MediaPlateRenderTarget.Reset();
}

FString FCameraCalibrationStepsController::NamespacedName(const FString&& Name) const
{
	return FString::Printf(TEXT("CameraCalib_%s_%s"), *LensFile->GetName(), *Name);
}

UWorld* FCameraCalibrationStepsController::GetWorld() const
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

UTextureRenderTarget2D* FCameraCalibrationStepsController::GetRenderTarget() const
{
	return RenderTarget.Get();
}

FIntPoint FCameraCalibrationStepsController::GetCompRenderResolution() const
{
	if (ACompositingElement* CompPtr = Comp.Get())
	{
		return CompPtr->GetRenderResolution();
	}
	return FIntPoint(0, 0);
}

FIntPoint FCameraCalibrationStepsController::GetCGRenderResolution() const
{
	if (ACompositingElement* CGLayerPtr = CGLayer.Get())
	{
		return CGLayerPtr->GetRenderResolution();
	}
	return FIntPoint(0, 0);
}

FIntPoint FCameraCalibrationStepsController::GetCameraFeedSize() const
{
	if (ULensFile* LensFilePtr = GetLensFile())
	{
		return LensFilePtr->CameraFeedInfo.GetDimensions();
	}

	return FIntPoint(0, 0);
}

void FCameraCalibrationStepsController::CreateComp()
{
	// Don't do anything if we already created it.
	if (Comp.IsValid())
	{
		// Some items are exposed in the World Outliner, and they will get cleaned up and re-created
		// when the configurator is closed and re-opened.

		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Tried to create Comp that already existed"));
		return;
	}

	ICompElementManager* CompElementManager = CameraCalibrationStepsController::GetCompElementManager();

	if (!CompElementManager)
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Could not find ICompElementManager"));
		return;
	}

	// Create Comp parent
	{
		const FString CompName = NamespacedName(TEXT("Comp"));

		Comp = FindElement(CompName);

		if (!Comp.IsValid())
		{
			Comp = CompElementManager->CreateElement(
				*CompName, ACompositingElement::StaticClass(), nullptr, EObjectFlags::RF_Transient | EObjectFlags::RF_DuplicateTransient);
		}

		if (!Comp.IsValid())
		{
			UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to create 'Comp'"));
			Cleanup();
			return;
		}
	}

	// Convenience function to add comp elements
	auto FindOrAddElementToComp = [&](
		TWeakObjectPtr<ACompositingElement>& Element,
		FString&& ElementClassPath,
		FString&& ElementName) -> bool
	{
		if (Element.IsValid())
		{
			return true;
		}

		Element = FindElement(ElementName);

		if (!Element.IsValid())
		{
			Element = AddElement(Comp.Get(), ElementClassPath, ElementName);
		}

		return Element.IsValid();
	};

	if (!FindOrAddElementToComp(
		CGLayer,
		TEXT("/Composure/Blueprints/CompositingElements/BP_CgCaptureCompElement.BP_CgCaptureCompElement_C"),
		NamespacedName(TEXT("CG"))))
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to create 'CG' Layer"));
		Cleanup();
		return;
	}

	if (!FindOrAddElementToComp(
		MediaPlate,
		TEXT("/Composure/Blueprints/CompositingElements/BP_MediaPlateCompElement.BP_MediaPlateCompElement_C"),
		NamespacedName(TEXT("MediaPlate"))))
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to create 'MediaPlate'"));
		Cleanup();
		return;
	}

	// The CG Layer's resolution will be tied to the aspect ratio of the camera's filmback, not the resolution of the top-level comp
	CGLayer->ResolutionSource = EInheritedSourceType::Override;

	// This updates the Composure panel view
	CompElementManager->RefreshElementsList();

	// Disable fog on scene capture component of CGLayer
	{
		TArray<USceneCaptureComponent2D*> CaptureComponents;
		CGLayer->GetComponents(CaptureComponents);

		for (USceneCaptureComponent2D* CaptureComponent : CaptureComponents)
		{
			// We need to disable Fog via the ShowFlagSettings property instead of directly in ShowFlags,
			// because it will likely be overwritten since they are often replaced by the archetype defaults
			// and then updated by the ShowFlagSettings.

			FEngineShowFlagsSetting NewFlagSetting;
			NewFlagSetting.ShowFlagName = FEngineShowFlags::FindNameByIndex(FEngineShowFlags::EShowFlag::SF_Fog);
			NewFlagSetting.Enabled = false;

			CaptureComponent->ShowFlagSettings.Add(NewFlagSetting);

			if (FProperty* ShowFlagSettingsProperty = CaptureComponent->GetClass()->FindPropertyByName(
				GET_MEMBER_NAME_CHECKED(USceneCaptureComponent2D, ShowFlagSettings)))
			{
				// This PostEditChange will ensure that ShowFlags is updated.
				FPropertyChangedEvent PropertyChangedEvent(ShowFlagSettingsProperty);
				CaptureComponent->PostEditChangeProperty(PropertyChangedEvent);
			}
		}
	}

	// Create media player and media texture and assign it to the media input of the media plate layer.
	//
	for (UCompositingElementInput* Input : MediaPlate->GetInputsList())
	{
		UMediaTextureCompositingInput* MediaInput = Cast<UMediaTextureCompositingInput>(Input);

		if (!MediaInput)
		{
			continue;
		}

		// Create MediaPlayer

		// Using a strong reference prevents the MediaPlayer from going stale when the level is saved.
		MediaPlayer = TStrongObjectPtr<UMediaPlayer>(NewObject<UMediaPlayer>(
			GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UMediaPlayer::StaticClass())
			));

		if (!MediaPlayer.IsValid())
		{
			UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to create MediaPlayer"));
			Cleanup();
			return;
		}

		MediaPlayer->PlayOnOpen = true;
		MediaPlayer->SetLooping(true);
		// Create MediaTexture

		MediaTexture = NewObject<UMediaTexture>(GetTransientPackage(), NAME_None, RF_Transient);

		if (MediaTexture.IsValid())
		{
			MediaTexture->AutoClear = true;
			MediaTexture->SetMediaPlayer(MediaPlayer.Get());
			MediaTexture->UpdateResource();

			MediaInput->MediaSource = MediaTexture.Get();
		}

		// Play the media source, preferring time-synchronizable sources.
		if (const UMediaProfile* MediaProfile = IMediaProfileManager::Get().GetCurrentMediaProfile())
		{
			bool bFoundPreferredMediaSource = false;

			for (int32 MediaSourceIdx = 0; MediaSourceIdx < MediaProfile->NumMediaSources(); ++MediaSourceIdx)
			{
				UMediaSource* MediaSource = MediaProfile->GetMediaSource(MediaSourceIdx);

				if (Cast<UTimeSynchronizableMediaSource>(MediaSource))
				{
					MediaPlayer->OpenSource(MediaSource);
					MediaPlayer->Play();

					bFoundPreferredMediaSource = true;

					// Break since we don't need to look for more MediaSources.
					break;
				}
			}

			if (!bFoundPreferredMediaSource)
			{
				for (int32 MediaSourceIdx = 0; MediaSourceIdx < MediaProfile->NumMediaSources(); ++MediaSourceIdx)
				{
					if (UMediaSource* MediaSource = MediaProfile->GetMediaSource(MediaSourceIdx))
					{
						MediaPlayer->OpenSource(MediaSource);
						MediaPlayer->Play();

						// Break since we don't need to look for more MediaSources.
						break;
					}
				}
			}
		}

		// Break since don't need to look at more UMediaTextureCompositingInputs.
		break;
	}

	// Create material pass that blends MediaPlate with CG.
	//@todo Make this material selectable by the user.

	MaterialPass = CastChecked<UCompositingElementMaterialPass>(
		Comp->CreateNewTransformPass(TEXT("CGOverMedia"), UCompositingElementMaterialPass::StaticClass())
		);

	if (!MaterialPass.IsValid())
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to create 'CGOverMedia' UCompositingElementMaterialPass"));
		Cleanup();
		return;
	}

	// Create Material
	const FString MaterialPath = TEXT("/CameraCalibration/Materials/M_SimulcamCalib.M_SimulcamCalib");
	UMaterial* Material = Cast<UMaterial>(FSoftObjectPath(MaterialPath).TryLoad());

	if (!Material)
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to load %s"), *MaterialPath);
		Cleanup();
		return;
	}

	MaterialPass->SetMaterialInterface(Material);

	// ActorLabel should coincide with ElementName. Name may not.
	MaterialPass->SetParameterMapping(TEXT("CG"), *CGLayer->GetActorLabel());
	MaterialPass->SetParameterMapping(TEXT("MediaPlate"), *MediaPlate->GetActorLabel());

	// Create new overlay transform passes
	CreateOverlayPass(TEXT("Calibration Step Overlay"), ToolOverlayPass, ToolOverlayRenderTarget);
	CreateOverlayPass(TEXT("User Selected Overlay"), UserOverlayPass, UserOverlayRenderTarget);

	const FIntPoint InitialCompRenderResolution = Comp->GetRenderResolution();

	if (ULensFile* LensFilePtr = LensFile.Get())
	{
		// Initialize the simulcam info of the LensFile to match the initial render resolution of the comp
		const float CompAspectRatio = (InitialCompRenderResolution.Y != 0) ? InitialCompRenderResolution.X / (float)InitialCompRenderResolution.Y : 1.0f;
		LensFilePtr->SimulcamInfo.CGLayerAspectRatio = CompAspectRatio;
		LensFilePtr->SimulcamInfo.MediaPlateAspectRatio = CompAspectRatio;

		// If the Camera Feed is not specifically overriden by the user, initialize it to the initial dimensions of the comp as well
		// If the camera feed exactly matches the comp's resolution, then no aspect ratio correction is needed
		if (!LensFilePtr->CameraFeedInfo.IsOverridden())
		{
			LensFilePtr->CameraFeedInfo.SetDimensions(InitialCompRenderResolution);
		}
		else
		{
			// If the Camera Feed has been overridden by the user, update the aspect ratio correction material parameters
			UpdateAspectRatioCorrection();
		}
	}

	URenderTargetCompositingOutput* RTOutput = Cast<URenderTargetCompositingOutput>(Comp->CreateNewOutputPass(
		TEXT("SimulcamCalOutput"),
		URenderTargetCompositingOutput::StaticClass())
		);

	if (!RTOutput)
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to create URenderTargetCompositingOutput"));
		Cleanup();
		return;
	}

	// Create RenderTarget
	RenderTarget = NewObject<UTextureRenderTarget2D>(
		GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UTextureRenderTarget2D::StaticClass())
		);

	if (!RenderTarget.IsValid())
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to create UTextureRenderTarget2D"));
		Cleanup();
		return;
	}

	RenderTarget->RenderTargetFormat = RTF_RGBA16f;
	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->InitAutoFormat(InitialCompRenderResolution.X, InitialCompRenderResolution.Y);
	RenderTarget->UpdateResourceImmediate(true);

	// Assign the RT to the compositing output
	RTOutput->RenderTarget = RenderTarget.Get();

	// By default, use a camera with our lens. If not found, let composure pick one.
	if (ACameraActor* CameraGuess = FindFirstCameraWithCurrentLens())
	{
		SetCamera(CameraGuess);
	}
	else
	{
		SetCamera(Comp->FindTargetCamera());
	}
}

UCompositingElementMaterialPass* FCameraCalibrationStepsController::GetOverlayMaterialPass(EOverlayPassType OverlayPassType) const
{
	if (OverlayPassType == EOverlayPassType::ToolOverlay)
	{
		return ToolOverlayPass.Get();
	}
	else if (OverlayPassType == EOverlayPassType::UserOverlay)
	{
		return UserOverlayPass.Get();
	}

	return nullptr;
}

UTextureRenderTarget2D* FCameraCalibrationStepsController::GetOverlayRenderTarget(EOverlayPassType OverlayPassType) const
{
	if (OverlayPassType == EOverlayPassType::ToolOverlay)
	{
		return ToolOverlayRenderTarget.Get();
	}
	else if (OverlayPassType == EOverlayPassType::UserOverlay)
	{
		return UserOverlayRenderTarget.Get();
	}

	return nullptr;
}

UMaterialInterface* FCameraCalibrationStepsController::GetOverlayMaterial(EOverlayPassType OverlayPassType) const
{
	if (OverlayPassType == EOverlayPassType::ToolOverlay)
	{
		return ToolOverlayMaterial.Get();
	}
	else if (OverlayPassType == EOverlayPassType::UserOverlay)
	{
		return UserOverlayMaterial.Get();
	}

	return nullptr;
}

bool FCameraCalibrationStepsController::IsOverlayEnabled(EOverlayPassType OverlayPassType) const
{
	if (UCompositingElementMaterialPass* OverlayPass = GetOverlayMaterialPass(OverlayPassType))
	{
		return OverlayPass->IsPassEnabled();
	}

	return false;
}

void FCameraCalibrationStepsController::SetOverlayEnabled(const bool bEnabled, EOverlayPassType OverlayPassType)
{
	if (UCompositingElementMaterialPass* OverlayPass = GetOverlayMaterialPass(OverlayPassType))
	{
		OverlayPass->SetPassEnabled(bEnabled);
	}
}

void FCameraCalibrationStepsController::SetOverlayMaterial(UMaterialInterface* InOverlay, bool bShowOverlay, EOverlayPassType OverlayPassType)
{
 	if (OverlayPassType == EOverlayPassType::ToolOverlay)
 	{
		ToolOverlayMaterial = InOverlay;
	}
 	else if (OverlayPassType == EOverlayPassType::UserOverlay)
 	{
		UserOverlayMaterial = InOverlay;
	}

	// If the overlay is non-null, refresh the overlay and enable/disable the overlay as necessary
	if (InOverlay)
	{
		RefreshOverlay(OverlayPassType);
		SetOverlayEnabled(bShowOverlay, OverlayPassType);
	}
	else
	{
		// Disable the overlay pass if the input overlay is null
		SetOverlayEnabled(false, OverlayPassType);
	}
}

void FCameraCalibrationStepsController::RefreshOverlay(EOverlayPassType OverlayPassType)
{
	if (UCompositingElementMaterialPass* OverlayPass = GetOverlayMaterialPass(OverlayPassType))
	{
		if (UTextureRenderTarget2D* OverlayRenderTarget = GetOverlayRenderTarget(OverlayPassType))
		{
			if (UMaterialInterface* OverlayMaterial = GetOverlayMaterial(OverlayPassType))
			{
				// Clear the overlay render target and draw to it again using the current overlay material
				UKismetRenderingLibrary::ClearRenderTarget2D(OverlayPass, OverlayRenderTarget, FLinearColor::Transparent);
				UKismetRenderingLibrary::DrawMaterialToRenderTarget(OverlayPass, OverlayRenderTarget, OverlayMaterial);
			}
		}
	}
}

void FCameraCalibrationStepsController::CreateMediaPlateOutput()
{
	if (!MediaPlate.IsValid())
	{
		return;
	}

	URenderTargetCompositingOutput* RTOutput = Cast<URenderTargetCompositingOutput>(MediaPlate->CreateNewOutputPass(
		TEXT("MediaPlateOutput"),
		URenderTargetCompositingOutput::StaticClass())
		);

	if (!RTOutput)
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to create URenderTargetCompositingOutput for the MediaPlateOutput"));
		Cleanup();
		return;
	}

	// Create RenderTarget
	MediaPlateRenderTarget = NewObject<UTextureRenderTarget2D>(
		GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UTextureRenderTarget2D::StaticClass())
		);

	if (!MediaPlateRenderTarget.IsValid())
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to create UTextureRenderTarget2D for the MediaPlateOutput"));
		Cleanup();
		return;
	}

	const FIntPoint InitialCompRenderResolution = GetCompRenderResolution();

	MediaPlateRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	MediaPlateRenderTarget->ClearColor = FLinearColor::Black;
	MediaPlateRenderTarget->bAutoGenerateMips = false;
	MediaPlateRenderTarget->InitAutoFormat(InitialCompRenderResolution.X, InitialCompRenderResolution.Y);
	MediaPlateRenderTarget->UpdateResourceImmediate(true);

	// Assign the RT to the compositing output
	RTOutput->RenderTarget = MediaPlateRenderTarget.Get();
}

void FCameraCalibrationStepsController::CreateOverlayPass(FName PassName, TWeakObjectPtr<UCompositingElementMaterialPass>& OverlayPass, TWeakObjectPtr<UTextureRenderTarget2D>& OverlayRenderTarget)
{
	// Create new overlay transform pass
	OverlayPass = CastChecked<UCompositingElementMaterialPass>(
		Comp->CreateNewTransformPass(PassName, UCompositingElementMaterialPass::StaticClass())
		);

	if (!OverlayPass.IsValid())
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to create 'Overlay' UCompositingElementMaterialPass"));
		Cleanup();
		return;
	}

	if (UMaterialInterface* OverlayBaseMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("/CameraCalibration/Materials/M_OverlayBase.M_OverlayBase"))))
	{
		OverlayPass->SetMaterialInterface(OverlayBaseMaterial);
	}

	OverlayPass->SetPassEnabled(false);

	OverlayRenderTarget = NewObject<UTextureRenderTarget2D>(
		GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UTextureRenderTarget2D::StaticClass())
		);

	if (!OverlayRenderTarget.IsValid())
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to create UTextureRenderTarget2D for the OverlayPass"));
		Cleanup();
		return;
	}

	const FIntPoint InitialCompRenderResolution = GetCompRenderResolution();

	OverlayRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	OverlayRenderTarget->ClearColor = FLinearColor::Transparent;
	OverlayRenderTarget->bAutoGenerateMips = false;
	OverlayRenderTarget->InitAutoFormat(InitialCompRenderResolution.X, InitialCompRenderResolution.Y);
	OverlayRenderTarget->UpdateResourceImmediate(true);

	OverlayPass->Material.SetTextureOverride(FName(TEXT("OverlayTexture")), OverlayRenderTarget.Get());
}

float FCameraCalibrationStepsController::GetWiperWeight() const
{
	float Weight = 0.5f;

	if (!MaterialPass.IsValid())
	{
		return Weight;
	}

	MaterialPass->Material.GetScalarOverride(TEXT("WiperWeight"), Weight);
	return Weight;
}

void FCameraCalibrationStepsController::SetWiperWeight(float InWeight)
{
	if (MaterialPass.IsValid())
	{
		MaterialPass->Material.SetScalarOverride(TEXT("WiperWeight"), InWeight);
	}
}

void FCameraCalibrationStepsController::SetCamera(ACameraActor* InCamera)
{
	Camera = InCamera;

	if (!InCamera)
	{
		return;
	}

	TInlineComponentArray<UCineCameraComponent*> CameraComponents;
	InCamera->GetComponents(CameraComponents);

	if (CameraComponents.Num() > 0)
	{
		CineCameraComponent = CameraComponents[0];
	}

	// Update the Comp with this camera
	if (Comp.IsValid())
	{
		Comp->SetTargetCamera(InCamera);
		EnableDistortionInCG();
	}
}

void FCameraCalibrationStepsController::EnableDistortionInCG()
{
	if (!Comp.IsValid())
	{
		return;
	}

	for (ACompositingElement* Element : Comp->GetChildElements())
	{
		ACompositingCaptureBase* CaptureBase = Cast<ACompositingCaptureBase>(Element);

		if (!CaptureBase)
		{
			continue;
		}

		// Set the LensComponent used by the CG layer to match the LensComponent in use by our target camera
		if (ACameraActor* const TargetCamera = Camera.Get())
		{
			if (ULensComponent* const TargetLens = FindLensComponentOnCamera(TargetCamera))
			{
				CaptureBase->SetLens(TargetLens);
			}
		}

		// Enable distortion on the CG compositing layer
		CaptureBase->SetApplyDistortion(true);
	}
}

ACameraActor* FCameraCalibrationStepsController::GetCamera() const
{
	return Camera.Get();
}

void FCameraCalibrationStepsController::OnSimulcamViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bool bStepHandled = false;

	for (TStrongObjectPtr<UCameraCalibrationStep>& Step : CalibrationSteps)
	{
		if (Step.IsValid() && Step->IsActive())
		{
			bStepHandled |= Step->OnViewportClicked(MyGeometry, MouseEvent);
			break;
		}
	}

	// If a step handled the event, we're done
	if (bStepHandled)
	{
		return;
	}

	// Toggle video pause with right click.
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		TogglePlay();
		return;
	}
}

bool FCameraCalibrationStepsController::OnSimulcamViewportInputKey(const FKey& InKey, const EInputEvent& InEvent)
{
	bool bStepHandled = false;

	for (TStrongObjectPtr<UCameraCalibrationStep>& Step : CalibrationSteps)
	{
		if (Step.IsValid() && Step->IsActive())
		{
			bStepHandled |= Step->OnViewportInputKey(InKey, InEvent);
			break;
		}
	}

	return bStepHandled;
}

FReply FCameraCalibrationStepsController::OnRewindButtonClicked()
{
	// Rewind to the beginning of the media
	if (MediaPlayer.IsValid())
	{
		MediaPlayer->Rewind();
	}

	return FReply::Handled();
}

FReply FCameraCalibrationStepsController::OnReverseButtonClicked()
{
	// Increase the reverse media playback rate
	if (MediaPlayer.IsValid())
	{
		MediaPlayer->SetRate(GetFasterReverseRate());
	}

	return FReply::Handled();
}

FReply FCameraCalibrationStepsController::OnStepBackButtonClicked() 
{
	if (MediaPlayer.IsValid())
	{
		const float DefaultStepRateInMilliseconds = GetDefault<UCameraCalibrationEditorSettings>()->DefaultMediaStepRateInMilliseconds;
		const bool bForceDefaultStepRate = GetDefault<UCameraCalibrationEditorSettings>()->bForceDefaultMediaStepRate;

		// The media player could return a frame rate of 0 for the current video track
		const float MediaFrameRate = MediaPlayer->GetVideoTrackFrameRate(INDEX_NONE, INDEX_NONE);

		// Use the default step rate if the media player returned an invalid frame rate or if the project settings force it
		float MillisecondsPerStep = 0.0f;
		if (FMath::IsNearlyEqual(MediaFrameRate, 0.0f) || bForceDefaultStepRate)
		{
			MillisecondsPerStep = DefaultStepRateInMilliseconds;
		}
		else
		{
			MillisecondsPerStep = 1000.0f / MediaFrameRate;
		}

		// Compute the number of ticks in one step and go backward from the media's current time (clamping to 0)
		const FTimespan TicksInOneStep = ETimespan::TicksPerMillisecond * (MillisecondsPerStep);
		FTimespan PreviousStepTime = MediaPlayer->GetTime() - TicksInOneStep;
		if (PreviousStepTime < FTimespan::Zero())
		{
			PreviousStepTime = FTimespan::Zero();
		}

		MediaPlayer->Seek(PreviousStepTime);
		MediaPlayer->Pause();
	}

	return FReply::Handled();
}

FReply FCameraCalibrationStepsController::OnPlayButtonClicked() 
{
	if (MediaPlayer.IsValid())
	{
		MediaPlayer->Play();
	}

	return FReply::Handled(); 
}

FReply FCameraCalibrationStepsController::OnPauseButtonClicked() 
{
	if (MediaPlayer.IsValid())
	{
		MediaPlayer->Pause();
	}

	return FReply::Handled(); 
}

FReply FCameraCalibrationStepsController::OnStepForwardButtonClicked()
{
	if (MediaPlayer.IsValid())
	{
		const float DefaultStepRateInMilliseconds = GetDefault<UCameraCalibrationEditorSettings>()->DefaultMediaStepRateInMilliseconds;
		const bool bForceDefaultStepRate = GetDefault<UCameraCalibrationEditorSettings>()->bForceDefaultMediaStepRate;

		// The media player could return a frame rate of 0 for the current video track
		const float MediaFrameRate = MediaPlayer->GetVideoTrackFrameRate(INDEX_NONE, INDEX_NONE);

		// Use the default step rate if the media player returned an invalid frame rate or if the project settings force it
		float MillisecondsPerStep = 0.0f;
		if (FMath::IsNearlyEqual(MediaFrameRate, 0.0f) || bForceDefaultStepRate)
		{
			MillisecondsPerStep = DefaultStepRateInMilliseconds;
		}
		else
		{
			MillisecondsPerStep = 1000.0f / MediaFrameRate;
		}


		// Compute the number of ticks in one step and go forward from the media's current time
		const FTimespan TicksInOneStep = ETimespan::TicksPerMillisecond * (MillisecondsPerStep);
		const FTimespan NextStepTime = MediaPlayer->GetTime() + TicksInOneStep;

		// Ensure that we do not attempt to seek past the end of the media
		const FTimespan Duration = MediaPlayer->GetDuration();
		if ((NextStepTime + TicksInOneStep) < Duration)
		{
			MediaPlayer->Seek(NextStepTime);
			MediaPlayer->Pause();
		}
	}

	return FReply::Handled();
}

FReply FCameraCalibrationStepsController::OnForwardButtonClicked() 
{
	// Increase the forward media playback rate
	if (MediaPlayer.IsValid())
	{
		MediaPlayer->SetRate(GetFasterForwardRate());
	}

	return FReply::Handled(); 
}

bool FCameraCalibrationStepsController::DoesMediaSupportSeeking() const
{
	if (MediaPlayer.IsValid())
	{
		return MediaPlayer->SupportsSeeking();
	}

	return false;
}

bool FCameraCalibrationStepsController::DoesMediaSupportNextReverseRate() const
{
	if (MediaPlayer.IsValid())
	{
		constexpr bool Unthinned = false;
		return MediaPlayer->SupportsRate(GetFasterReverseRate(), Unthinned);
	}

	return false;
}

bool FCameraCalibrationStepsController::DoesMediaSupportNextForwardRate() const
{
	if (MediaPlayer.IsValid())
	{
		constexpr bool Unthinned = false;
		return MediaPlayer->SupportsRate(GetFasterForwardRate(), Unthinned);
	}

	return false;
}

float FCameraCalibrationStepsController::GetFasterReverseRate() const
{
	if (MediaPlayer.IsValid())
	{
		// Get the current playback rate of the media player
		float Rate = MediaPlayer->GetRate();

		// Reverse the playback direction (if needed) to ensure the rate is going in reverse
		if (Rate > -1.0f)
		{
			return -1.0f;
		}

		// Double the reverse playback rate
		return 2.0f * Rate;
	}

	return -1.0f;
}

float FCameraCalibrationStepsController::GetFasterForwardRate() const
{
	if (MediaPlayer.IsValid())
	{
		// Get the current playback rate of the media player
		float Rate = MediaPlayer->GetRate();

		// Reverse the playback direction (if needed) to ensure the rate is going forward
		if (Rate < 1.0f)
		{
			Rate = 1.0f;
		}

		// Double the forward playback rate
		return 2.0f * Rate;
	}

	return 1.0f;
}

void FCameraCalibrationStepsController::ToggleShowMediaPlaybackControls()
{
	bShowMediaPlaybackButtons = !bShowMediaPlaybackButtons;
}

bool FCameraCalibrationStepsController::AreMediaPlaybackControlsVisible() const
{
	return bShowMediaPlaybackButtons;
}

ACameraActor* FCameraCalibrationStepsController::FindFirstCameraWithCurrentLens() const
{
	// We iterate over all cameras in the scene and try to find one that is using the current LensFile
	ACineCameraActor* FirstCamera = nullptr;
	for (TActorIterator<ACineCameraActor> CameraItr(GetWorld()); CameraItr; ++CameraItr)
	{
		ACineCameraActor* CameraActor = *CameraItr;

		if (ULensComponent* FoundLensComponent = FindLensComponentOnCamera(CameraActor))
		{
			if (FirstCamera == nullptr)
			{
				FirstCamera = CameraActor;
			}
			else
			{
				FText ErrorMessage = LOCTEXT("MoreThanOneCameraFoundError", "There are multiple cameras in the scene using this LensFile. When the asset editor opens, be sure to select the correct camera if not already selected.");
				FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
				break;
			}
		}
	}

	return FirstCamera;
}

void FCameraCalibrationStepsController::TogglePlay()
{
	if (!MediaPlayer.IsValid())
	{
		return;
	}

	//@todo Eventually pause should cache the texture instead of relying on player play/pause support.

	if (IsPaused())
	{
		MediaPlayer->Play();
	}
	else
	{
		// TODO: Trigger the current step (and ultimately the algo) to cache any data it cares about (like 3D scene data)
		MediaPlayer->Pause();
	}
}

void FCameraCalibrationStepsController::Play()
{
	if (!MediaPlayer.IsValid())
	{
		return;
	}

	MediaPlayer->Play();
}

void FCameraCalibrationStepsController::Pause()
{
	if (!MediaPlayer.IsValid())
	{
		return;
	}

	MediaPlayer->Pause();
}

bool FCameraCalibrationStepsController::IsPaused() const
{
	if (MediaPlayer.IsValid())
	{
		return MediaPlayer->IsPaused();
	}

	return true;
}

FLensFileEvaluationInputs FCameraCalibrationStepsController::GetLensFileEvaluationInputs() const
{
	return LensFileEvaluationInputs;
}

ULensFile* FCameraCalibrationStepsController::GetLensFile() const
{
	if (LensFile.IsValid())
	{
		return LensFile.Get();
	}

	return nullptr;
}

ULensComponent* FCameraCalibrationStepsController::FindLensComponentOnCamera(ACameraActor* CineCamera) const
{
	const ULensFile* OpenLensFile = GetLensFile();
	if (CineCamera && OpenLensFile)
	{
		TInlineComponentArray<ULensComponent*> LensComponents;
		CineCamera->GetComponents(LensComponents);

		for (ULensComponent* LensComponent : LensComponents)
		{
			if (LensComponent->GetLensFile() == OpenLensFile)
			{
				return LensComponent;
			}
		}
	}

	return nullptr;
}

ULensComponent* FCameraCalibrationStepsController::FindLensComponent() const
{
	return FindLensComponentOnCamera(GetCamera());
}

const ULensDistortionModelHandlerBase* FCameraCalibrationStepsController::GetDistortionHandler() const
{
	if (ULensComponent* LensComponent = FindLensComponent())
	{
		return LensComponent->GetLensDistortionHandler();
	}

	return nullptr;
}

bool FCameraCalibrationStepsController::SetMediaSourceUrl(const FString& InMediaSourceUrl)
{
	const UMediaProfile* MediaProfile = IMediaProfileManager::Get().GetCurrentMediaProfile();

	if (!MediaProfile || !MediaPlayer.IsValid())
	{
		return false;
	}

	// If we're already playing it, we're done
	if (InMediaSourceUrl == GetMediaSourceUrl())
	{
		return true;
	}

	if (InMediaSourceUrl == TEXT("None") || !InMediaSourceUrl.Len())
	{
		MediaPlayer->Close();
		return true;
	}

	for (int32 MediaSourceIdx = 0; MediaSourceIdx < MediaProfile->NumMediaSources(); ++MediaSourceIdx)
	{
		UMediaSource* MediaSource = MediaProfile->GetMediaSource(MediaSourceIdx);

		if (!MediaSource || (MediaSource->GetUrl() != InMediaSourceUrl))
		{
			continue;
		}

		MediaPlayer->OpenSource(MediaSource);
		MediaPlayer->Play();

		return true;
	}

	return false;
}

/** Gets the current media source url being played. Empty if None */
FString FCameraCalibrationStepsController::GetMediaSourceUrl() const
{
	if (!MediaPlayer.IsValid())
	{
		return TEXT("");
	}

	return MediaPlayer->GetUrl();
}

void FCameraCalibrationStepsController::FindMediaSourceUrls(TArray<TSharedPtr<FString>>& OutMediaSourceUrls) const
{
	const UMediaProfile* MediaProfile = IMediaProfileManager::Get().GetCurrentMediaProfile();

	if (!MediaProfile)
	{
		return;
	}

	for (int32 MediaSourceIdx = 0; MediaSourceIdx < MediaProfile->NumMediaSources(); ++MediaSourceIdx)
	{
		if (const UMediaSource* MediaSource = MediaProfile->GetMediaSource(MediaSourceIdx))
		{
			OutMediaSourceUrls.Add(MakeShared<FString>(MediaSource->GetUrl()));
		}
	}
}

const TConstArrayView<TStrongObjectPtr<UCameraCalibrationStep>> FCameraCalibrationStepsController::GetCalibrationSteps() const
{
	return TConstArrayView<TStrongObjectPtr<UCameraCalibrationStep>>(CalibrationSteps);
}

void FCameraCalibrationStepsController::SelectStep(const FName& Name)
{
	for (TStrongObjectPtr<UCameraCalibrationStep>& Step : CalibrationSteps)
	{
		if (Step.IsValid())
		{
			if (Name == Step->FriendlyName())
			{
				Step->Activate();

				// Switch the overlay material for the tool overlay pass to the material used by the step being selected (and enable if needed)
				SetOverlayMaterial(Step->GetOverlayMID(), Step->IsOverlayEnabled(), EOverlayPassType::ToolOverlay);
			}
			else
			{
				Step->Deactivate();
			}
		}
	}
}

void FCameraCalibrationStepsController::Initialize()
{
	// Not doing these in the constructor so that SharedThis can be used.

	CreateComp();
	CreateMediaPlateOutput();
	CreateSteps();
}

UTextureRenderTarget2D* FCameraCalibrationStepsController::GetMediaPlateRenderTarget() const
{
	return MediaPlateRenderTarget.Get();
}

bool FCameraCalibrationStepsController::CalculateNormalizedMouseClickPosition(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FVector2f& OutPosition, ESimulcamViewportPortion ViewportPortion) const
{
	// Reject viewports with no area
	if (FMath::IsNearlyZero(MyGeometry.Size.X) || FMath::IsNearlyZero(MyGeometry.Size.Y))
	{
		return false;
	}

	// About the Mouse Event data:
	// 
	// * MouseEvent.GetScreenSpacePosition(): Position in pixels on the screen (independent of window size of position)
	// * MyGeometry.Size                    : Size of viewport (the one with the media, not the whole window)
	// * MyGeometry.AbsolutePosition        : Position of the top-left corner of viewport within screen
	// * MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()) gives you the pixel coordinates local to the viewport.

	const FVector2f LocalInPixels = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	float XNormalized = LocalInPixels.X / MyGeometry.Size.X;
	float YNormalized = LocalInPixels.Y / MyGeometry.Size.Y;

	if (ViewportPortion == ESimulcamViewportPortion::CameraFeed)
	{
		ULensFile* LensFilePtr = GetLensFile();
		if (!LensFilePtr)
		{
			return false;
		}

		const FIntPoint CameraFeedDimensions = LensFilePtr->CameraFeedInfo.GetDimensions();
		const FIntPoint CompRenderResolution = GetCompRenderResolution();

		const float AspectRatioCorrectionX = CompRenderResolution.X / (float)CameraFeedDimensions.X;
		const float AspectRatioCorrectionY = CompRenderResolution.Y / (float)CameraFeedDimensions.Y;

		XNormalized = ((XNormalized - 0.5f) * AspectRatioCorrectionX) + 0.5f;
		YNormalized = ((YNormalized - 0.5f) * AspectRatioCorrectionY) + 0.5f;

		// If the scaled values for X or Y are outside the range of [0,1], then the position is invalid (not on the camera feed)
		if (XNormalized < 0.0f || XNormalized > 1.0f || YNormalized < 0.0f || YNormalized > 1.0f)
		{
			return false;
		}
	}

	// Position 0~1. Origin at top-left corner of the viewport.
	OutPosition = FVector2f(XNormalized, YNormalized);

	return true;
}

bool FCameraCalibrationStepsController::ReadMediaPixels(TArray<FColor>& Pixels, FIntPoint& Size, FText& OutErrorMessage, ESimulcamViewportPortion ViewportPortion) const
{
	// Get the media plate texture render target 2d

	if (!MediaPlateRenderTarget.IsValid())
	{
		OutErrorMessage = LOCTEXT("InvalidMediaPlateRenderTarget", "Invalid MediaPlateRenderTarget");
		return false;
	}

	// Extract its render target resource
	FRenderTarget* MediaRenderTarget = MediaPlateRenderTarget->GameThread_GetRenderTargetResource();

	if (!MediaRenderTarget)
	{
		OutErrorMessage = LOCTEXT("InvalidRenderTargetResource", "MediaPlateRenderTarget did not have a RenderTarget resource");
		return false;
	}

	if (MediaPlateRenderTarget->RenderTargetFormat != ETextureRenderTargetFormat::RTF_RGBA8)
	{
		OutErrorMessage = LOCTEXT("InvalidFormat", "MediaPlateRenderTarget did not have the expected RTF_RGBA8 format");
		return false;
	}

	// Read the pixels onto CPU
	TArray<FColor> MediaPixels;
	const bool bReadPixels = MediaRenderTarget->ReadPixels(MediaPixels);

	if (!bReadPixels)
	{
		OutErrorMessage = LOCTEXT("ReadPixelsFailed", "ReadPixels from render target failed");
		return false;
	}

	ULensFile* LensFilePtr = GetLensFile();
	if (!LensFilePtr)
	{
		OutErrorMessage = LOCTEXT("InvalidLensFile", "There was no LensFile found.");
		return false;
	}

	if ((ViewportPortion == ESimulcamViewportPortion::CameraFeed) && LensFilePtr->CameraFeedInfo.IsValid())
	{
		Size = LensFilePtr->CameraFeedInfo.GetDimensions();
		Pixels.SetNumUninitialized(Size.X * Size.Y);

		const FIntPoint MediaResolution = MediaRenderTarget->GetSizeXY();
		const FIntPoint MediaCenterPoint = MediaResolution / 2;
		const float AspectRatioCorrectionX = MediaResolution.X / (float)Size.X;
		const float AspectRatioCorrectionY = MediaResolution.Y / (float)Size.Y;

		// Only return pixels from the actual camera feed (which may be smaller than the full media render target)
		for (int32 YCoordinate = 0; YCoordinate < Size.Y; YCoordinate++)
		{
			for (int32 XCoordinate = 0; XCoordinate < Size.X; XCoordinate++)
			{
				const int32 ScaledX = (((XCoordinate * AspectRatioCorrectionX) - MediaCenterPoint.X) / AspectRatioCorrectionX) + MediaCenterPoint.X;
				const int32 ScaledY = (((YCoordinate * AspectRatioCorrectionY) - MediaCenterPoint.Y) / AspectRatioCorrectionY) + MediaCenterPoint.Y;

				Pixels[YCoordinate * Size.X + XCoordinate] = MediaPixels[ScaledY * MediaResolution.X + ScaledX];
			}
		}
	}
	else
	{
		Pixels = MoveTemp(MediaPixels);
		Size = MediaRenderTarget->GetSizeXY();
	}

	check(Pixels.Num() == Size.X * Size.Y);

	return true;
}

#undef LOCTEXT_NAMESPACE

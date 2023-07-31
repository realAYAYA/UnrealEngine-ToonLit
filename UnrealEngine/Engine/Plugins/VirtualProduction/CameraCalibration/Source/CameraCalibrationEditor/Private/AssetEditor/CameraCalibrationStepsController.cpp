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
#include "TimeSynchronizableMediaSource.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWidget.h"


#if WITH_OPENCV
#include "OpenCVHelper.h"
#endif


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
	, RenderTargetSize(FIntPoint(1920, 1080))
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
	// Update the lens file eval data
	LensFileEvaluationInputs.bIsValid = false;
	if (const ULensComponent* const LensComponent = FindLensComponent())
	{
		LensFileEvaluationInputs = LensComponent->GetLensFileEvaluationInputs();
	}

	// Compare the current dimensions of the playing media track to the comp's render resolution to determine if the comp needs to be resized
	if (MediaPlayer.IsValid())
	{
		const FIntPoint MediaDimensions = MediaPlayer->GetVideoTrackDimensions(INDEX_NONE, INDEX_NONE);

		// If no track was found, the dimensions might be (0, 0)
		if (MediaDimensions.X != 0 && MediaDimensions.Y != 0)
		{
			if (RenderTarget->SizeX != MediaDimensions.X || RenderTarget->SizeY != MediaDimensions.Y)
			{
				// Resize the media plate comp layer and its output render target to match the incoming media dimensions
				MediaPlateRenderTarget->ResizeTarget(MediaDimensions.X, MediaDimensions.Y);
				MediaPlate->SetRenderResolution(MediaDimensions);

				// Resize the parent comp and its output render target to match the incoming media dimensions
				RenderTarget->ResizeTarget(MediaDimensions.X, MediaDimensions.Y);
				Comp->SetRenderResolution(MediaDimensions);
			}
		}
	}

	for (TStrongObjectPtr<UCameraCalibrationStep>& Step : CalibrationSteps)
	{
		if (Step.IsValid())
		{
			Step->Tick(DeltaTime);
		}
	}

	return true;
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

FIntPoint FCameraCalibrationStepsController::GetCompRenderTargetSize() const
{
	return RenderTargetSize;
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
	RenderTarget->InitAutoFormat(RenderTargetSize.X, RenderTargetSize.Y);
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
	
	MediaPlateRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	MediaPlateRenderTarget->ClearColor = FLinearColor::Black;
	MediaPlateRenderTarget->bAutoGenerateMips = false;
	MediaPlateRenderTarget->InitAutoFormat(RenderTargetSize.X, RenderTargetSize.Y);
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

	OverlayRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	OverlayRenderTarget->ClearColor = FLinearColor::Black;
	OverlayRenderTarget->bAutoGenerateMips = false;
	OverlayRenderTarget->InitAutoFormat(RenderTargetSize.X, RenderTargetSize.Y);
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


bool FCameraCalibrationStepsController::CalculateNormalizedMouseClickPosition(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FVector2D& OutPosition) const
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

	const FVector2D LocalInPixels = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	const float XNormalized = LocalInPixels.X / MyGeometry.Size.X;
	const float YNormalized = LocalInPixels.Y / MyGeometry.Size.Y;

	// Position 0~1. Origin at top-left corner of the viewport.
	OutPosition = FVector2D(XNormalized, YNormalized);

	return true;
}

bool FCameraCalibrationStepsController::ReadMediaPixels(TArray<FColor>& Pixels, FIntPoint& Size, ETextureRenderTargetFormat& PixelFormat, FText& OutErrorMessage) const
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

	PixelFormat = MediaPlateRenderTarget->RenderTargetFormat;

	// Read the pixels onto CPU
	const bool bReadPixels = MediaRenderTarget->ReadPixels(Pixels);

	if (!bReadPixels)
	{
		OutErrorMessage = LOCTEXT("ReadPixelsFailed", "ReadPixels from render target failed");
		return false;
	}

	Size = MediaRenderTarget->GetSizeXY();

	check(Pixels.Num() == Size.X * Size.Y);

	return true;
}

#undef LOCTEXT_NAMESPACE

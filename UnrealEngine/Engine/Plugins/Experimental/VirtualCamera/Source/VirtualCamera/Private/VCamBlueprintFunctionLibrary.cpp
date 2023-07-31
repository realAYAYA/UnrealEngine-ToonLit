// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamBlueprintFunctionLibrary.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "LevelSequence.h"
#include "VirtualCameraClipsMetaData.h"
#include "Components/SceneCaptureComponent2D.h"
#include "AssetRegistry/AssetData.h"
#include "VirtualCameraUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "MovieScene.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/Transactor.h"
#include "EditorAssetLibrary.h"
#include "ITakeRecorderModule.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "SceneView.h"
#include "SLevelViewport.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "TakePreset.h"
#include "VPUtilitiesEditor/Public/VPUtilitiesEditorBlueprintLibrary.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "LevelEditor/Public/LevelEditorSubsystem.h"
#include "Modules/ModuleManager.h"
#include "Recorder/TakeRecorderBlueprintLibrary.h"
#include "Recorder/TakeRecorderPanel.h"
#endif

bool UVCamBlueprintFunctionLibrary::IsGameRunning()
{
#if WITH_EDITOR
	return (GEditor && GEditor->IsPlaySessionInProgress());
#endif
	return true;
}

UVirtualCameraUserSettings* UVCamBlueprintFunctionLibrary::GetUserSettings()
{
	return GetMutableDefault<UVirtualCameraUserSettings>();
}

ULevelSequence* UVCamBlueprintFunctionLibrary::GetCurrentLevelSequence()
{
#if WITH_EDITOR
	ULevelSequence* LevelSequence = ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence();

	//if there's no sequence open in sequencer, use take recorder's pending take
	if (!LevelSequence)
	{
		ITakeRecorderModule& TakeRecorderModule = FModuleManager::LoadModuleChecked<ITakeRecorderModule>("TakeRecorder");
		UTakePreset* TakePreset = TakeRecorderModule.GetPendingTake();
		
		if (TakePreset)
		{
			LevelSequence = TakePreset->GetLevelSequence();
		}
	}
	return LevelSequence;
#else
	return nullptr;
#endif
}

void UVCamBlueprintFunctionLibrary::PlayCurrentLevelSequence()
{
#if WITH_EDITOR
	ULevelSequenceEditorBlueprintLibrary::Play();
#endif
}

void UVCamBlueprintFunctionLibrary::PauseCurrentLevelSequence()
{
#if WITH_EDITOR
	ULevelSequenceEditorBlueprintLibrary::Pause();
#endif
}

void UVCamBlueprintFunctionLibrary::SetCurrentLevelSequenceCurrentFrame(int32 NewFrame)
{
#if WITH_EDITOR
	ULevelSequenceEditorBlueprintLibrary::SetCurrentTime(NewFrame);
#endif

}

int32 UVCamBlueprintFunctionLibrary::GetCurrentLevelSequenceCurrentFrame()
{
#if WITH_EDITOR
	return ULevelSequenceEditorBlueprintLibrary::GetCurrentTime();
#else
	return 0;
#endif
}

int32 UVCamBlueprintFunctionLibrary::GetLevelSequenceLengthInFrames(const ULevelSequence* LevelSequence)
{
	if (LevelSequence)
	{
		int32 SequenceLowerBound = LevelSequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue().Value;
		int32 SequenceUpperBound = LevelSequence->GetMovieScene()->GetPlaybackRange().GetUpperBoundValue().Value;
		int32 Length = SequenceUpperBound - SequenceLowerBound;
		return ConvertFrameTime(Length, LevelSequence->GetMovieScene()->GetTickResolution(), LevelSequence->GetMovieScene()->GetDisplayRate()).FloorToFrame().Value;
	}

	return 0;
}


int32 UVCamBlueprintFunctionLibrary::TimecodeToFrameAmount(FTimecode Timecode, const FFrameRate& InFrameRate)
{
	return Timecode.ToFrameNumber(InFrameRate).Value; 

}

FTimecode UVCamBlueprintFunctionLibrary::GetLevelSequenceFrameAsTimecode(const ULevelSequence* LevelSequence, int32 InFrame)
{
	if (LevelSequence)
	{
		return FTimecode::FromFrameNumber(InFrame, LevelSequence->GetMovieScene()->GetDisplayRate());
	}

	return FTimecode();
}

FTimecode UVCamBlueprintFunctionLibrary::GetLevelSequenceFrameAsTimecodeWithoutObject(const FFrameRate DisplayRate, int32 InFrame)
{
	return FTimecode::FromFrameNumber(InFrame, DisplayRate);
}

bool UVCamBlueprintFunctionLibrary::IsCurrentLevelSequencePlaying()
{
#if WITH_EDITOR
	return ULevelSequenceEditorBlueprintLibrary::IsPlaying();
#else
	return false;
#endif
}


UTexture* UVCamBlueprintFunctionLibrary::ImportSnapshotTexture(FString FileName, FString SubFolderName, FString AbsolutePathPackage)
{
#if WITH_EDITOR
	return UVPUtilitiesEditorBlueprintLibrary::ImportSnapshotTexture(FileName, SubFolderName, AbsolutePathPackage);
#else
	return nullptr;
#endif
}


bool UVCamBlueprintFunctionLibrary::ModifyLevelSequenceMetadata(UVirtualCameraClipsMetaData* LevelSequenceMetaData)
{
#if WITH_EDITOR
	if (LevelSequenceMetaData)
	{
		LevelSequenceMetaData->MarkPackageDirty();

		return UEditorAssetLibrary::SaveAsset(LevelSequenceMetaData->GetPathName());
	}
	else
	{
		return false;
	}

#endif
	return false;
}

bool UVCamBlueprintFunctionLibrary::ModifyLevelSequenceMetadataForSelects(UVirtualCameraClipsMetaData* LevelSequenceMetaData, bool bIsSelected)
{
#if WITH_EDITOR
	if (LevelSequenceMetaData)
	{
		LevelSequenceMetaData->SetSelected(bIsSelected);
		LevelSequenceMetaData->MarkPackageDirty();

		return UEditorAssetLibrary::SaveAsset(LevelSequenceMetaData->GetPathName());
	}
	else
	{
		return false;
	}

#endif
	return false;

}

bool UVCamBlueprintFunctionLibrary::EditorSaveAsset(FString AssetPath)
{
#if WITH_EDITOR
	return UEditorAssetLibrary::SaveAsset(AssetPath, true);
#endif
	return false;

}


UObject* UVCamBlueprintFunctionLibrary::EditorLoadAsset(FString AssetPath)
{
#if WITH_EDITOR
	return UEditorAssetLibrary::LoadAsset(AssetPath);
#endif
	return nullptr;
}



void UVCamBlueprintFunctionLibrary::ModifyObjectMetadataTags(UObject* InObject, FName InTag, FString InValue)
{
#if WITH_EDITOR
	if (InObject) 
	{
		UEditorAssetLibrary::SetMetadataTag(InObject, InTag, InValue); 

	}
#endif
}

TMap<FName, FString> UVCamBlueprintFunctionLibrary::GetObjectMetadataTags(UObject* InObject)
{

#if WITH_EDITOR
	if (InObject)
	{
		return UEditorAssetLibrary::GetMetadataTagValues(InObject); 
	}
#endif
return TMap<FName, FString>();

}

TArray<FAssetData> UVCamBlueprintFunctionLibrary::SortAssetsByTimecodeAssetData(TArray<FAssetData> LevelSequenceAssetData)
{
	TArray<FAssetData> SortedTimecodeArray = LevelSequenceAssetData;


	//Sort an array of AssetData by their timecode contained in TakeMetaData inteded for use with LevelSequences
	SortedTimecodeArray.Sort([](const FAssetData& LevelSequence1AssetData, const FAssetData& LevelSequence2AssetData)
	{
		FString LevelSequence1TimestampTagValue;
		bool bFoundLevelSequence1Tag = LevelSequence1AssetData.GetTagValue("TakeMetaData_Timestamp", LevelSequence1TimestampTagValue);

		FString LevelSequence2TimestampTagValue;
		bool bFoundLevelSequence2Tag = LevelSequence2AssetData.GetTagValue("TakeMetaData_Timestamp", LevelSequence2TimestampTagValue);


		if (bFoundLevelSequence1Tag && bFoundLevelSequence2Tag)
		{
			FDateTime LevelSequence1TimeStamp;
			FDateTime LevelSequence2TimeStamp;
			FDateTime::Parse(LevelSequence1TimestampTagValue, LevelSequence1TimeStamp);
			FDateTime::Parse(LevelSequence2TimestampTagValue, LevelSequence2TimeStamp);
			return LevelSequence1TimeStamp > LevelSequence2TimeStamp;
		}

		//Handle cases in which valid metadata is found on one but not the another. 
		else if (bFoundLevelSequence1Tag == false && bFoundLevelSequence2Tag == true)
		{
			return false;
		}

		else if (bFoundLevelSequence1Tag == true && bFoundLevelSequence2Tag == false)
		{
			return true;

		}
		return false;

	});

	return SortedTimecodeArray;
}

void UVCamBlueprintFunctionLibrary::PilotActor(AActor* SelectedActor)
{
#if WITH_EDITOR
	if (SelectedActor)
	{
		ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

		if (LevelEditorSubsystem)
		{
			LevelEditorSubsystem->PilotLevelActor(SelectedActor);

			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
			if (LevelEditor)
			{
				TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditor->GetActiveViewportInterface();
				if (ActiveLevelViewport && !ActiveLevelViewport->IsLockedCameraViewEnabled())
				{
					ActiveLevelViewport->ToggleActorPilotCameraView();
				}
			}
		}
	}
#endif
}

bool UVCamBlueprintFunctionLibrary::UpdatePostProcessSettingsForCapture(USceneCaptureComponent2D* CaptureComponent, float DepthOfField, float FStopValue)
{
	if (CaptureComponent)
	{
		FPostProcessSettings NewCapturePostProcessSettings = CaptureComponent->PostProcessSettings;
		NewCapturePostProcessSettings.bOverride_DepthOfFieldFstop = true;
		NewCapturePostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;

		NewCapturePostProcessSettings.DepthOfFieldFstop = FStopValue;
		NewCapturePostProcessSettings.DepthOfFieldFocalDistance = DepthOfField;

		CaptureComponent->PostProcessSettings = NewCapturePostProcessSettings;
		return true;
	}
	return false;
}

FFrameRate UVCamBlueprintFunctionLibrary::GetDisplayRate(ULevelSequence* LevelSequence)
{
	if (LevelSequence)
	{
		return LevelSequence->GetMovieScene()->GetDisplayRate();
	}
	return FFrameRate();
}

FFrameRate UVCamBlueprintFunctionLibrary::ConvertStringToFrameRate(FString InFrameRateString)
{
	TValueOrError<FFrameRate, FExpressionError> ParseResult = ParseFrameRate(*InFrameRateString);
	if (ParseResult.IsValid())
	{
		return ParseResult.GetValue();
	}
	else {
		return FFrameRate();
	}
}

bool UVCamBlueprintFunctionLibrary::CallFunctionByName(UObject* ObjPtr, FName FunctionName)
{
	if (ObjPtr)
	{
		if (UFunction* Function = ObjPtr->FindFunction(FunctionName))
		{
			ObjPtr->ProcessEvent(Function, nullptr);
			return true;
		}
		return false;
	}
	return false;
}

float UVCamBlueprintFunctionLibrary::CalculateAutoFocusDistance(FVector2D ReticlePosition, UCineCameraComponent* CineCamera)
{
	if (!CineCamera)
	{
		return 0.0f;
	}

	float MaxFocusTraceDistance = 1000000.0f;
	FVector TraceDirection;
	FVector CameraWorldLocation;
	if (!DeprojectScreenToWorld(ReticlePosition, CameraWorldLocation, TraceDirection))
	{
		return 0.0f;
	}

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(UpdateAutoFocus), true);

	const FVector TraceEnd = CameraWorldLocation + TraceDirection * MaxFocusTraceDistance;
	FHitResult Hit;
	const bool bHit = CineCamera->GetWorld()->LineTraceSingleByChannel(Hit, CameraWorldLocation, TraceEnd, ECC_Visibility, TraceParams);

	return (bHit) ? Hit.Distance : MaxFocusTraceDistance;
}


void UVCamBlueprintFunctionLibrary::EditorSetGameView(bool bIsToggled)
{
#if WITH_EDITOR
	//Only set Game view when streaming in editor mode (so not on PIE, SIE or standalone) 
	if (!GEditor || GEditor->IsPlaySessionInProgress())
	{
		return;
	}
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

	if (LevelEditorSubsystem)
	{
		return LevelEditorSubsystem->EditorSetGameView(bIsToggled);
	}
#endif
}

void UVCamBlueprintFunctionLibrary::EnableDebugFocusPlane(UCineCameraComponent* CineCamera, bool bEnabled)
{
#if WITH_EDITOR
	if (!CineCamera)
	{
		return;
	}
	CineCamera->FocusSettings.bDrawDebugFocusPlane = bEnabled;
#endif
}

FString UVCamBlueprintFunctionLibrary::GetNextUndoDescription()
{
#if WITH_EDITOR
	if (GEditor != nullptr && GEditor->Trans != nullptr)
	{
		return GEditor->Trans->GetUndoContext().Title.ToString();
	}
#endif

	return "";
}

bool UVCamBlueprintFunctionLibrary::CopyToCineCameraActor(UCineCameraComponent* SourceCameraComponent, ACineCameraActor* TargetCameraActor)
{
	if (!(IsValid(SourceCameraComponent) && IsValid(TargetCameraActor) && IsValid(TargetCameraActor->GetCineCameraComponent())))
	{
		return false;
	}

	// Ensure the root actor transforms match
	if (const USceneComponent* SourceAttachment = SourceCameraComponent->GetAttachParent())
	{
		TargetCameraActor->SetActorTransform(SourceAttachment->GetComponentTransform());
	}

	// Copy the properties from one CineCameraComponent to the other
	UCineCameraComponent* TargetCameraComponent = TargetCameraActor->GetCineCameraComponent();
	UEngine::CopyPropertiesForUnrelatedObjects(SourceCameraComponent, TargetCameraComponent);
	
	TargetCameraActor->UpdateComponentTransforms();

	return true;
}

void UVCamBlueprintFunctionLibrary::SetActorLabel(AActor* Actor, const FString& NewActorLabel)
{
#if WITH_EDITOR
	if (Actor)
	{
		Actor->SetActorLabel(NewActorLabel);
	}
#endif
}

bool UVCamBlueprintFunctionLibrary::IsTakeRecorderPanelOpen()
{
#if WITH_EDITOR
	return IsValid(UTakeRecorderBlueprintLibrary::GetTakeRecorderPanel());
#else
	return false;
#endif
}

bool UVCamBlueprintFunctionLibrary::TryOpenTakeRecorderPanel()
{
#if WITH_EDITOR
	return IsValid(UTakeRecorderBlueprintLibrary::OpenTakeRecorderPanel());
#else
	return false;
#endif
}

bool UVCamBlueprintFunctionLibrary::DeprojectScreenToWorld(const FVector2D& InScreenPosition, FVector& OutWorldPosition, FVector& OutWorldDirection)
{
	FName LevelEditorName(TEXT("LevelEditor"));
	bool bSuccess = false;

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
		{
			APlayerController* PC = Context.OwningGameInstance->GetFirstLocalPlayerController(Context.World());
			if (PC)
			{
				bSuccess |= PC->DeprojectScreenPositionToWorld(InScreenPosition.X, InScreenPosition.Y, OutWorldPosition, OutWorldDirection);
				break;
			}
		}
#if WITH_EDITOR
		else if (Context.WorldType == EWorldType::Editor)
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(LevelEditorName);
			TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveLevelViewport();
			if (ActiveLevelViewport.IsValid() && ActiveLevelViewport->GetActiveViewport())
			{
				FViewport* ActiveViewport = ActiveLevelViewport->GetActiveViewport();
				FLevelEditorViewportClient& LevelViewportClient = ActiveLevelViewport->GetLevelViewportClient();
				FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
					ActiveViewport,
					LevelViewportClient.GetScene(),
					LevelViewportClient.EngineShowFlags)
					.SetRealtimeUpdate(true));
				FSceneView* View = LevelViewportClient.CalcSceneView(&ViewFamily);

				const FIntPoint ViewportSize = ActiveViewport->GetSizeXY();
				const FIntRect ViewRect = FIntRect(0, 0, ViewportSize.X, ViewportSize.Y);
				const FMatrix InvViewProjectionMatrix = View->ViewMatrices.GetInvViewProjectionMatrix();
				FSceneView::DeprojectScreenToWorld(InScreenPosition, ViewRect, InvViewProjectionMatrix, OutWorldPosition, OutWorldDirection);
				bSuccess = true;
			}
		}
#endif
	}

	if (!bSuccess)
	{
		OutWorldPosition = FVector::ZeroVector;
		OutWorldDirection = FVector::ZeroVector;
	}
	return bSuccess;
}

TArray<UObject*> UVCamBlueprintFunctionLibrary::GetBoundObjects(FMovieSceneObjectBindingID CameraBindingID)
{
	TArray<UObject*> BoundObjectsArray;
#if WITH_EDITOR
	BoundObjectsArray = ULevelSequenceEditorBlueprintLibrary::GetBoundObjects(CameraBindingID);
#endif
	return BoundObjectsArray;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePlaybackController.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/FileManager.h"
#include "LevelSequence.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MovieSceneTimeHelpers.h"
#include "UObject/UObjectBase.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "ILevelSequenceEditorToolkit.h"
#include "Input/Reply.h"
#include "Recorder/TakeRecorderBlueprintLibrary.h"
#include "Templates/SharedPointer.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif //WITH_EDITOR



ULevelSequencePlaybackController::ULevelSequencePlaybackController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ActiveLevelSequence(nullptr)
{

}

void ULevelSequencePlaybackController::GetLevelSequences(TArray<FLevelSequenceData>& OutLevelSequenceNames)
{
	OutLevelSequenceNames.Empty();

	TArray<FAssetData> LevelSequences;
	LevelSequences.Empty();

	FAssetRegistryModule* AssetRegistryModule = FModuleManager::Get().GetModulePtr<FAssetRegistryModule>("AssetRegistry");
	if (AssetRegistryModule)
	{
		IAssetRegistry& AssetRegistry = AssetRegistryModule->Get();
		if (!AssetRegistry.GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/LevelSequence"), TEXT("LevelSequence")), LevelSequences, false))
		{
			UE_LOG(LogActor, Error, TEXT("VirtualCamera - No Asset Registry module found!"));
			return;
		}
	}

	IFileManager& FileManager = IFileManager::Get();
	for (FAssetData LevelSequence : LevelSequences)
	{
		// Get the file system name of the package so we can get other data on it (i.e. timestamp)
		FString LevelSequenceFile = FPackageName::LongPackageNameToFilename(
			LevelSequence.PackageName.ToString(), 
			FPackageName::GetAssetPackageExtension()
		);

		FLevelSequenceData NewSequenceData = FLevelSequenceData(
			LevelSequence.GetObjectPathString(),
			LevelSequence.AssetName.ToString(),
			FileManager.GetTimeStamp(*LevelSequenceFile)
		);

		OutLevelSequenceNames.Add(NewSequenceData);
	}

	// Sort the level sequence names alphabetically by Display Name
	OutLevelSequenceNames.Sort([](const FLevelSequenceData& LeftItem, const FLevelSequenceData& RightItem)
	{
		return LeftItem.DisplayName < RightItem.DisplayName;
	});
}

FString ULevelSequencePlaybackController::GetActiveLevelSequenceName() const 
{
	if (ActiveLevelSequence)
	{
		return ActiveLevelSequence->GetName();
	}

	return FString();
}

FFrameRate ULevelSequencePlaybackController::GetCurrentSequenceFrameRate() const
{
	if (ActiveLevelSequence)
	{
		return ActiveLevelSequence->GetMovieScene()->GetDisplayRate();
	}

	return FFrameRate();
}

bool ULevelSequencePlaybackController::IsSequencerLockedToCameraCut() const
{
#if WITH_EDITOR
	if (ActiveLevelSequence)
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer)
		{
			return Sequencer->IsPerspectiveViewportCameraCutEnabled();
		}
	}
#endif //WITH_EDITOR

	return false;
}

void ULevelSequencePlaybackController::SetSequencerLockedToCameraCut(bool bLockView)
{
#if WITH_EDITOR
	if (ActiveLevelSequence)
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer)
		{
			return Sequencer->SetPerspectiveViewportCameraCutEnabled(bLockView);
		}
	}
#endif //WITH_EDITOR
}

FFrameNumber ULevelSequencePlaybackController::GetCurrentSequencePlaybackStart() const
{
	if (ActiveLevelSequence)
	{
		FFrameNumber Value = ActiveLevelSequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue();
		if (ActiveLevelSequence->GetMovieScene()->GetDisplayRate() != ActiveLevelSequence->GetMovieScene()->GetTickResolution())
		{
			const FFrameTime ConvertedTime = FFrameRate::TransformTime(FFrameTime(Value), ActiveLevelSequence->GetMovieScene()->GetTickResolution(), ActiveLevelSequence->GetMovieScene()->GetDisplayRate());
			Value = ConvertedTime.FrameNumber;
		}

		return Value;
	}

	return FFrameNumber();
}

FFrameNumber ULevelSequencePlaybackController::GetCurrentSequencePlaybackEnd() const
{
	if (ActiveLevelSequence)
	{
		FFrameNumber Value = ActiveLevelSequence->GetMovieScene()->GetPlaybackRange().GetUpperBoundValue();
		if (ActiveLevelSequence->GetMovieScene()->GetDisplayRate() != ActiveLevelSequence->GetMovieScene()->GetTickResolution())
		{
			const FFrameTime ConvertedTime = FFrameRate::TransformTime(FFrameTime(Value), ActiveLevelSequence->GetMovieScene()->GetTickResolution(), ActiveLevelSequence->GetMovieScene()->GetDisplayRate());
			Value = ConvertedTime.FrameNumber;
		}
	
		return Value;
	}

	return FFrameNumber();
}

FFrameNumber ULevelSequencePlaybackController::GetCurrentSequenceDuration() const
{
	if (ActiveLevelSequence)
	{
		FFrameNumber Value = UE::MovieScene::DiscreteSize(ActiveLevelSequence->GetMovieScene()->GetPlaybackRange());
		if (ActiveLevelSequence->GetMovieScene()->GetDisplayRate() != ActiveLevelSequence->GetMovieScene()->GetTickResolution())
		{
			const FFrameTime ConvertedTime = FFrameRate::TransformTime(FFrameTime(Value), ActiveLevelSequence->GetMovieScene()->GetTickResolution(), ActiveLevelSequence->GetMovieScene()->GetDisplayRate());
			Value = ConvertedTime.FrameNumber;
		}
		
		return Value;
	}

	return FFrameNumber();
}

FFrameTime ULevelSequencePlaybackController::GetCurrentSequencePlaybackPosition() const
{
#if WITH_EDITOR
	if (ActiveLevelSequence)
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer)
		{
			return Sequencer->GetLocalTime().ConvertTo(Sequencer->GetFocusedDisplayRate());
		}
	}
#endif //WITH_EDITOR

	return FFrameTime();
}

FTimecode ULevelSequencePlaybackController::GetCurrentSequencePlaybackTimecode() const
{
#if WITH_EDITOR
	if (ActiveLevelSequence)
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer)
		{
			const FFrameTime DisplayTime = Sequencer->GetLocalTime().ConvertTo(Sequencer->GetFocusedDisplayRate());
			return FTimecode::FromFrameNumber(DisplayTime.FrameNumber, Sequencer->GetFocusedDisplayRate());
		}
	}
#endif //WITH_EDITOR

	return FTimecode();
}

void ULevelSequencePlaybackController::JumpToPlaybackPosition(const FFrameNumber& InFrameNumber)
{
#if WITH_EDITOR
	if (ActiveLevelSequence)
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer)
		{
			const FFrameTime NewTime = ConvertFrameTime(InFrameNumber, Sequencer->GetFocusedDisplayRate(), Sequencer->GetFocusedTickResolution());
			Sequencer->SetLocalTime(NewTime, STM_None);
		}
	}
#endif //WITH_EDITOR
}

bool ULevelSequencePlaybackController::IsSequencePlaybackActive() const 
{
#if WITH_EDITOR
	if (ActiveLevelSequence)
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer)
		{
			return Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Playing && Sequencer->GetPlaybackSpeed() != 0.0f;
		}
	}
#endif //WITH_EDITOR

	return false;
}

void ULevelSequencePlaybackController::PauseLevelSequence()
{
#if WITH_EDITOR
	if (ActiveLevelSequence)
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer)
		{
			Sequencer->Pause();
		}
	}
#endif //WITH_EDITOR
}

void ULevelSequencePlaybackController::PlayLevelSequence()
{
#if WITH_EDITOR
	if (ActiveLevelSequence)
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer)
		{
			Sequencer->SetPlaybackSpeed(1.0f);
			Sequencer->OnPlay(false);
		}
	}
#endif //WITH_EDITOR
}

void ULevelSequencePlaybackController::PlayLevelSequenceReverse()
{
#if WITH_EDITOR
	if (ActiveLevelSequence)
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer)
		{
			Sequencer->SetPlaybackSpeed(-1.0f);
			Sequencer->OnPlay(false);
		}
	}
#endif //WITH_EDITOR
}

void ULevelSequencePlaybackController::StopLevelSequencePlay()
{
#if WITH_EDITOR
	if (ActiveLevelSequence)
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer)
		{
			Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
		}
	}
#endif //WITH_EDITOR
}

bool ULevelSequencePlaybackController::SetActiveLevelSequence(ULevelSequence* InNewLevelSequence)
{
#if WITH_EDITOR
	if(InNewLevelSequence && !IsRunningGame())
	{
		const bool bDoFocusOnEditor = false;
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(InNewLevelSequence);
		IAssetEditorInstance* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(InNewLevelSequence, bDoFocusOnEditor);
		ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor);

		WeakSequencer = LevelSequenceEditor ? LevelSequenceEditor->GetSequencer() : nullptr;
		if (WeakSequencer.IsValid())
		{
			TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
			if (Sequencer)
			{
				const bool bSetCameraPerspective = InNewLevelSequence->GetMovieScene()->GetCameraCutTrack() != nullptr;

				//If we're dealing with a sequence that already has a camera cut, don't enable recording and set camera perspective for review.
				OnRecordEnabledStateChanged.ExecuteIfBound(!bSetCameraPerspective);
				Sequencer->SetPerspectiveViewportCameraCutEnabled(bSetCameraPerspective);

				ActiveLevelSequence = InNewLevelSequence;
				return true;
			}
		}
	}
#endif //WITH_EDITOR

	return false;
}

void ULevelSequencePlaybackController::ClearActiveLevelSequence()
{
	if (ActiveLevelSequence)
	{
		StopLevelSequencePlay();
		ActiveLevelSequence = nullptr;
#if WITH_EDITOR
		WeakSequencer = nullptr;
#endif
	}
}

void ULevelSequencePlaybackController::PlayToEnd()
{
	if (ActiveLevelSequence)
	{
		PlayLevelSequence();
	}
}

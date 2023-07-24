// Copyright Epic Games, Inc. All Rights Reserved.

#include "Recorder/TakeRecorderBlueprintLibrary.h"
#include "Recorder/TakeRecorder.h"
#include "Recorder/TakeRecorderPanel.h"
#include "Widgets/STakeRecorderTabContent.h"
#include "ITakeRecorderModule.h"
#include "TakeRecorderSettings.h"
#include "UObject/Package.h"

#include "Modules/ModuleManager.h"
#include "LevelEditor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TakeRecorderBlueprintLibrary)

namespace
{
	static UTakeRecorderPanel* CurrentTakeRecorderPanel;
	static FOnTakeRecorderPanelChanged TakeRecorderPanelChanged;
	static FOnTakeRecorderPreInitialize TakeRecorderPreInitialize;
	static FOnTakeRecorderStarted TakeRecorderStarted;
	static FOnTakeRecorderStopped TakeRecorderStopped;
	static FOnTakeRecorderFinished TakeRecorderFinished;
	static FOnTakeRecorderCancelled TakeRecorderCancelled;
	static FOnTakeRecorderMarkedFrameAdded TakeRecorderMarkedFrameAdded;
}


bool UTakeRecorderBlueprintLibrary::IsTakeRecorderEnabled()
{
#if WITH_EDITOR
	return !IsRunningGame();
#else
	return false;
#endif 
}

UTakeRecorder* UTakeRecorderBlueprintLibrary::StartRecording(ULevelSequence* LevelSequence, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, const FTakeRecorderParameters& Parameters)
{
	if (IsRecording())
	{
		// @todo: script error?
		return nullptr;
	}

	if (LevelSequence == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("The LevelSequence is invalid."), ELogVerbosity::Error);
		return nullptr;
	}
	if (Sources == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("The Sources is invalid."), ELogVerbosity::Error);
		return nullptr;
	}
	if (MetaData == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("The MetaData is invalid."), ELogVerbosity::Error);
		return nullptr;
	}

	FText ErrorText = NSLOCTEXT("TakeRecorderBlueprintLibrary", "UnknownError", "An unknown error occurred when trying to start recording");

	UTakeRecorder* NewRecorder = NewObject<UTakeRecorder>(GetTransientPackage(), NAME_None, RF_Transient);
	if (NewRecorder->Initialize(LevelSequence, Sources, MetaData, Parameters, &ErrorText))
	{
		return NewRecorder;
	}
	else if (ensure(!ErrorText.IsEmpty()))
	{
		FFrame::KismetExecutionMessage(*ErrorText.ToString(), ELogVerbosity::Error);
	}

	return nullptr;
}

FTakeRecorderParameters UTakeRecorderBlueprintLibrary::GetDefaultParameters()
{
	FTakeRecorderParameters DefaultParams;

	DefaultParams.User    = GetDefault<UTakeRecorderUserSettings>()->Settings;
	DefaultParams.Project = GetDefault<UTakeRecorderProjectSettings>()->Settings;

	return DefaultParams;
}

void UTakeRecorderBlueprintLibrary::SetDefaultParameters(const FTakeRecorderParameters& InDefaultParameters)
{
	GetMutableDefault<UTakeRecorderUserSettings>()->Settings = InDefaultParameters.User;
	GetMutableDefault<UTakeRecorderProjectSettings>()->Settings = InDefaultParameters.Project;
}

bool UTakeRecorderBlueprintLibrary::IsRecording()
{
	return UTakeRecorder::GetActiveRecorder() != nullptr && UTakeRecorder::GetActiveRecorder()->GetState() == ETakeRecorderState::Started;
}

UTakeRecorder* UTakeRecorderBlueprintLibrary::GetActiveRecorder()
{
	return UTakeRecorder::GetActiveRecorder();
}

void UTakeRecorderBlueprintLibrary::StopRecording()
{
	UTakeRecorder* Existing = UTakeRecorder::GetActiveRecorder();
	if (Existing)
	{
		Existing->Stop();
	}
}

void UTakeRecorderBlueprintLibrary::CancelRecording()
{
	UTakeRecorder* Existing = UTakeRecorder::GetActiveRecorder();
	if (Existing)
	{
		Existing->Cancel();
	}
}

UTakeRecorderPanel* UTakeRecorderBlueprintLibrary::OpenTakeRecorderPanel()
{
	UTakeRecorderPanel* Existing = GetTakeRecorderPanel();
	if (Existing)
	{
		return Existing;
	}

	if (!IsRunningGame())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetLevelEditorTabManager()->TryInvokeTab(ITakeRecorderModule::TakeRecorderTabName);
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("The Take Recorder Panel will not open because the game is running."), ELogVerbosity::Log);
	}

	return GetTakeRecorderPanel();
}

UTakeRecorderPanel* UTakeRecorderBlueprintLibrary::GetTakeRecorderPanel()
{
	return CurrentTakeRecorderPanel;
}

void UTakeRecorderBlueprintLibrary::SetOnTakeRecorderPanelChanged(FOnTakeRecorderPanelChanged OnTakeRecorderPanelChanged)
{
	TakeRecorderPanelChanged = OnTakeRecorderPanelChanged;
};

void UTakeRecorderBlueprintLibrary::SetOnTakeRecorderPreInitialize(FOnTakeRecorderPreInitialize OnTakeRecorderPreInitialize)
{
	TakeRecorderPreInitialize = OnTakeRecorderPreInitialize;
};

void UTakeRecorderBlueprintLibrary::SetOnTakeRecorderStarted(FOnTakeRecorderStarted OnTakeRecorderStarted)
{
	TakeRecorderStarted = OnTakeRecorderStarted;
};

void UTakeRecorderBlueprintLibrary::SetOnTakeRecorderStopped(FOnTakeRecorderStopped OnTakeRecorderStopped)
{
	TakeRecorderStopped = OnTakeRecorderStopped;
};

void UTakeRecorderBlueprintLibrary::SetOnTakeRecorderFinished(FOnTakeRecorderFinished OnTakeRecorderFinished)
{
	TakeRecorderFinished = OnTakeRecorderFinished;
};

void UTakeRecorderBlueprintLibrary::SetOnTakeRecorderCancelled(FOnTakeRecorderCancelled OnTakeRecorderCancelled)
{
	TakeRecorderCancelled = OnTakeRecorderCancelled;
};

void UTakeRecorderBlueprintLibrary::SetOnTakeRecorderMarkedFrameAdded(FOnTakeRecorderMarkedFrameAdded OnTakeRecorderMarkedFrameAdded)
{
	TakeRecorderMarkedFrameAdded = OnTakeRecorderMarkedFrameAdded;
};

void UTakeRecorderBlueprintLibrary::OnTakeRecorderPreInitialize()
{
	TakeRecorderPreInitialize.ExecuteIfBound();
}

void UTakeRecorderBlueprintLibrary::OnTakeRecorderStarted()
{
	TakeRecorderStarted.ExecuteIfBound();
}

void UTakeRecorderBlueprintLibrary::OnTakeRecorderStopped()
{
	TakeRecorderStopped.ExecuteIfBound();
}

void UTakeRecorderBlueprintLibrary::OnTakeRecorderFinished(ULevelSequence* InSequenceAsset)
{
	TakeRecorderFinished.ExecuteIfBound(InSequenceAsset);
}

void UTakeRecorderBlueprintLibrary::OnTakeRecorderCancelled()
{
	TakeRecorderCancelled.ExecuteIfBound();
}

void UTakeRecorderBlueprintLibrary::OnTakeRecorderMarkedFrameAdded(const FMovieSceneMarkedFrame& InMarkedFrame)
{
	TakeRecorderMarkedFrameAdded.ExecuteIfBound(InMarkedFrame);
}

void UTakeRecorderBlueprintLibrary::SetTakeRecorderPanel(UTakeRecorderPanel* InNewPanel)
{
	if (CurrentTakeRecorderPanel != InNewPanel)
	{
		if (CurrentTakeRecorderPanel)
		{
			// Old panel is no longer valid
			CurrentTakeRecorderPanel->ClosePanel();

			CurrentTakeRecorderPanel->RemoveFromRoot();
			CurrentTakeRecorderPanel = nullptr;
		}

		if (InNewPanel && InNewPanel->IsPanelOpen())
		{
			InNewPanel->AddToRoot();
			CurrentTakeRecorderPanel = InNewPanel;
		}

		TakeRecorderPanelChanged.ExecuteIfBound();
	}
}

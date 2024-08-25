// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelinePIEExecutor.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineShotConfig.h"
#include "MoviePipeline.h"
#include "MoviePipelineGameOverrideSetting.h"
#include "MoviePipelineOutputSetting.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Framework/Application/SlateApplication.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MovieRenderPipelineSettings.h"
#include "Editor/EditorEngine.h"
#include "Editor.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineBlueprintLibrary.h"
#include "MoviePipelinePIEExecutorSettings.h"
#include "MoviePipelineEditorBlueprintLibrary.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "MessageLogModule.h"
#include "Logging/MessageLog.h"
#include "Graph/MovieGraphPipeline.h"
#include "Graph/Nodes/MovieGraphGlobalGameOverrides.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelinePIEExecutor)

#define LOCTEXT_NAMESPACE "MoviePipelinePIEExecutor"


const TArray<FString> UMoviePipelinePIEExecutor::FValidationMessageGatherer::AllowList = { "LogMovieRenderPipeline", "LogMovieRenderPipelineIO", "LogMoviePipelineExecutor", "LogImageWriteQueue", "LogAppleProResMedia", "LogAvidDNxMedia"};

UMoviePipelinePIEExecutor::FValidationMessageGatherer::FValidationMessageGatherer()
	: FOutputDevice()
	, ExecutorLog()
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions MessageLogOptions;
	MessageLogOptions.bShowPages = true;
	MessageLogOptions.bAllowClear = true;
	MessageLogOptions.MaxPageCount = 10;
	MessageLogOptions.bShowFilters = true;
	MessageLogModule.RegisterLogListing("MoviePipelinePIEExecutor", LOCTEXT("MoviePipelineExecutorLogLabel", "High Quality Media Export"));

	ExecutorLog = MakeUnique<FMessageLog>("MoviePipelinePIEExecutor");
}

void UMoviePipelinePIEExecutor::FValidationMessageGatherer::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	for (const FString& AllowedCategory : AllowList)
	{
		if (Category.ToString().Equals(AllowedCategory))
		{
			if (Verbosity == ELogVerbosity::Warning)
			{
				ExecutorLog->Warning(FText::FromString(FString(V)));
			}
			else if (Verbosity == ELogVerbosity::Error)
			{
				ExecutorLog->Error(FText::FromString(FString(V)));
			}
			return;
		}
	}
}

UMoviePipelinePIEExecutor::UMoviePipelinePIEExecutor()
	: UMoviePipelineLinearExecutorBase()
	, bRenderOffscreen(false)
	, RemainingInitializationFrames(-1)
	, bPreviousUseFixedTimeStep(false)
	, PreviousFixedTimeStepDelta(1 / 30.0)
{
	if (!IsTemplate() && FSlateApplication::IsInitialized())
	{
		if (!FApp::CanEverRender() || FSlateApplication::Get().IsRenderingOffScreen())
		{
			SetIsRenderingOffscreen(true);
		}
	}
}

void UMoviePipelinePIEExecutor::Start(const UMoviePipelineExecutorJob* InJob)
{
	Super::Start(InJob);

	// Check for null sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(InJob->Sequence.TryLoad());
	if (!LevelSequence)
	{
		if(!IsRenderingOffscreen())
		{
			FText FailureReason = LOCTEXT("InvalidSequenceFailureDialog", "One or more jobs in the queue has an invalid/null sequence.");
			FMessageDialog::Open(EAppMsgType::Ok, FailureReason);
		}

		OnExecutorFinishedImpl();
		return;
	}

	// Check for unsaved maps. It's pretty rare that someone actually wants to execute on an unsaved map,
	// and it catches the much more common case of adding the job to an unsaved map and then trying to render
	// from a newly loaded map, PIE startup will fail because the map is no longer valid.
	const bool bAllMapsValid = UMoviePipelineEditorBlueprintLibrary::IsMapValidForRemoteRender(Queue->GetJobs());
	if (!bAllMapsValid)
	{
		if(!IsRenderingOffscreen())
		{
			FText FailureReason = LOCTEXT("UnsavedMapFailureDialog", "One or more jobs in the queue have an unsaved map as their target map. Maps must be saved at least once before rendering, and then the job must be manually updated to point to the newly saved map.");
			FMessageDialog::Open(EAppMsgType::Ok, FailureReason);
		}

		OnExecutorFinishedImpl();
		return;
	}

	// Start capturing logging messages
	ValidationMessageGatherer.StartGathering();

	FVector2D WindowSize = FVector2D(1280, 720);

	const UMoviePipelinePIEExecutorSettings* ExecutorSettings = GetDefault<UMoviePipelinePIEExecutorSettings>();
	if (ExecutorSettings->bResizePIEWindowToOutputResolution)
	{
		// Allow creating the PIE window at the size of the output to work around UMG scaling bugs when the main viewport is one resolution and then we re-render it at another.
		UMoviePipelineOutputSetting* OutputSetting = Cast<UMoviePipelineOutputSetting>(InJob->GetConfiguration()->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass()));
		WindowSize = FVector2D(OutputSetting->OutputResolution.X, OutputSetting->OutputResolution.Y);
	}


	// Create a Slate window to hold our preview.
	TSharedRef<SWindow> CustomWindow = SNew(SWindow)
		.ClientSize(WindowSize)
		.AutoCenter(EAutoCenter::PrimaryWorkArea)
		.UseOSWindowBorder(true)
		.FocusWhenFirstShown(true)
		.ActivationPolicy(EWindowActivationPolicy::Never)
		.HasCloseButton(true)
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.SizingRule(ESizingRule::UserSized);

	WeakCustomWindow = CustomWindow;
	FSlateApplication::Get().AddWindow(CustomWindow, !IsRenderingOffscreen());

	// Initialize our own copy of the Editor Play settings which we will adjust defaults on.
	ULevelEditorPlaySettings* PlayInEditorSettings = NewObject<ULevelEditorPlaySettings>();
	PlayInEditorSettings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	PlayInEditorSettings->SetPlayNumberOfClients(1);
	PlayInEditorSettings->bLaunchSeparateServer = false;
	PlayInEditorSettings->SetRunUnderOneProcess(true);
	PlayInEditorSettings->LastExecutedPlayModeType = EPlayModeType::PlayMode_InEditorFloating;
	PlayInEditorSettings->bUseNonRealtimeAudioDevice = true;

	FRequestPlaySessionParams Params;
	Params.EditorPlaySettings = PlayInEditorSettings;
	Params.CustomPIEWindow = CustomWindow;
	Params.GlobalMapOverride = InJob->Map.GetAssetPathString();
	// Don't allow the online subsystem to try and authenticate as it will delay PIE startup and no PIE world will exist when PostPIEStarted is called.
	Params.bAllowOnlineSubsystem = false;

	// Initialize the transient settings so that they will exist in time for the GameOverrides check.
	InJob->GetConfiguration()->InitializeTransientSettings();

	if (TSubclassOf<AGameModeBase> GameModeOverride = UMovieGraphGlobalGameOverridesNode::GetGameModeOverride(InJob))
	{
		Params.GameModeOverride = GameModeOverride;
	}

	bPreviousUseFixedTimeStep = FApp::UseFixedTimeStep();
	PreviousFixedTimeStepDelta = FApp::GetFixedDeltaTime();

	// Force the engine into fixed timestep mode. It's going to get overridden on the first frame by the movie pipeline,
	// and everything controlled by Sequencer will use the correct timestep for renders but non-controlled things (such
	// as pawns) use an uncontrolled DT on the first frame which lowers determinism.
	FApp::SetUseFixedTimeStep(true);
	FApp::SetFixedDeltaTime(InJob->GetConfiguration()->GetEffectiveFrameRate(LevelSequence).AsInterval());

	// Kick off an async request to start a play session. This won't happen until the next frame.
	GEditor->RequestPlaySession(Params);

	// Listen for PIE startup since there's no current way to pass a delegate through the request.
	FEditorDelegates::PostPIEStarted.AddUObject(this, &UMoviePipelinePIEExecutor::OnPIEStartupFinished);
}

void UMoviePipelinePIEExecutor::OnPIEStartupFinished(bool)
{
	// Immediately un-bind our delegate so that we don't catch all PIE startup requests in the future.
	FEditorDelegates::PostPIEStarted.RemoveAll(this);

	// Hack to find out the PIE world since it is not provided by the delegate.
	UWorld* ExecutingWorld = nullptr;
	
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE)
		{
			ExecutingWorld = Context.World();
		}
	}
	
	if(!ExecutingWorld)
	{
		// This only happens if PIE startup fails and they've usually gotten a pop-up dialog already.
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Failed to find a PIE UWorld after OnPIEStartupFinished!"));
		OnExecutorFinishedImpl();
		return;
	}

	// Only mark us as rendering once we've gotten the OnPIEStartupFinished call. If something were to interrupt PIE
	// startup (such as non-compiled blueprints) the queue would get stuck thinking it's rendering when it's not.
	bIsRendering = true;

	// Allow the user to have overridden which Pipeline is actually run. This is an unlikely scenario but allows
	// the user to create their own implementation while still re-using the rest of our UI and infrastructure.
	const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();
	TSubclassOf<UMoviePipelineBase> PipelineClass = ProjectSettings->DefaultPipeline.TryLoadClass<UMoviePipelineBase>();
	if (!PipelineClass)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Failed to load project specified MoviePipeline class type!"));
		OnExecutorFinishedImpl();
		return;
	}

	const UMoviePipelineExecutorJob* CurrentJob = Queue->GetJobs()[CurrentPipelineIndex];
	if (CurrentJob->IsUsingGraphConfiguration())
	{
		PipelineClass = UMovieGraphPipeline::StaticClass();
	}

	// This Pipeline belongs to the world being created so that they have context for things they execute.
	ActiveMoviePipeline = NewObject<UMoviePipelineBase>(ExecutingWorld, PipelineClass);

	UMoviePipeline* PipelineAsLegacy = Cast<UMoviePipeline>(ActiveMoviePipeline);

	
	// We allow users to set a multi-frame delay before we actually run the Initialization function and start thinking.
	// This solves cases where there are engine systems that need to finish loading before we do anything.
	const UMoviePipelinePIEExecutorSettings* ExecutorSettings = GetDefault<UMoviePipelinePIEExecutorSettings>();

	// We tick each frame to update the Window Title, and kick off latent pipeling initialization.
	FCoreDelegates::OnBeginFrame.AddUObject(this, &UMoviePipelinePIEExecutor::OnTick);

	// Listen for when the pipeline thinks it has finished.
	ActiveMoviePipeline->OnMoviePipelineWorkFinished().AddUObject(this, &UMoviePipelinePIEExecutor::OnPIEMoviePipelineFinished);
	ActiveMoviePipeline->OnMoviePipelineShotWorkFinished().AddUObject(this, &UMoviePipelinePIEExecutor::OnJobShotFinished);
	if (PipelineAsLegacy)
	{

		PipelineAsLegacy->SetViewportInitArgs(ViewportInitArgs);
	}
	
	if (ExecutorSettings->InitialDelayFrameCount == 0)
	{
		OnIndividualJobStartedImpl(Queue->GetJobs()[CurrentPipelineIndex]);
		if (PipelineAsLegacy)
		{
			PipelineAsLegacy->Initialize(Queue->GetJobs()[CurrentPipelineIndex]);
			if (CustomInitializationTime.IsSet())
			{
				PipelineAsLegacy->SetInitializationTime(CustomInitializationTime.GetValue());
			}
		}
		else if (UMovieGraphPipeline* PipelineAsGraph = Cast<UMovieGraphPipeline>(ActiveMoviePipeline))
		{
			PipelineAsGraph->Initialize(Queue->GetJobs()[CurrentPipelineIndex], FMovieGraphInitConfig());
			if (CustomInitializationTime.IsSet())
			{
				PipelineAsGraph->SetInitializationTime(CustomInitializationTime.GetValue());
			}
		}
		RemainingInitializationFrames = -1;
	}
	else
	{
		RemainingInitializationFrames = ExecutorSettings->InitialDelayFrameCount;
	}
	
	// Listen for PIE shutdown in case the user hits escape to close it. 
	FEditorDelegates::EndPIE.AddUObject(this, &UMoviePipelinePIEExecutor::OnPIEEnded);
}

void UMoviePipelinePIEExecutor::OnTick()
{
	if (RemainingInitializationFrames >= 0)
	{
		if (RemainingInitializationFrames == 0)
		{
			OnIndividualJobStartedImpl(Queue->GetJobs()[CurrentPipelineIndex]);
			
			UMoviePipeline* PipelineAsLegacy = Cast<UMoviePipeline>(ActiveMoviePipeline);
			if (PipelineAsLegacy)
			{
				PipelineAsLegacy->Initialize(Queue->GetJobs()[CurrentPipelineIndex]);
				if (CustomInitializationTime.IsSet())
				{
					PipelineAsLegacy->SetInitializationTime(CustomInitializationTime.GetValue());
				}
			}
			else if (UMovieGraphPipeline* PipelineAsGraph = Cast<UMovieGraphPipeline>(ActiveMoviePipeline))
			{
				PipelineAsGraph->Initialize(Queue->GetJobs()[CurrentPipelineIndex], FMovieGraphInitConfig());
				if (CustomInitializationTime.IsSet())
				{
					PipelineAsGraph->SetInitializationTime(CustomInitializationTime.GetValue());
				}
			}
		}

		RemainingInitializationFrames--;
	}
	
	if (RemainingInitializationFrames <= 0)
	{
		Queue->GetJobs()[CurrentPipelineIndex]->SetStatusProgress(GetCompletionPercentageFromActivePipeline());
	}
	
	FText WindowTitle = GetWindowTitle();
	TSharedPtr<SWindow> CustomWindow = WeakCustomWindow.Pin();
	if (CustomWindow)
	{
		CustomWindow->SetTitle(WindowTitle);
	}
}

void UMoviePipelinePIEExecutor::OnPIEMoviePipelineFinished(FMoviePipelineOutputData InOutputData)
{
	if (!InOutputData.bSuccess)
	{
		// We do the cast to UMoviePipeline (which will return nullptr during a graph render) because the functions
		// all take a pointer, but it's never actually used, and you can only have one pipeline at once so the API
		// doesn't really need to pass back the pointer to a soon-to-be-destroyed object.
		OnPipelineErrored(Cast<UMoviePipeline>(InOutputData.Pipeline), true, FText());
	}

	// Unsubscribe to the EndPIE event so we don't think the user canceled it.
	FCoreDelegates::OnBeginFrame.RemoveAll(this);

	// Reset progress for UI (regardless of success)
	Queue->GetJobs()[CurrentPipelineIndex]->SetStatusProgress(0.0f);

	if (ActiveMoviePipeline)
	{
		// Unsubscribe in the event that it gets called twice we don't have issues.
		ActiveMoviePipeline->OnMoviePipelineWorkFinished().RemoveAll(this);
	}

	// The End Play will happen on the next frame.
	GEditor->RequestEndPlayMap();
}

void UMoviePipelinePIEExecutor::OnJobShotFinished(FMoviePipelineOutputData InOutputData)
{
	// Just re-broadcast the delegate to our listeners.
	OnIndividualShotWorkFinishedDelegateNative.Broadcast(InOutputData);
	OnIndividualShotWorkFinishedDelegate.Broadcast(InOutputData);
}

void UMoviePipelinePIEExecutor::BeginDestroy()
{
	// Ensure we're no longer gathering, otherwise it tries to call a callback on the now dead uobject.
	ValidationMessageGatherer.StopGathering();

	Super::BeginDestroy();
}

void UMoviePipelinePIEExecutor::OnPIEEnded(bool)
{
	FEditorDelegates::EndPIE.RemoveAll(this);

	CachedOutputDataParams = FMoviePipelineOutputData();
	UMoviePipeline* PipelineAsLegacy = Cast<UMoviePipeline>(ActiveMoviePipeline);

	// Only call Shutdown if the pipeline hasn't been finished.
	if (ActiveMoviePipeline && ActiveMoviePipeline->GetPipelineState() != EMovieRenderPipelineState::Finished)
	{
		UE_LOG(LogMovieRenderPipeline, Log, TEXT("PIE Ended while Movie Pipeline was still active. Stalling to do full shutdown."));

		// This will flush any outstanding work on the movie pipeline (file writes) immediately
		ActiveMoviePipeline->Shutdown(true);
		UE_LOG(LogMovieRenderPipeline, Log, TEXT("MoviePipelinePIEExecutor: Stalling finished, pipeline has shut down."));
	}
	if (PipelineAsLegacy)
	{
		// Cache this off so we can use it in DelayedFinishNotification, at which point ActiveMoviePipeline is null.
		CachedOutputDataParams = PipelineAsLegacy->GetOutputDataParams();
	}

	// We need to null out this reference this frame, otherwise we try to hold onto a PIE
	// object after PIE finishes which causes a GC leak.
	ActiveMoviePipeline = nullptr;

	// Delay for one frame so that PIE can finish shut down. It's not a huge fan of us starting up on the same frame.
	GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UMoviePipelinePIEExecutor::DelayedFinishNotification));

	// Restore the previous settings.
	FApp::SetUseFixedTimeStep(bPreviousUseFixedTimeStep);
	FApp::SetFixedDeltaTime(PreviousFixedTimeStepDelta);

	// Stop capturing logging messages
	ValidationMessageGatherer.StopGathering();
	ValidationMessageGatherer.OpenLog();
}

void UMoviePipelinePIEExecutor::DelayedFinishNotification()
{
	// Get the params for the job (including output info) from the cache. ActiveMoviePipeline is null already.
	OnIndividualJobFinishedImpl(CachedOutputDataParams);
	
	// Now that another frame has passed and we should be OK to start another PIE session, notify our owner.
	OnIndividualPipelineFinished(nullptr);
}
void UMoviePipelinePIEExecutor::OnIndividualJobStartedImpl(UMoviePipelineExecutorJob* InJob)
{
	// Broadcast to both Native and Python/BP
	OnIndividualJobStartedDelegateNative.Broadcast(InJob);
	OnIndividualJobStartedDelegate.Broadcast(InJob);
}

void UMoviePipelinePIEExecutor::OnIndividualJobFinishedImpl(FMoviePipelineOutputData InOutputData)
{
	// Broadcast to both Native and Python/BP
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OnIndividualJobFinishedDelegateNative.Broadcast(InOutputData.Job, !IsAnyJobErrored());
	OnIndividualJobFinishedDelegate.Broadcast(InOutputData.Job, !IsAnyJobErrored());
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	OnIndividualJobWorkFinishedDelegateNative.Broadcast(InOutputData);
	OnIndividualJobWorkFinishedDelegate.Broadcast(InOutputData);
}

#undef LOCTEXT_NAMESPACE // "MoviePipelinePIEExecutor"


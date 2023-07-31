// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineQueueSubsystem.h"
#include "LevelSequenceEditorModule.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelineQueueSubsystem)

UMoviePipelineExecutorBase* UMoviePipelineQueueSubsystem::RenderQueueWithExecutor(TSubclassOf<UMoviePipelineExecutorBase> InExecutorType)
{
	if(!ensureMsgf(!IsRendering(), TEXT("RenderQueueWithExecutor cannot be called while already rendering!")))
	{
		return nullptr;
	}

	ActiveExecutor = NewObject<UMoviePipelineExecutorBase>(this, InExecutorType);
	RenderQueueWithExecutorInstance(ActiveExecutor);
	return ActiveExecutor;
}

void UMoviePipelineQueueSubsystem::RenderQueueWithExecutorInstance(UMoviePipelineExecutorBase* InExecutor)
{
	if (!ensureMsgf(!IsRendering(), TEXT("RenderQueueWithExecutor cannot be called while already rendering!")))
	{
		FFrame::KismetExecutionMessage(TEXT("Render already in progress."), ELogVerbosity::Error);
		return;
	}

	if (!InExecutor)
	{
		FFrame::KismetExecutionMessage(TEXT("Invalid executor supplied."), ELogVerbosity::Error);
		return;
	}

	ILevelSequenceEditorModule* LevelSequenceEditorModule = FModuleManager::GetModulePtr<ILevelSequenceEditorModule>("LevelSequenceEditor");
	if (LevelSequenceEditorModule)
	{
		LevelSequenceEditorModule->OnComputePlaybackContext().AddUObject(this, &UMoviePipelineQueueSubsystem::OnSequencerContextBinding);
	}

	ActiveExecutor = InExecutor;
	ActiveExecutor->OnExecutorFinished().AddUObject(this, &UMoviePipelineQueueSubsystem::OnExecutorFinished);
	ActiveExecutor->Execute(GetQueue());
}

void UMoviePipelineQueueSubsystem::OnSequencerContextBinding(bool& bAllowBinding)
{
	if (ActiveExecutor && ActiveExecutor->IsRendering())
	{
		bAllowBinding = false;
	}
}

void UMoviePipelineQueueSubsystem::OnExecutorFinished(UMoviePipelineExecutorBase* InPipelineExecutor, bool bSuccess)
{
	ILevelSequenceEditorModule* LevelSequenceEditorModule = FModuleManager::GetModulePtr<ILevelSequenceEditorModule>("LevelSequenceEditorModule");
	if (LevelSequenceEditorModule)
	{
		LevelSequenceEditorModule->OnComputePlaybackContext().RemoveAll(this);
	}

	ActiveExecutor = nullptr;
}



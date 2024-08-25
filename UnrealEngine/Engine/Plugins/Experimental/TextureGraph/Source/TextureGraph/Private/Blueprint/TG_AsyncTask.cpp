// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/TG_AsyncTask.h"

#include "TextureGraphEngine.h"
#include "Blueprint/TG_AsyncTaskManager.h"


UTG_AsyncTask::UTG_AsyncTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTG_AsyncTask::RegisterWithTGAsyncTaskManger()
{
	FTG_AsyncTaskManager::GetInstance().RegisterTask(this);
}

void UTG_AsyncTask::SetReadyToDestroy()
{
	UBlueprintAsyncActionBase::SetReadyToDestroy();
	FTG_AsyncTaskManager::GetInstance().UnRegisterTask(this);
}

void UTG_AsyncTask::Activate()
{
	TextureGraphEngine::SetRunEngine();
}

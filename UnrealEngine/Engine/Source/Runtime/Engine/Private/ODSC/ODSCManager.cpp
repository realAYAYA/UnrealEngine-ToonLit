// Copyright Epic Games, Inc. All Rights Reserved.

#include "ODSC/ODSCManager.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "ODSCLog.h"
#include "ODSCThread.h"
#include "Containers/BackgroundableTicker.h"

DEFINE_LOG_CATEGORY(LogODSC);

// FODSCManager

FODSCManager* GODSCManager = nullptr;

FODSCManager::FODSCManager()
	: FTSTickerObjectBase(0.0f, FTSBackgroundableTicker::GetCoreTicker())
{
	FString Host;
	const bool bODSCEnabled = FParse::Value(FCommandLine::Get(), TEXT("-odschost="), Host);

	if (IsRunningCookOnTheFly() || bODSCEnabled)
	{
		FCoreDelegates::OnEnginePreExit.AddRaw(this, &FODSCManager::OnEnginePreExit);
		Thread = new FODSCThread(Host);
		Thread->StartThread();
	}
}

FODSCManager::~FODSCManager()
{
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
	StopThread();
}

void FODSCManager::OnEnginePreExit()
{
	StopThread();
}

void FODSCManager::StopThread()
{
	if (Thread)
	{
		Thread->StopThread();
		delete Thread;
		Thread = nullptr;
	}
}

bool FODSCManager::Tick(float DeltaSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FODSCManager_Tick);

	if (IsHandlingRequests())
	{
		Thread->Wakeup();

		TArray<FODSCMessageHandler*> CompletedThreadedRequests;
		Thread->GetCompletedRequests(CompletedThreadedRequests);

		// Finish and remove any completed requests
		for (FODSCMessageHandler* CompletedRequest : CompletedThreadedRequests)
		{
			check(CompletedRequest);
			ProcessCookOnTheFlyShaders(false, CompletedRequest->GetMeshMaterialMaps(), CompletedRequest->GetMaterialsToLoad(), CompletedRequest->GetGlobalShaderMap());
			delete CompletedRequest;
		}
		// keep ticking
		return true;
	}
	// stop ticking
	return false;
}

void FODSCManager::AddThreadedRequest(
	const TArray<FString>& MaterialsToCompile,
	const FString& ShaderTypesToLoad,
	EShaderPlatform ShaderPlatform,
	ERHIFeatureLevel::Type FeatureLevel,
	EMaterialQualityLevel::Type QualityLevel,
	ODSCRecompileCommand RecompileCommandType
)
{
	if (IsHandlingRequests())
	{
		Thread->AddRequest(MaterialsToCompile, ShaderTypesToLoad, ShaderPlatform, FeatureLevel, QualityLevel, RecompileCommandType);
	}
}

void FODSCManager::AddThreadedShaderPipelineRequest(
	EShaderPlatform ShaderPlatform,
	ERHIFeatureLevel::Type FeatureLevel,
	EMaterialQualityLevel::Type QualityLevel,
	const FString& MaterialName,
	const FString& VertexFactoryName,
	const FString& PipelineName,
	const TArray<FString>& ShaderTypeNames,
	int32 PermutationId
)
{
	if (IsHandlingRequests())
	{
		Thread->AddShaderPipelineRequest(ShaderPlatform, FeatureLevel, QualityLevel, MaterialName, VertexFactoryName, PipelineName, ShaderTypeNames, PermutationId);
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Queue.h"
#include "DistributedBuildControllerInterface.h"

class FUbaJobProcessor;
struct FTaskCommandData;

DECLARE_LOG_CATEGORY_EXTERN(LogUbaController, Log, Log);

class FUbaControllerModule : public IDistributedBuildController , public TSharedFromThis<FUbaControllerModule>
{
public:
	
	FUbaControllerModule();
	virtual ~FUbaControllerModule() override;
	
	UBACONTROLLER_API static FUbaControllerModule& Get();
	UBACONTROLLER_API virtual bool IsSupported() override final;

	UBACONTROLLER_API virtual const FString GetName() override final { return FString("UBA Controller"); };
	
	virtual void StartupModule() override final;
	virtual void ShutdownModule() override final;
	virtual void InitializeController() override final;
	virtual FString CreateUniqueFilePath() override final;
	virtual TFuture<FDistributedBuildTaskResult> EnqueueTask(const FTaskCommandData& CommandData) override final;
	
	void ReportJobProcessed(const FTaskResponse& InTaskResponse, FTask* CompileTask);
	void CleanWorkingDirectory() const;

	const FString& GetRootWorkingDirectory() const { return RootWorkingDirectory; }
	const FString& GetWorkingDirectory() const { return WorkingDirectory; }

	bool HasTasksDispatchedOrPending() const;

	// Queue of tasks submitted by the engine, but not yet dispatched to the controller.
	TQueue<FTask*, EQueueMode::Mpsc> PendingRequestedCompilationTasks;

private:
	void LoadDependencies();

	bool bSupported;
	bool bModuleInitialized;
	bool bControllerInitialized;
	
	FString RootWorkingDirectory;
	FString WorkingDirectory;

	TAtomic<int32> NextFileID;
	TAtomic<int32> NextTaskID;

	TSharedPtr<FUbaJobProcessor> JobDispatcherThread;
};

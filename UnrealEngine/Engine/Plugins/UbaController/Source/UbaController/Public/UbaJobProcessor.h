// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/PlatformAtomics.h"
#include "HAL/Runnable.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UbaExports.h"

class FUbaControllerModule;
class FUbaHordeAgentManager;
struct FTask;

class FUbaJobProcessor : public FRunnable, public TSharedFromThis<FUbaJobProcessor>
{
public:
	FUbaJobProcessor(FUbaControllerModule& InControllerModule);

	virtual ~FUbaJobProcessor() override;
	
	/** Main loop */
	virtual uint32 Run() override;
	
	/** Aborts the main loop as soon as possible */
	virtual void Stop() override { bForceStop = true; };
	
	/** Creates the threads and starts the main loop */
	void StartThread();
	bool ProcessOutputFile(FTask* CompileTask);

	void HandleUbaJobFinished(FTask* CompileTask);

	/** Used to know when this thread has finished the main loop */
	bool IsWorkDone() const { return bIsWorkDone;};

	void HandleTaskQueueUpdated(const FString& InputFileName);

	bool HasJobsInFlight() const;

private:

	void CalculateKnownInputs();
	void RunTaskWithUba(FTask* Task);

	void StartUba();
	void ShutDownUba();

	/** The runnable thread */
	FRunnableThread* Thread;
	
	FUbaControllerModule& ControllerModule;

	int32 MaxLocalParallelJobs = 0;
	
	/** Used to abort the current processing loop */
	TAtomic<bool> bForceStop;
	
	/** Used to abort the current processing loop */
	uint32 LastTimeCheckedForTasks;
	
	/** Used to stop the processing loop in the next main loop */
	FCriticalSection bShouldProcessJobsLock;
	bool bShouldProcessJobs;

	/** Set to true when the main loop finishes*/
	bool bIsWorkDone;

	uba::NetworkServer* UbaServer = nullptr;
	uba::Storage* UbaStorageServer = nullptr;
	uba::SessionServer* UbaSessionServer = nullptr;
	uba::Scheduler* UbaScheduler = nullptr;
	TUniquePtr<FUbaHordeAgentManager> HordeAgentManager;

	uint32 KnownInputsCount = 0;
	TArray<uba::tchar> KnownInputsBuffer;

	uba::CallbackLogWriter LogWriter;
};
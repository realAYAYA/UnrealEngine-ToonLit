// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Modules/ModuleInterface.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "Containers/Ticker.h"
#include "Misc/ScopeRWLock.h"

struct FChaosVDRecording;
class FText;

/* Option flags that controls what should be recorded when doing a full capture **/
enum class EChaosVDFullCaptureFlags : int32
{
	Geometry = 1 << 0,
	Particles = 1 << 1,
};
ENUM_CLASS_FLAGS(EChaosVDFullCaptureFlags)

DECLARE_MULTICAST_DELEGATE(FChaosVDRecordingStateChangedDelegate)
DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDCaptureRequestDelegate, EChaosVDFullCaptureFlags)
DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDRecordingStartFailedDelegate, const FText&)

class CHAOSVDRUNTIME_API FChaosVDRuntimeModule : public IModuleInterface
{
public:

	static FChaosVDRuntimeModule& Get();
	static bool IsLoaded();
	
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Starts a CVD recording by starting a Trace session. It will stop any existing trace session
	 * @param Args : Arguments array provided by the commandline. Used to determine if we want to record to file or a local trace server
	 */
	void StartRecording(TConstArrayView<FString> Args);
	
	/* Stops an active recording */
	void StopRecording();

	/** Returns true if we are currently recording a Physics simulation */
	bool IsRecording() const { return bIsRecording; }

	/** Returns a unique ID used to be used to identify CVD (Chaos Visual Debugger) data */
	int32 GenerateUniqueID();

	static FDelegateHandle RegisterRecordingStartedCallback(const FChaosVDRecordingStateChangedDelegate::FDelegate& InCallback)
	{
		FWriteScopeLock WriteLock(DelegatesRWLock);
		return RecordingStartedDelegate.Add(InCallback);
	}

	static FDelegateHandle RegisterRecordingStopCallback(const FChaosVDRecordingStateChangedDelegate::FDelegate& InCallback)
	{
		FWriteScopeLock WriteLock(DelegatesRWLock);
		return RecordingStopDelegate.Add(InCallback);
	}

	static FDelegateHandle RegisterRecordingStartFailedCallback(const FChaosVDRecordingStartFailedDelegate::FDelegate& InCallback)
	{
		FWriteScopeLock WriteLock(DelegatesRWLock);
		return RecordingStartFailedDelegate.Add(InCallback);
	}
	
	static FDelegateHandle RegisterFullCaptureRequestedCallback(const FChaosVDCaptureRequestDelegate::FDelegate& InCallback)
	{
		FWriteScopeLock WriteLock(DelegatesRWLock);
		return PerformFullCaptureDelegate.Add(InCallback);
	}

	static bool RemoveRecordingStartedCallback(const FDelegateHandle& InDelegateToRemove)
	{
		FWriteScopeLock WriteLock(DelegatesRWLock);
		return RecordingStartedDelegate.Remove(InDelegateToRemove);
	}

	static bool RemoveRecordingStopCallback(const FDelegateHandle& InDelegateToRemove)
	{
		FWriteScopeLock WriteLock(DelegatesRWLock);
		return RecordingStopDelegate.Remove(InDelegateToRemove);
	}

	static bool RemoveRecordingStartFailedCallback(const FDelegateHandle& InDelegateToRemove)
	{
		FWriteScopeLock WriteLock(DelegatesRWLock);
		return RecordingStartFailedDelegate.Remove(InDelegateToRemove);
	}
	
	static bool RemoveFullCaptureRequestedCallback(const FDelegateHandle& InDelegateToRemove)
	{
		FWriteScopeLock WriteLock(DelegatesRWLock);
		return PerformFullCaptureDelegate.Remove(InDelegateToRemove);
	}

	/** Returns the accumulated recording time in seconds since the recording started */
	float GetAccumulatedRecordingTime() const { return AccumulatedRecordingTime; }

	/** Returns the full path of the active recording file*/
	FString GetLastRecordingFileNamePath() const;

private:

	/** Stops the current Trace session */
	void StopTrace();
	/** Finds a valid file name for a new file - Used to generate the file name for the Trace recording */
	void GenerateRecordingFileName(FString& OutFileName);

	/** Queues a full Capture of the simulation on the next frame */
	bool RequestFullCapture(float DeltaTime);

	/** Queues a full Capture of the simulation on the next frame */
	bool RecordingTimerTick(float DeltaTime);

	/** Used to handle stop requests to the active trace session that were not done by us
	 * That is a possible scenario because Trace is shared by other In-Editor tools
	 */
	void HandleTraceStopRequest(FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination);

	bool bIsRecording = false;
	bool bRequestedStop = false;

	float AccumulatedRecordingTime = 0.0f;

	FTSTicker::FDelegateHandle FullCaptureRequesterHandle;
	FTSTicker::FDelegateHandle RecordingTimerHandle;

	static FChaosVDRecordingStateChangedDelegate RecordingStartedDelegate;
	static FChaosVDRecordingStateChangedDelegate RecordingStopDelegate;
	static FChaosVDRecordingStartFailedDelegate RecordingStartFailedDelegate;
	static FChaosVDCaptureRequestDelegate PerformFullCaptureDelegate;

	FThreadSafeCounter LastGeneratedID;

	FString LastRecordingFileNamePath;

	static FRWLock DelegatesRWLock;
};

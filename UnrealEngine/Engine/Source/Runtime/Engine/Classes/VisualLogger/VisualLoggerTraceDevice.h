// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLoggerTypes.h"

#if ENABLE_VISUAL_LOG

class FVisualLoggerTraceDevice : public FVisualLogDevice
{
public:
	static FVisualLoggerTraceDevice& Get()
	{
		static FVisualLoggerTraceDevice GDevice;
		return GDevice;
	}

	FVisualLoggerTraceDevice();
	virtual void Cleanup(bool bReleaseMemory = false) override;
	virtual void StartRecordingToFile(float TImeStamp) override;
	virtual void StopRecordingToFile(float TImeStamp) override;
	virtual void DiscardRecordingToFile() override;
	virtual void SetFileName(const FString& InFileName) override;
	virtual void Serialize(const class UObject* LogOwner, FName OwnerName, FName OwnerClassName, const FVisualLogEntry& LogEntry) override;
	virtual bool HasFlags(int32 InFlags) const override { return !!(InFlags & (EVisualLoggerDeviceFlags::CanSaveToFile | EVisualLoggerDeviceFlags::StoreLogsLocally)); }

};

#endif

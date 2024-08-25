// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLoggerTypes.h"

#if ENABLE_VISUAL_LOG

DECLARE_DELEGATE_TwoParams(FImmediateRenderDelegate, const UObject*, const FVisualLogEntry&);

class FVisualLoggerTraceDevice : public FVisualLogDevice
{
public:
	static ENGINE_API FVisualLoggerTraceDevice& Get()
	{
		static FVisualLoggerTraceDevice GDevice;
		return GDevice;
	}

	ENGINE_API FVisualLoggerTraceDevice();
	virtual void Cleanup(bool bReleaseMemory = false) override;
	virtual void StartRecordingToFile(double TimeStamp) override;
	virtual void StopRecordingToFile(double TimeStamp) override;
	virtual void DiscardRecordingToFile() override;
	virtual void SetFileName(const FString& InFileName) override;
	virtual void Serialize(const class UObject* LogOwner, FName OwnerName, FName OwnerClassName, const FVisualLogEntry& LogEntry) override;
	virtual bool HasFlags(int32 InFlags) const override { return !!(InFlags & (EVisualLoggerDeviceFlags::CanSaveToFile | EVisualLoggerDeviceFlags::StoreLogsLocally)); }

	FImmediateRenderDelegate ImmediateRenderDelegate;
};

#endif

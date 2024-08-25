// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StormSyncPackageDescriptor.h"
#include "Tasks/IStormSyncImportTask.h"

/** Import files from Buffer implementation for tasks that need delayed execution */
class FStormSyncImportBufferTask final : public IStormSyncImportSubsystemTask
{
public:
	explicit FStormSyncImportBufferTask(const FStormSyncPackageDescriptor& InPackageDescriptor, const TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& InBuffer)
		: PackageDescriptor(InPackageDescriptor)
		, Buffer(InBuffer)
	{
	}

	virtual void Run() override;

private:
	/** Metadata info about buffer being extracted */
	FStormSyncPackageDescriptor PackageDescriptor;
	
	/** Holds a shared ptr to the serialized buffer */
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> Buffer = nullptr;
};
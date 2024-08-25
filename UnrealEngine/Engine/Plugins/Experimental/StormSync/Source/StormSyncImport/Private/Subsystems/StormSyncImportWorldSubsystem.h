// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StormSyncCommonTypes.h"
#include "StormSyncPackageDescriptor.h"
#include "Subsystems/WorldSubsystem.h"
#include "StormSyncImportWorldSubsystem.generated.h"

/**
 * UStormSyncImportWorldSubsystem
 * 
 * **World** Subsystem for importing assets in -game mode, from a local exported storm sync buffer, or coming from a network request.
 *
 * Bulk of the import logic is handled by UStormSyncImportSubsystem, this world subsystem registers a delegate on FStormSyncCoreDelegates
 * OnRequestImportFile / 
 */

UCLASS()
class UStormSyncImportWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	void HandleImportFile(const FString& InFilename) const;
	void HandleImportBuffer(const FStormSyncPackageDescriptor& InPackageDescriptor, const FStormSyncBufferPtr& InBuffer) const;
};

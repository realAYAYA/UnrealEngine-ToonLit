// Copyright Epic Games, Inc. All Rights Reserved.


#include "Subsystems/StormSyncImportWorldSubsystem.h"

#include "Engine/World.h"
#include "StormSyncCoreDelegates.h"
#include "StormSyncImportLog.h"
#include "Subsystems/StormSyncImportSubsystem.h"
#include "Tasks/StormSyncImportBufferTask.h"
#include "Tasks/StormSyncImportFilesTask.h"

void UStormSyncImportWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogStormSyncImport, Verbose, TEXT("UStormSyncImportWorldSubsystem::Initialize (World: %s) - %s"), *GetNameSafe(GetWorld()), *GetName());

	FStormSyncCoreDelegates::OnRequestImportFile.AddUObject(this, &UStormSyncImportWorldSubsystem::HandleImportFile);
	FStormSyncCoreDelegates::OnRequestImportBuffer.AddUObject(this, &UStormSyncImportWorldSubsystem::HandleImportBuffer);
}

void UStormSyncImportWorldSubsystem::Deinitialize()
{
	Super::Deinitialize();
	UE_LOG(LogStormSyncImport, Verbose, TEXT("UStormSyncImportWorldSubsystem::Deinitialize (World: %s) - %s"), *GetNameSafe(GetWorld()), *GetName());

	FStormSyncCoreDelegates::OnRequestImportFile.RemoveAll(this);
	FStormSyncCoreDelegates::OnRequestImportBuffer.RemoveAll(this);
}

void UStormSyncImportWorldSubsystem::HandleImportFile(const FString& InFilename) const
{
	UE_LOG(LogStormSyncImport, Verbose, TEXT("UStormSyncImportWorldSubsystem::HandleImportFile InFilename: %s (World: %s)"), *InFilename, *GetNameSafe(GetWorld()));
	const UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogStormSyncImport, Error, TEXT("UStormSyncImportWorldSubsystem::ImportNextTick failed because of invalid world (World: %s)"), *GetNameSafe(GetWorld()));
		return;
	}

	UStormSyncImportSubsystem::Get().EnqueueImportTask(MakeShared<FStormSyncImportFilesTask>(InFilename), GetWorld());
}

void UStormSyncImportWorldSubsystem::HandleImportBuffer(const FStormSyncPackageDescriptor& InPackageDescriptor, const FStormSyncBufferPtr& InBuffer) const
{
	if (!InBuffer.IsValid())
	{
		UE_LOG(LogStormSyncImport, Error, TEXT("UStormSyncImportSubsystem::HandleImportBuffer failed from invalid buffer"));
		return;
	}
	
	UE_LOG(LogStormSyncImport, 
		Verbose,
		TEXT("UStormSyncImportWorldSubsystem::HandleImportBuffer InPackageDescriptor: %s, BufferSize: %d (World: %s)"),
		*InPackageDescriptor.ToString(),
		InBuffer->Num(),
		*GetNameSafe(GetWorld())
	);

	const UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogStormSyncImport, Error, TEXT("UStormSyncImportWorldSubsystem::HandleImportBuffer failed because of invalid world (World: %s)"), *GetNameSafe(GetWorld()));
		return;
	}

	UStormSyncImportSubsystem::Get().EnqueueImportTask(MakeShared<FStormSyncImportBufferTask>(InPackageDescriptor, InBuffer), GetWorld());
}

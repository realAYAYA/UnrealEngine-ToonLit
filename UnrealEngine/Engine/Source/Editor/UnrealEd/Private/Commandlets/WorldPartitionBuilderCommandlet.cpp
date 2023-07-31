// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/WorldPartitionBuilderCommandlet.h"

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "EditorWorldUtils.h"
#include "Logging/LogMacros.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformFileManager.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionBuilder.h"
#include "UObject/GCObjectScopeGuard.h"
#include "Trace/Trace.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionBuilderCommandlet, All, All);

UWorldPartitionBuilderCommandlet::UWorldPartitionBuilderCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

int32 UWorldPartitionBuilderCommandlet::Main(const FString& Params)
{
	FPackageSourceControlHelper PackageHelper;

	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionBuilderCommandlet::Main);

	UE_SCOPED_TIMER(TEXT("Execution"), LogWorldPartitionBuilderCommandlet, Display);

	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	if (Tokens.Num() != 1)
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Missing world name"));
		return 1;
	}

	if (Switches.Contains(TEXT("Verbose")))
	{
		LogWorldPartitionBuilderCommandlet.SetVerbosity(ELogVerbosity::Verbose);
	}

	// This will convert incomplete package name to a fully qualified path
	FString WorldFilename;
	if (!FPackageName::SearchForPackageOnDisk(Tokens[0], &Tokens[0], &WorldFilename))
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Unknown world '%s'"), *Tokens[0]);
		return 1;
	}

	// Parse builder class name
	FString BuilderClassName;
	if (!FParse::Value(FCommandLine::Get(), TEXT("Builder="), BuilderClassName, false))
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Invalid builder name."));
		return 1;
	}

	// Find builder class
	UClass* BuilderClass = FindFirstObject<UClass>(*BuilderClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
	if (!BuilderClass)
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Unknown builder %s."), *BuilderClassName);
		return 1;
	}

	// Load the world package
	UPackage* WorldPackage = LoadWorldPackageForEditor(Tokens[0]);
	if (!WorldPackage)
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Couldn't load package %s."), *Tokens[0]);
		return 1;
	}

	// Find the world in the given package
	UWorld* World = UWorld::FindWorldInPackage(WorldPackage);
	if (!World)
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("No world in specified package %s."), *Tokens[0]);
		return 1;
	}

	// Load configuration file
	FString WorldConfigFilename = FPackageName::LongPackageNameToFilename(World->GetPackage()->GetName(), TEXT(".ini"));
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*WorldConfigFilename))
	{
		LoadConfig(GetClass(), *WorldConfigFilename);
	}

	// Create builder instance
	UWorldPartitionBuilder* Builder = NewObject<UWorldPartitionBuilder>(GetTransientPackage(), BuilderClass);
	if (!Builder)
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Failed to create builder."));
		return false;
	}

	bool bResult;
	{
		FGCObjectScopeGuard BuilderGuard(Builder);
		bResult = Builder->RunBuilder(World);
	}

	// Save configuration file
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*WorldConfigFilename) ||
		!FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*WorldConfigFilename))
	{
		SaveConfig(CPF_Config, *WorldConfigFilename);
	}	

	return bResult ? 0 : 1;
}

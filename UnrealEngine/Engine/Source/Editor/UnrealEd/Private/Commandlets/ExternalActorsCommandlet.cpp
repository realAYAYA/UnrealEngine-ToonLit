// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/ExternalActorsCommandlet.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PackageHelperFunctions.h"

DEFINE_LOG_CATEGORY_STATIC(LogExternalActorsCommandlet, All, All);

UExternalActorsCommandlet::UExternalActorsCommandlet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

UWorld* UExternalActorsCommandlet::LoadWorld(const FString& LevelToLoad)
{
	SET_WARN_COLOR(COLOR_WHITE);
	UE_LOG(LogExternalActorsCommandlet, Log, TEXT("Loading level %s."), *LevelToLoad);
	CLEAR_WARN_COLOR();

	UPackage* MapPackage = LoadPackage(nullptr, *LevelToLoad, LOAD_None);
	if (!MapPackage)
	{
		UE_LOG(LogExternalActorsCommandlet, Error, TEXT("Error loading %s."), *LevelToLoad);
		return nullptr;
	}

	return UWorld::FindWorldInPackage(MapPackage);
}

int32 UExternalActorsCommandlet::Main(const FString& Params)
{
	FPackageSourceControlHelper PackageHelper;

	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	// Need at least the level to convert
	if (Tokens.Num() < 1)
	{
		UE_LOG(LogExternalActorsCommandlet, Error, TEXT("Bad parameters"));
		return 1;
	}

	const bool bRepair = Switches.Contains(TEXT("repair"));

	if (!FPackageName::SearchForPackageOnDisk(Tokens[0], &Tokens[0]))
	{
		UE_LOG(LogExternalActorsCommandlet, Error, TEXT("Unknown level '%s'"), *Tokens[0]);
		return 1;
	}

	// Load world
	UWorld* MainWorld = LoadWorld(Tokens[0]);
	if (!MainWorld)
	{
		UE_LOG(LogExternalActorsCommandlet, Error, TEXT("Unknown world '%s'"), *Tokens[0]);
		return 1;
	}

	// Validate external actors
	FString ExternalActorsPath = ULevel::GetExternalActorsPath(Tokens[0]);
	FString ExternalActorsFilePath = FPackageName::LongPackageNameToFilename(ExternalActorsPath);

	// Look for duplicated actor GUIDs
	TMap<FGuid, AActor*> ActorGuids;

	TSet<UPackage*> PackagesToSave;
	TArray<FString> PackagesToDelete;	
	if (IFileManager::Get().DirectoryExists(*ExternalActorsFilePath))
	{
		bool bResult = IFileManager::Get().IterateDirectoryRecursively(*ExternalActorsFilePath, [this, bRepair, &PackagesToSave, &PackagesToDelete, &ActorGuids](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (!bIsDirectory)
			{
				FString Filename(FilenameOrDirectory);
				if (Filename.EndsWith(FPackageName::GetAssetPackageExtension()))
				{
					AActor* MainPackageActor = nullptr;
					AActor* PotentialMainPackageActor = nullptr;

					const FString PackageName = FPackageName::FilenameToLongPackageName(*Filename);
					if (UPackage* Package = LoadPackage(nullptr, *Filename, LOAD_None, nullptr, nullptr))
					{
						ForEachObjectWithPackage(Package, [&MainPackageActor, &PotentialMainPackageActor](UObject* Object)
						{
							if (AActor* Actor = Cast<AActor>(Object))
							{
								if (Actor->IsMainPackageActor())
								{
									MainPackageActor = Actor;
									PotentialMainPackageActor = nullptr;
								}
								else if (!MainPackageActor)
								{
									if (!Actor->IsChildActor())
									{
										PotentialMainPackageActor = Actor;
									}
								}
							}
							return true;
						});
					}

					if (!MainPackageActor)
					{
						UE_LOG(LogExternalActorsCommandlet, Error, TEXT("Missing main actor for file '%s'"), *Filename);

						if (bRepair)
						{
							if (PotentialMainPackageActor)
							{
								PotentialMainPackageActor->SetPackageExternal(false);
								PotentialMainPackageActor->SetPackageExternal(true);
								
								UPackage* PackageToSave = PotentialMainPackageActor->GetPackage();
								PackagesToSave.Add(PackageToSave);

								MainPackageActor = PotentialMainPackageActor;
							}

							PackagesToDelete.Add(Filename);
						}
					}

					if (MainPackageActor)
					{
						if (ActorGuids.Contains(MainPackageActor->GetActorGuid()))
						{
							UE_LOG(LogExternalActorsCommandlet, Error, TEXT("Duplicated actor guid for file '%s'"), *Filename);

							if (bRepair)
							{
								FSetActorGuid SetActorGuid(MainPackageActor, FGuid::NewGuid());

								UPackage* PackageToSave = MainPackageActor->GetPackage();
								PackagesToSave.Add(PackageToSave);
							}
						}
						else
						{
							ActorGuids.Add(MainPackageActor->GetActorGuid(), MainPackageActor);
						}
					}
				}
				else
				{
					UE_LOG(LogExternalActorsCommandlet, Error, TEXT("Invalid actor file '%s'"), *Filename);

					if (bRepair)
					{
						PackagesToDelete.Add(Filename);
					}
				}
			}
			return true;
		});
	}

	for (UPackage* PackageToSave: PackagesToSave)
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		FString PackageFileName = SourceControlHelpers::PackageFilename(PackageToSave);

		if (PackageHelper.Checkout(PackageToSave))
		{
			if (UPackage::SavePackage(PackageToSave, nullptr, *PackageFileName, SaveArgs))
			{
				PackageHelper.AddToSourceControl(PackageToSave);
			}
		}
	}

	CollectGarbage(RF_NoFlags);

	for (const FString& PackageToDelete : PackagesToDelete)
	{
		PackageHelper.Delete(*PackageToDelete);
	}

	return 0;
}
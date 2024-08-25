// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MapTestSpawner.h"

#if ENABLE_MAPSPAWNER_TEST
#include "Commands/TestCommands.h"
#include "Tests/AutomationCommon.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "HAL/FileManager.h"
#include "LevelEditor.h"
#include "LevelEditorSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEdGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogMapTest, Log, All);

namespace {

static const FString TempMapDirectory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("CQTestMapTemp"));

/**
 * Generates a unique random 8 character map name.
 */
FString GenerateUniqueMapName()
{
	FString UniqueMapName = FGuid::NewGuid().ToString();
	UniqueMapName.LeftInline(8);

	return UniqueMapName;
}

/**
 * Cleans up all created resources.
 */
void CleanupTempResources()
{
	bool bDirectoryMustExist = true;
	bool bRemoveRecursively = true;
	bool bWasDeleted = IFileManager::Get().DeleteDirectory(*TempMapDirectory, bDirectoryMustExist, bRemoveRecursively);
	check(bWasDeleted);
}

} //anonymous

FMapTestSpawner::FMapTestSpawner(const FString& MapDirectory, const FString& MapName) : MapDirectory(MapDirectory) , MapName(MapName)
{
	// Register Map Change Events
	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	MapChangedHandle = LevelEditor.OnMapChanged().AddRaw(this, &FMapTestSpawner::OnMapChanged);
}

TUniquePtr<FMapTestSpawner> FMapTestSpawner::CreateFromTempLevel(FTestCommandBuilder& InCommandBuilder)
{
	if (IsValid(GUnrealEd->PlayWorld))
	{
		UE_LOG(LogMapTest, Verbose, TEXT("Active PIE session '%s' needs to be shutdown before a creation of a new level can occur."), *GUnrealEd->PlayWorld->GetMapName());
		GUnrealEd->EndPlayMap();
	}

	FString MapName = GenerateUniqueMapName();
	FString MapPath = FPaths::Combine(TempMapDirectory, MapName);
	FString NewLevelPackage = FPackageName::FilenameToLongPackageName(MapPath);

	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
	bool bWasTempLevelCreated = LevelEditorSubsystem->NewLevel(NewLevelPackage);
	check(bWasTempLevelCreated);

	TUniquePtr<FMapTestSpawner> Spawner = MakeUnique<FMapTestSpawner>(TempMapDirectory, MapName);
	InCommandBuilder.OnTearDown([&]() {
		// Create a new map to free up the reference to the map used during testing before cleaning up all temporary resources
		FAutomationEditorCommonUtils::CreateNewMap();
		CleanupTempResources();
	});
	return MoveTemp(Spawner);
}

void FMapTestSpawner::AddWaitUntilLoadedCommand(FAutomationTestBase* TestRunner)
{
	check(PieWorld == nullptr);

	FString PackagePath;
	const FString Path = FPaths::Combine(MapDirectory, MapName);
	bool bPackageExists = FPackageName::DoesPackageExist(Path, &PackagePath);
	check(bPackageExists);

	bool bOpened = AutomationOpenMap(PackagePath);
	check(bOpened);

	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(*TestRunner, [&]() -> bool {
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (((Context.WorldType == EWorldType::PIE) || (Context.WorldType == EWorldType::Game)) && (Context.World() != nullptr))
			{
				PieWorld = Context.World();
				return true;
			}
		}

		return false;
	}));
}

UWorld* FMapTestSpawner::CreateWorld()
{
	checkf(PieWorld, TEXT("Must call AddWaitUntilLoadedCommand in BEFORE_TEST"));
	return PieWorld;
}

APawn* FMapTestSpawner::FindFirstPlayerPawn()
{
	return GetWorld().GetFirstPlayerController()->GetPawn();
}

void FMapTestSpawner::OnMapChanged(UWorld* World, EMapChangeType ChangeType)
{
	if (PieWorld && ChangeType == EMapChangeType::TearDownWorld)
	{
		UE_LOG(LogMapTest, Verbose, TEXT("Map used by the Spawner has been changed."));
		GameWorld = nullptr;
		PieWorld = nullptr;

		FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.OnMapChanged().Remove(MapChangedHandle);
	}
}

#endif // ENABLE_MAPSPAWNER_TEST
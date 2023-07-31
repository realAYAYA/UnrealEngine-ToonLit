// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionBuilder.h"

#include "CoreMinimal.h"
#include "Editor.h"
#include "EditorWorldUtils.h"
#include "EngineModule.h"
#include "HAL/PlatformFileManager.h"
#include "StaticMeshCompiler.h"
#include "Engine/World.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Math/IntVector.h"
#include "UObject/SavePackage.h"
#include "Algo/Transform.h"

#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionBuilder, All, All);

namespace FWorldPartitionBuilderLog
{
	void Error(bool bErrorsAsWarnings, const FString& Msg)
	{
		if (bErrorsAsWarnings)
		{
			UE_LOG(LogWorldPartitionBuilder, Warning, TEXT("%s"), *Msg);
		}
		else
		{
			UE_LOG(LogWorldPartitionBuilder, Error, TEXT("%s"), *Msg);
		}
	}
}

FCellInfo::FCellInfo()
	: Location(ForceInitToZero)
	, Bounds(ForceInitToZero)
	, EditorBounds(ForceInitToZero)
	, IterativeCellSize(102400)
{
}

UWorldPartitionBuilder::UWorldPartitionBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoSubmit = FParse::Param(FCommandLine::Get(), TEXT("AutoSubmit"));
	if (bAutoSubmit)
	{
		FParse::Value(FCommandLine::Get(), TEXT("AutoSubmitTags="), AutoSubmitTags);
	}
}

UWorld::InitializationValues UWorldPartitionBuilder::GetWorldInitializationValues() const
{
	UWorld::InitializationValues IVS;
	IVS.RequiresHitProxies(false);
	IVS.ShouldSimulatePhysics(false);
	IVS.EnableTraceCollision(false);
	IVS.CreateNavigation(false);
	IVS.CreateAISystem(false);
	IVS.AllowAudioPlayback(false);
	IVS.CreatePhysicsScene(true);
	return IVS;
}

bool UWorldPartitionBuilder::RunBuilder(UWorld* World)
{
	// Load configuration file & builder configuration
	const FString WorldConfigFilename = FPackageName::LongPackageNameToFilename(World->GetPackage()->GetName(), TEXT(".ini"));
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*WorldConfigFilename))
	{
		LoadConfig(GetClass(), *WorldConfigFilename);
	}

	// Validate builder settings
	if (IsRunningCommandlet() && RequiresCommandletRendering() && !IsAllowCommandletRendering())
	{
		UE_LOG(LogWorldPartitionBuilder, Error, TEXT("The option \"-AllowCommandletRendering\" must be provided for the %s process to work"), *GetClass()->GetName());
		return false;
	}

	FPackageSourceControlHelper SCCHelper;

	// Perform builder pre world initialisation
	if (!PreWorldInitialization(SCCHelper))
	{
		UE_LOG(LogWorldPartitionBuilder, Error, TEXT("PreWorldInitialization failed"));
		return false;
	}

	bool bResult = true;
	// Setup the world
	{
		TUniquePtr<FScopedEditorWorld> EditorWorld;
		if (World->bIsWorldInitialized)
		{
			// Skip initialization, but ensure we're dealing with an editor world.
			check(World->WorldType == EWorldType::Editor);
		}
		else
		{
			EditorWorld = MakeUnique<FScopedEditorWorld>(World, GetWorldInitializationValues());
		}

		// Make sure the world is partitioned
		if (UWorld::IsPartitionedWorld(World))
		{
			// Ensure the world has a valid world partition.
			UWorldPartition* WorldPartition = World->GetWorldPartition();
			check(WorldPartition);

			FWorldContext& WorldContext = GEditor->GetEditorWorldContext(true /*bEnsureIsGWorld*/);
			WorldContext.SetCurrentWorld(World);
			UWorld* PrevGWorld = GWorld;
			GWorld = World;

			// Run builder
			bResult = Run(World, SCCHelper);

			// Restore previous world
			WorldContext.SetCurrentWorld(PrevGWorld);
			GWorld = PrevGWorld;

			// Save default configuration
			if (bResult)
			{
				if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*WorldConfigFilename) ||
					!FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*WorldConfigFilename))
				{
					SaveConfig(CPF_Config, *WorldConfigFilename);
				}
			}
		}
		else
		{
			UE_LOG(LogWorldPartitionBuilder, Error, TEXT("WorldPartition builders only works on partitioned maps."));
			bResult = false;
		}
	}

	if (bResult)
	{
		bResult = PostWorldTeardown(SCCHelper);
	}

	return bResult;
}

FWorldBuilderCellCoord FCellInfo::GetCellCoord(const FVector& InPos, const int32 InCellSize)
{
	return FWorldBuilderCellCoord(
		FMath::FloorToInt(InPos.X / InCellSize),
		FMath::FloorToInt(InPos.Y / InCellSize),
		FMath::FloorToInt(InPos.Z / InCellSize)
	);
}

FWorldBuilderCellCoord FCellInfo::GetCellCount(const FBox& InBounds, const int32 InCellSize)
{
	const FWorldBuilderCellCoord MinCellCoords = GetCellCoord(InBounds.Min, InCellSize);
	const FWorldBuilderCellCoord MaxCellCoords = FWorldBuilderCellCoord(FMath::CeilToInt(InBounds.Max.X / InCellSize),
												FMath::CeilToInt(InBounds.Max.Y / InCellSize),
												FMath::CeilToInt(InBounds.Max.Z / InCellSize));
	return MaxCellCoords - MinCellCoords;
}

bool UWorldPartitionBuilder::Run(UWorld* World, FPackageSourceControlHelper& PackageHelper)
{
	// Notify derived classes that partition building process starts
	bool bResult = PreRun(World, PackageHelper);

	UWorldPartition* WorldPartition = World->GetWorldPartition();

	// Properly Setup DataLayers for Builder
	UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(World);
	
	// Load Data Layers
	bool bUpdateEditorCells = false;
	DataLayerSubsystem->ForEachDataLayer([&bUpdateEditorCells, this](UDataLayerInstance* DataLayer)
	{
		const FName DataLayerShortName(DataLayer->GetDataLayerShortName());

		// Load all Non Excluded Data Layers + Non DynamicallyLoaded Data Layers + Initially Active Data Layers + Data Layers provided by builder
		const bool bLoadedInEditor = !ExcludedDataLayerShortNames.Contains(DataLayerShortName) && ((bLoadNonDynamicDataLayers && !DataLayer->IsRuntime()) ||
										(bLoadInitiallyActiveDataLayers && DataLayer->GetInitialRuntimeState() == EDataLayerRuntimeState::Activated) ||
									DataLayerShortNames.Contains(DataLayerShortName));
		if (DataLayer->IsLoadedInEditor() != bLoadedInEditor)
		{
			bUpdateEditorCells = true;
			DataLayer->SetIsLoadedInEditor(bLoadedInEditor, /*bFromUserChange*/false);
			if (RequiresCommandletRendering() && bLoadedInEditor)
			{
				DataLayer->SetIsInitiallyVisible(true);
			}
		}
			
		UE_LOG(LogWorldPartitionBuilder, Display, TEXT("DataLayer '%s' Loaded: %d"), *UDataLayerInstance::GetDataLayerText(DataLayer).ToString(), bLoadedInEditor ? 1 : 0);
			
		return true;
	});
	
	if (bUpdateEditorCells)
	{
		UE_LOG(LogWorldPartitionBuilder, Display, TEXT("DataLayer load state changed refreshing editor cells"));
		FDataLayersEditorBroadcast::StaticOnActorDataLayersEditorLoadingStateChanged(false);
	}

	const ELoadingMode LoadingMode = GetLoadingMode();
	FCellInfo CellInfo;

	CellInfo.EditorBounds = IterativeWorldBounds.IsValid ? IterativeWorldBounds : WorldPartition->GetEditorWorldBounds();
	CellInfo.IterativeCellSize = IterativeCellSize;

	if ((LoadingMode == ELoadingMode::IterativeCells) || (LoadingMode == ELoadingMode::IterativeCells2D))
	{
		// do partial loading loop that calls RunInternal
		const FWorldBuilderCellCoord MinCellCoords = FCellInfo::GetCellCoord(CellInfo.EditorBounds.Min, IterativeCellSize);
		const FWorldBuilderCellCoord NumCellsIterations = FCellInfo::GetCellCount(CellInfo.EditorBounds, IterativeCellSize);
		const FWorldBuilderCellCoord BeginCellCoords = MinCellCoords;
		const FWorldBuilderCellCoord EndCellCoords = BeginCellCoords + NumCellsIterations;

		auto CanIterateZ = [&BeginCellCoords, &EndCellCoords, LoadingMode](const bool bInResult, const int64 InZ) -> bool
		{
			if (LoadingMode == ELoadingMode::IterativeCells2D)
			{
				return bInResult && (InZ == BeginCellCoords.Z);
			}

			return bInResult && (InZ < EndCellCoords.Z);
		};

		const int64 IterationCount = ((LoadingMode == ELoadingMode::IterativeCells2D) ? 1 : NumCellsIterations.Z) * NumCellsIterations.Y * NumCellsIterations.X;
		int32 IterationIndex = 0;

		UE_LOG(LogWorldPartitionBuilder, Display, TEXT("Iterative Cell Mode"));
		UE_LOG(LogWorldPartitionBuilder, Display, TEXT("Cell Size %d"), IterativeCellSize);
		UE_LOG(LogWorldPartitionBuilder, Display, TEXT("Cell Overlap %d"), IterativeCellOverlapSize);
		UE_LOG(LogWorldPartitionBuilder, Display, TEXT("WorldBounds: Min %s, Max %s"), *CellInfo.EditorBounds.Min.ToString(), *CellInfo.EditorBounds.Max.ToString());
		UE_LOG(LogWorldPartitionBuilder, Display, TEXT("Iteration Count: %d"), IterationCount);
		
		TArray<TUniquePtr<IWorldPartitionActorLoaderInterface::ILoaderAdapter>> LoaderAdapters;

		for (int64 z = BeginCellCoords.Z; CanIterateZ(bResult, z); z++)
		{
			for (int64 y = BeginCellCoords.Y; bResult && (y < EndCellCoords.Y); y++)
			{
				for (int64 x = BeginCellCoords.X; bResult && (x < EndCellCoords.X); x++)
				{
					IterationIndex++;
					
					if (ShouldSkipCell(FWorldBuilderCellCoord(x, y, z)))
					{
						UE_LOG(LogWorldPartitionBuilder, Display, TEXT("[%d / %d] Processing cells... (skipped)"), IterationIndex, IterationCount);
						continue;
					}
					
					UE_LOG(LogWorldPartitionBuilder, Display, TEXT("[%d / %d] Processing cells..."), IterationIndex, IterationCount);

					FVector Min(x * IterativeCellSize, y * IterativeCellSize, z * IterativeCellSize);
					FVector Max = Min + FVector(IterativeCellSize);

					if (LoadingMode == ELoadingMode::IterativeCells2D)
					{
						Min.Z = CellInfo.EditorBounds.Min.Z;
						Max.Z = CellInfo.EditorBounds.Max.Z;
					}

					FBox BoundsToLoad(Min, Max);
					BoundsToLoad = BoundsToLoad.ExpandBy(IterativeCellOverlapSize);

					CellInfo.Location = FWorldBuilderCellCoord(x, y, z);
					CellInfo.Bounds = BoundsToLoad;

					UE_LOG(LogWorldPartitionBuilder, Verbose, TEXT("Loading Bounds: Min %s, Max %s"), *BoundsToLoad.Min.ToString(), *BoundsToLoad.Max.ToString());

					LoaderAdapters.Emplace_GetRef(MakeUnique<FLoaderAdapterShape>(World, BoundsToLoad, TEXT("Loaded Region")))->Load();

					// Simulate an engine tick
					FWorldPartitionHelpers::FakeEngineTick(World);

					bResult = RunInternal(World, CellInfo, PackageHelper);

					if (FWorldPartitionHelpers::HasExceededMaxMemory())
					{
						LoaderAdapters.Empty();
						FWorldPartitionHelpers::DoCollectGarbage();
					}
				}
			}
		}
	}
	else
	{
		TUniquePtr<FLoaderAdapterShape> LoaderAdapterShape;
		
		if (LoadingMode == ELoadingMode::EntireWorld)
		{
			CellInfo.Bounds = FBox(FVector(-HALF_WORLD_MAX, -HALF_WORLD_MAX, -HALF_WORLD_MAX), FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX));
			LoaderAdapterShape = MakeUnique<FLoaderAdapterShape>(World, CellInfo.Bounds, TEXT("Loaded Region"));
			LoaderAdapterShape->Load();
		}
		else
		{
			CellInfo.Bounds.Init();
		}

		bResult = RunInternal(World, CellInfo, PackageHelper);

		if (LoaderAdapterShape)
		{
			LoaderAdapterShape.Reset();
		}
	}

	return PostRun(World, PackageHelper, bResult);
}

bool UWorldPartitionBuilder::SavePackages(const TArray<UPackage*>& Packages, FPackageSourceControlHelper& PackageHelper, bool bErrorsAsWarnings)
{
	if (Packages.IsEmpty())
	{
		return true;
	}
	UE_LOG(LogWorldPartitionBuilder, Display, TEXT("Saving %d packages..."), Packages.Num());

	const TArray<FString> PackageFilenames = SourceControlHelpers::PackageFilenames(Packages);
	
	TArray<FString> PackagesToCheckout;
	TArray<FString> PackagesToAdd;
	TArray<FString> PackageNames;

	PackageNames.Reserve(Packages.Num());
	Algo::Transform(Packages, PackageNames, [](UPackage* InPackage) { return InPackage->GetName(); });
	bool bSuccess = PackageHelper.GetDesiredStatesForModification(PackageNames, PackagesToCheckout, PackagesToAdd, bErrorsAsWarnings);
	if (!bSuccess && !bErrorsAsWarnings)
	{
		return false;
	}
		
	if (PackagesToCheckout.Num())
	{
		if (!PackageHelper.Checkout(PackagesToCheckout, bErrorsAsWarnings))
		{
			bSuccess = false;
			if (!bErrorsAsWarnings)
			{
				return false;
			}
		}
	}

	for (int PackageIndex = 0; PackageIndex < Packages.Num(); ++PackageIndex)
	{
		const FString& PackageFilename = PackageFilenames[PackageIndex];
		// Check readonly flag in case some checkouts failed
		if (!IPlatformFile::GetPlatformPhysical().IsReadOnly(*PackageFilename))
		{
			// Save package
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			if (!UPackage::SavePackage(Packages[PackageIndex], nullptr, *PackageFilenames[PackageIndex], SaveArgs))
			{
				bSuccess = false;
				FWorldPartitionBuilderLog::Error(bErrorsAsWarnings, FString::Printf(TEXT("Error saving package %s."), *Packages[PackageIndex]->GetName()));
				if(!bErrorsAsWarnings)
				{
					return false;
				}
			}
		}
		else
		{
			check(bErrorsAsWarnings);
		}
	}

	if (PackagesToAdd.Num())
	{
		if (!PackageHelper.AddToSourceControl(PackagesToAdd, bErrorsAsWarnings))
		{
			return false;
		}
	}

	return bSuccess;
}

bool UWorldPartitionBuilder::DeletePackages(const TArray<UPackage*>& Packages, FPackageSourceControlHelper& PackageHelper, bool bErrorsAsWarnings)
{
	TArray<FString> PackagesNames;
	Algo::Transform(Packages, PackagesNames, [](UPackage* InPackage) { return InPackage->GetName(); });
	return DeletePackages(PackagesNames, PackageHelper, bErrorsAsWarnings);
}

bool UWorldPartitionBuilder::DeletePackages(const TArray<FString>& PackageNames, FPackageSourceControlHelper& PackageHelper, bool bErrorsAsWarnings)
{
	if (PackageNames.Num() > 0)
	{
		UE_LOG(LogWorldPartitionBuilder, Display, TEXT("Deleting %d packages..."), PackageNames.Num());
		return PackageHelper.Delete(PackageNames, bErrorsAsWarnings);
	}

	return true;
}

bool UWorldPartitionBuilder::AutoSubmitPackages(const TArray<UPackage*>& InModifiedPackages, const FString& InChangelistDescription) const
{
	TArray<FString> ModifiedFiles;
	Algo::Transform(InModifiedPackages, ModifiedFiles, [](const UPackage* InPackage) { return USourceControlHelpers::PackageFilename(InPackage); });
	return AutoSubmitFiles(ModifiedFiles, InChangelistDescription);
}

bool UWorldPartitionBuilder::AutoSubmitFiles(const TArray<FString>& InModifiedFiles, const FString& InChangelistDescription) const
{
	bool bSucceeded = true;

	if (bAutoSubmit)
	{
		UE_LOG(LogWorldPartitionBuilder, Display, TEXT("Submitting changes to source control..."));

		if (!InModifiedFiles.IsEmpty())
		{
			FText ChangelistDescription = FText::FromString(FString::Printf(TEXT("%s\nBased on CL %d\n%s"), *InChangelistDescription, FEngineVersion::Current().GetChangelist(), *AutoSubmitTags));

			TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
			CheckInOperation->SetDescription(ChangelistDescription);
			if (ISourceControlModule::Get().GetProvider().Execute(CheckInOperation, InModifiedFiles) != ECommandResult::Succeeded)
			{
				UE_LOG(LogWorldPartitionBuilder, Error, TEXT("Failed to submit changes to source control."));
				bSucceeded = false;
			}
			else
			{
				UE_LOG(LogWorldPartitionBuilder, Display, TEXT("Submitted changes to source control"));
			}
		}
		else
		{
			UE_LOG(LogWorldPartitionBuilder, Display, TEXT("No files to submit!"));
		}
	}

	return bSucceeded;
}

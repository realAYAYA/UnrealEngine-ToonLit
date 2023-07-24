// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PackageSourceControlHelper.h"
#include "Engine/World.h"
#include "WorldPartitionBuilder.generated.h"

typedef UE::Math::TIntVector3<int64> FWorldBuilderCellCoord;

/**
 * Structure containing information about a World Partition Builder cell
 */
struct FCellInfo
{
	FCellInfo();

	/**
	 * Location of the cell, expressed inside World Partition Builder space
	 * (floor(Coordinate) / IterativeCellSize)
	 */
	FWorldBuilderCellCoord Location;

	/** Bounds of the cell */
	FBox Bounds;

	/** Whole space */
	FBox EditorBounds;

	/** The size of a cell used by the World Partition Builder */
	int32 IterativeCellSize;

	static UNREALED_API FWorldBuilderCellCoord GetCellCoord(const FVector& InPos, const int32 InCellSize);
	static UNREALED_API FWorldBuilderCellCoord GetCellCount(const FBox& InBounds, const int32 InCellSize);
};

UCLASS(Abstract, Config=Engine)
class UNREALED_API UWorldPartitionBuilder : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	enum ELoadingMode
	{
		Custom,
		EntireWorld,
		IterativeCells,
		IterativeCells2D,
	};

	bool RunBuilder(UWorld* World);

	virtual bool RequiresCommandletRendering() const PURE_VIRTUAL(UWorldPartitionBuilder::RequiresCommandletRendering, return false;);
	virtual ELoadingMode GetLoadingMode() const PURE_VIRTUAL(UWorldPartitionBuilder::GetLoadingMode, return ELoadingMode::Custom;);
	
	bool Run(UWorld* World, FPackageSourceControlHelper& PackageHelper);

	virtual bool PreWorldInitialization(FPackageSourceControlHelper& PackageHelper) { return true; }

	static bool SavePackages(const TArray<UPackage*>& Packages, FPackageSourceControlHelper& PackageHelper, bool bErrorsAsWarnings = false);
	static bool DeletePackages(const TArray<UPackage*>& Packages, FPackageSourceControlHelper& PackageHelper, bool bErrorsAsWarnings = false);
	static bool DeletePackages(const TArray<FString>& PackageNames, FPackageSourceControlHelper& PackageHelper, bool bErrorsAsWarnings = false);

protected:
	/**
	 * Overridable method for derived classes to perform operations when world builder process starts.
	 * This is called before loading data (e.g. data layers, editor cells) and before calling `RunInternal`.
	 */
	virtual bool PreRun(UWorld* World, FPackageSourceControlHelper& PackageHelper) { return true; }

	virtual bool RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper) PURE_VIRTUAL(UWorldPartition::RunInternal, return false;);

	/**
	 * Overridable method for derived classes to perform operations when world builder process completes.
	 * This is called after loading all data (e.g. data layers, editor cells) and after calling `RunInternal` for all editor cells.
	 */
	virtual bool PostRun(UWorld* World, FPackageSourceControlHelper& PackageHelper, const bool bInRunSuccess) { return true; }

	/**
	 * Overridable method for derived classes to perform operations when world builder has unloaded the world.
	 */
	virtual bool PostWorldTeardown(FPackageSourceControlHelper& PackageHelper) { return true; }

	/**
	 * When using EloadingMode::IterativeCells/IterativeCells2D return true to skip processing of cell.
	 */
	virtual bool ShouldSkipCell(const FWorldBuilderCellCoord& CellCoord) const { return false; }

	bool AutoSubmitFiles(const TArray<FString>& InModifiedFiles, const FString& InChangelistDescription) const;
	bool AutoSubmitPackages(const TArray<UPackage*>& InModifiedPackages, const FString& InChangelistDescription) const;

	virtual UWorld::InitializationValues GetWorldInitializationValues() const;

	int32 IterativeCellSize = 102400;
	int32 IterativeCellOverlapSize = 0;
	FBox  IterativeWorldBounds;

	TSet<FName> DataLayerShortNames;
	TSet<FName> ExcludedDataLayerShortNames;
	bool bLoadNonDynamicDataLayers = true;
	bool bLoadInitiallyActiveDataLayers = true;

	bool bAutoSubmit = false;
	FString AutoSubmitTags;
};
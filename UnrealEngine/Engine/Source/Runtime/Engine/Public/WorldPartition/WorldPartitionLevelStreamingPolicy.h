// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * WorldPartitionLevelStreamingPolicy
 *
 * World Partition Streaming Policy that handles load/unload of streaming cell using a corresponding Streaming Level and 
 * a Level that will contain all streaming cell's content.
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionStreamingPolicy.h"
#include "WorldPartitionLevelStreamingPolicy.generated.h"

enum class EWorldPartitionRuntimeCellState : uint8;

UCLASS()
class UWorldPartitionLevelStreamingPolicy : public UWorldPartitionStreamingPolicy
{
	GENERATED_BODY()

public:
	virtual void DrawRuntimeCellsDetails(class UCanvas* Canvas, FVector2D& Offset) override;
	virtual void DrawStreamingStatusLegend(UCanvas* Canvas, FVector2D& Offset) override;
	virtual bool IsStreamingCompleted(const FWorldPartitionStreamingSource* InStreamingSource) const override;

#if WITH_EDITOR
	virtual TSubclassOf<class UWorldPartitionRuntimeCell> GetRuntimeCellClass() const override;
	virtual void PrepareActorToCellRemapping() override;
	virtual void RemapSoftObjectPath(FSoftObjectPath& ObjectPath) override;
	static FString GetCellPackagePath(const FName& InCellName, const UWorld* InWorld);
#endif

	virtual UObject* GetSubObject(const TCHAR* SubObjectPath) override;

protected:
	virtual int32 GetCellLoadingCount() const override;

	void ForEachActiveRuntimeCell(TFunctionRef<void(const UWorldPartitionRuntimeCell*)> Func) const;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TMap<FName, FName> ActorToCellRemapping;
#endif

	UPROPERTY()
	TMap<FName, FName> SubObjectsToCellRemapping;
};
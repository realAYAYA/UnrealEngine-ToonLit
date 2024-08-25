// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldGridPreviewer.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "RuntimePartitionLHGrid.generated.h"

UCLASS()
class ENGINE_API URuntimePartitionLHGrid : public URuntimePartition
{
	GENERATED_BODY()

	friend class UWorldPartitionRuntimeHashSet;
	friend struct FFortWorldPartitionUtils;

public:
#if WITH_EDITOR
	//~ Begin UObject Interface.
	virtual void PreEditChange(FProperty* InPropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	//~ End UObject Interface.

	//~ Begin URuntimePartition interface
	virtual bool SupportsHLODs() const override { return true; }
	virtual void InitHLODRuntimePartitionFrom(const URuntimePartition* InRuntimePartition, int32 InHLODIndex);
	virtual void SetDefaultValues() override;
	virtual bool IsValidPartitionTokens(const TArray<FName>& InPartitionTokens) const override;
	virtual bool GenerateStreaming(const FGenerateStreamingParams& InParams, FGenerateStreamingResult& OutResult) override;
	virtual FArchive& AppendCellGuid(FArchive& InAr) override;
	//~ End URuntimePartition interface
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = RuntimeSettings)
	uint32 CellSize;

	UPROPERTY(EditAnywhere, Category = RuntimeSettings, Transient, SkipSerialization)
	bool bShowGridPreview = false;
#endif

#if WITH_EDITOR
	TUniquePtr<FWorldGridPreviewer> WorldGridPreviewer;
#endif
};
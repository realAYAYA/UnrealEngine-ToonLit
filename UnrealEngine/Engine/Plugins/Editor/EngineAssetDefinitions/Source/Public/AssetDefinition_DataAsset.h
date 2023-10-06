// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Engine/DataAsset.h"

#include "AssetDefinition_DataAsset.generated.h"

UCLASS()
class UAssetDefinition_DataAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Implementation
	UAssetDefinition_DataAsset()
	{
		IncludeClassInFilter = EIncludeClassInFilter::Always;
	}
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetDefinition", "DataAsset", "Data Asset"); }
	virtual FText GetAssetDisplayName(const FAssetData& AssetData) const override;
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(201, 29, 85)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UDataAsset::StaticClass(); }
	
	virtual EAssetCommandResult PerformAssetDiff(const FAssetDiffArgs& DiffArgs) const override;

	virtual bool CanMerge() const override;
	virtual EAssetCommandResult Merge(const FAssetAutomaticMergeArgs& MergeArgs) const override;
	virtual EAssetCommandResult Merge(const FAssetManualMergeArgs& MergeArgs) const override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif


UCLASS()
class UUndoableResolveHandler : public UObject
{
public:
	GENERATED_BODY()
	void SetManagedObject(UObject* Object);
	void MarkResolved();

	virtual void PostEditUndo() override;

private:
	FString BaseRevisionNumber;
	FString CurrentRevisionNumber;
	FString BackupFilepath;
	UObject* ManagedObject;
	TSharedPtr<class ISourceControlChangelist> CheckinIdentifier;
	
	UPROPERTY()
	bool bShouldBeResolved = false;
};
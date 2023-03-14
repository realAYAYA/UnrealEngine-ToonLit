// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataRegistrySource.h"
#include "Engine/DataTable.h"
#include "DataRegistrySource_DataTable.generated.h"

/** Rules struct for data table access */
USTRUCT()
struct DATAREGISTRY_API FDataRegistrySource_DataTableRules
{
	GENERATED_BODY()

	/** True if the entire table should be loaded into memory when the source is loaded, false if the table is loaded on demand */
	UPROPERTY(EditAnywhere, Category = DataRegistry)
	bool bPrecacheTable = true;

	/** Time in seconds to keep cached table alive if hard reference is off. 0 will release immediately, -1 will never release */
	UPROPERTY(EditAnywhere, Category = DataRegistry)
	float CachedTableKeepSeconds = -1.0;
};


/** Data source that loads from a specific data table containing the same type of structs as the registry */
UCLASS(Meta = (DisplayName = "DataTable Source"))
class DATAREGISTRY_API UDataRegistrySource_DataTable : public UDataRegistrySource
{
	GENERATED_BODY()
public:

	/** What table to load from */
	UPROPERTY(EditAnywhere, Category = DataRegistry)
	TSoftObjectPtr<UDataTable> SourceTable;

	/** Access rules */
	UPROPERTY(EditAnywhere, Category = DataRegistry)
	FDataRegistrySource_DataTableRules TableRules;

	/** Update source table and rules, meant to be called from a meta source */
	void SetSourceTable(const TSoftObjectPtr<UDataTable>& InSourceTable, const FDataRegistrySource_DataTableRules& InTableRules);

protected:
	/** Hard ref to loaded table */
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> CachedTable;

	/** Preload table ref, will be set if this is a hard source */
	UPROPERTY()
	TObjectPtr<UDataTable> PreloadTable;

	/** Last time this was accessed */
	mutable float LastAccessTime;

	/** Handle for in progress load */
	TSharedPtr<FStreamableHandle> LoadingTableHandle;

	/** List of requests to resolve when table is loaded */
	TArray<FDataRegistrySourceAcquireRequest> PendingAcquires;

	/** Tells it to set CachedTable if possible */
	virtual void SetCachedTable(bool bForceLoad = false);

	/** Clears cached table pointer so it can be GCd */
	virtual void ClearCachedTable();

	/** Tells it to go through each pending acquire */
	virtual void HandlePendingAcquires();

	/** Callback after table loads */
	virtual void OnTableLoaded();


	// Source interface
	virtual EDataRegistryAvailability GetSourceAvailability() const override;
	virtual EDataRegistryAvailability GetItemAvailability(const FName& ResolvedName, const uint8** PrecachedDataPtr) const override;
	virtual void GetResolvedNames(TArray<FName>& Names) const override;
	virtual void ResetRuntimeState() override;
	virtual bool AcquireItem(FDataRegistrySourceAcquireRequest&& Request) override;
	virtual void TimerUpdate(float CurrentTime, float TimerUpdateFrequency) override;
	virtual FString GetDebugString() const override;
	virtual bool Initialize() override;

	// Object interface
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void EditorRefreshSource();
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

#endif
};


/** Meta source that will generate DataTable sources at runtime based on a directory scan or asset registration */
UCLASS(Meta = (DisplayName = "DataTable Meta Source"))
class DATAREGISTRY_API UMetaDataRegistrySource_DataTable : public UMetaDataRegistrySource
{
	GENERATED_BODY()
public:
	/** Constructor */
	UMetaDataRegistrySource_DataTable();

	/** What specific source class to spawn */
	UPROPERTY(EditAnywhere, Category = DataRegistry)
	TSubclassOf<UDataRegistrySource_DataTable> CreatedSource;

	/** Access rules */
	UPROPERTY(EditAnywhere, Category = DataRegistry)
	FDataRegistrySource_DataTableRules TableRules;

protected:
	
	// Source interface
	virtual TSubclassOf<UDataRegistrySource> GetChildSourceClass() const override;
	virtual bool SetDataForChild(FName SourceId, UDataRegistrySource* ChildSource) override;
	virtual bool DoesAssetPassFilter(const FAssetData& AssetData, bool bNewRegisteredAsset) override;

};

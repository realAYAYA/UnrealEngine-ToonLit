// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataRegistrySource_DataTable.h"
#include "DataRegistrySource_CurveTable.generated.h"

class UCurveTable;


/** Data source that loads from a specific curve table asset */
UCLASS(Meta = (DisplayName = "CurveTable Source"))
class DATAREGISTRY_API UDataRegistrySource_CurveTable : public UDataRegistrySource
{
	GENERATED_BODY()
public:

	/** What table to load from */
	UPROPERTY(EditAnywhere, Category = DataRegistry)
	TSoftObjectPtr<UCurveTable> SourceTable;

	/** Access rules */
	UPROPERTY(EditAnywhere, Category = DataRegistry)
	FDataRegistrySource_DataTableRules TableRules;

	/** Update source table and rules, meant to be called from a meta source */
	void SetSourceTable(const TSoftObjectPtr<UCurveTable>& InSourceTable, const FDataRegistrySource_DataTableRules& InTableRules);

protected:
	/** Hard ref to loaded table */
	UPROPERTY(Transient)
	TObjectPtr<UCurveTable> CachedTable;

	/** Preload table ref, will be set if this is a hard source */
	UPROPERTY()
	TObjectPtr<UCurveTable> PreloadTable;

	/** Last time this was accessed */
	mutable float LastAccessTime;

	/** This is set if the table fails to load or could never load */
	bool bInvalidSourceTable = false;

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

	virtual void OnDataTableChanged();

#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#endif
};


/** Meta source that will generate CurveTable sources at runtime based on a directory scan or asset registration */
UCLASS(Meta = (DisplayName = "CurveTable Meta Source"))
class DATAREGISTRY_API UMetaDataRegistrySource_CurveTable : public UMetaDataRegistrySource
{
	GENERATED_BODY()
public:
	/** Constructor */
	UMetaDataRegistrySource_CurveTable();

	/** What specific source class to spawn */
	UPROPERTY(EditAnywhere, Category = DataRegistry)
	TSubclassOf<UDataRegistrySource_CurveTable> CreatedSource;

	/** Access rules */
	UPROPERTY(EditAnywhere, Category = DataRegistry)
	FDataRegistrySource_DataTableRules TableRules;

protected:
	
	// Source interface
	virtual TSubclassOf<UDataRegistrySource> GetChildSourceClass() const override;
	virtual bool SetDataForChild(FName SourceId, UDataRegistrySource* ChildSource) override;
	virtual bool DoesAssetPassFilter(const FAssetData& AssetData, bool bNewRegisteredAsset) override;

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/CurveTable.h"
#endif

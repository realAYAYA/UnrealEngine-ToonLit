// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGSettings.h"
#include "Async/PCGAsyncLoadingContext.h"

#include "PCGGetPropertyFromObjectPath.generated.h"

/**
* Extract property from a list of soft object paths.
*/
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGGetPropertyFromObjectPathSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	virtual void GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
	virtual bool CanDynamicallyTrackKeys() const override { return true; }
#endif
	virtual bool CanCullTaskIfUnwired() const override { return false; }
	virtual FString GetAdditionalTitleInformation() const override;

protected:
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override { return Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** If nothing is connected in the In pin, will use those static paths to load.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TArray<FSoftObjectPath> ObjectPathsToExtract;

	/** If something is connected in the In pin, will look for this attribute values to load.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable, PCG_DiscardPropertySelection, PCG_DiscardExtraSelection))
	FPCGAttributePropertyInputSelector InputSource;

	/** Property name to extract. Can only extract properties that are compatible with metadata types. If None, extract the object. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName PropertyName = NAME_None;

	/** If the property is a struct/object supported by metadata, this option can be toggled to force extracting all (compatible) properties contained in this property. Automatically true if unsupported by metadata. For now, only supports direct child properties (and not deeper). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bForceObjectAndStructExtraction = false;

	/** By default, attribute name will be None, but it can be overridden by this name. Use @SourceName to use the property name (only works when not extracting). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "!bForceObjectAndStructExtraction", EditConditionHides))
	FName OutputAttributeName = NAME_None;

	/** By default, object loading is asynchronous, can force it synchronous if needed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug")
	bool bSynchronousLoad = false;

	/** Opt-in option to create empty data when there is nothing to extract or property is not found, to have the same number of inputs than outputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced")
	bool bPersistAllData = false;

	/** Opt-in option to silence errors when the path is Empty or nothing to extract. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced")
	bool bSilenceErrorOnEmptyObjectPath = false;
};

struct FPCGGetPropertyFromObjectPathContext : public FPCGContext, public IPCGAsyncLoadingContext
{
	TArray<TTuple<FSoftObjectPath, int32>> PathsToObjectsToExtractAndIncomingDataIndex;
};

class FPCGGetPropertyFromObjectPathElement : public IPCGElement
{
public:
	// Loading needs to be done on the main thread and accessing objects outside of PCG might not be thread safe, so taking the safe approach
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	// FIXME: Disable the caching to be sure that we have the most up-to-date data from external objects
	// To be re-enabled when we have more control on the cache, to be able to remove entries that were generated
	// but no longer tracked, because if this object changes but it's not tracked anymore, then it will not properly remove itself from the 
	// cache and not be caught up in future executions
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual FPCGContext* CreateContext() override;
	virtual bool PrepareDataInternal(FPCGContext* Context) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};


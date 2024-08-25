// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGSettings.h"
#include "Elements/PCGActorSelector.h"

#include "UObject/ObjectKey.h"

#include "PCGDataFromActor.generated.h"

UENUM()
enum class EPCGGetDataFromActorMode : uint8
{
	ParseActorComponents UMETA(Tooltip = "Parse the found actor(s) for relevant components such as Primitives, Splines, and Volumes."),
	GetSinglePoint UMETA(Tooltip = "Produces a single point per actor with the actor transform and bounds."),
	GetDataFromProperty UMETA(Tooltip = "Gets a data collection from an actor property."),
	GetDataFromPCGComponent UMETA(Tooltip = "Copy generated output from other PCG components on the found actor(s)."),
	GetDataFromPCGComponentOrParseComponents UMETA(Tooltip = "Attempts to copy generated output from other PCG components on the found actor(s), otherwise, falls back to parsing actor components.")
};

/** Builds a collection of PCG-compatible data from the selected actors. */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGDataFromActorSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetActorData")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGDataFromActorSettings", "NodeTitle", "Get Actor Data"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual void GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
	virtual bool HasDynamicPins() const override { return true; }
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;
#endif
	virtual EPCGDataType GetCurrentPinTypes(const UPCGPin* InPin) const override;

	virtual FString GetAdditionalTitleInformation() const override;

protected:
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override { return Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	//~Begin UObject interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

public:
	/** Override this to filter what kinds of data should be retrieved from the actor(s). */
	virtual EPCGDataType GetDataFilter() const { return EPCGDataType::Any; }

	/** Override this to change the default value the selector will revert to when changing the actor selection type */
	virtual TSubclassOf<AActor> GetDefaultActorSelectorClass() const;

	/** Describes which actors to select for data collection. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties))
	FPCGActorSelectorSettings ActorSelector;

	/** Describes what kind of data we will collect from the found actor(s). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data Retrieval Settings", meta = (EditCondition = bDisplayModeSettings, EditConditionHides, HideEditConditionToggle))
	EPCGGetDataFromActorMode Mode = EPCGGetDataFromActorMode::ParseActorComponents;

	/** Also produces a single point data at the actor location. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data Retrieval Settings", meta = (EditCondition = "Mode == EPCGGetDataFromActorMode::GetDataFromPCGComponent || Mode == EPCGGetDataFromActorMode::GetDataFromPCGComponentOrParseComponents", EditConditionHides))
	bool bAlsoOutputSinglePointData = false;

	/** Only get data from components which overlap with the bounds of your source component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data Retrieval Settings", meta = (EditCondition = "Mode == EPCGGetDataFromActorMode::GetDataFromPCGComponent || Mode == EPCGGetDataFromActorMode::GetDataFromPCGComponentOrParseComponents", EditConditionHides))
	bool bComponentsMustOverlapSelf = true;

	/** Get data from all grid sizes if there is a partitioned PCG component on the actor, instead of a specific set of grid sizes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data Retrieval Settings", meta = (EditCondition = "Mode == EPCGGetDataFromActorMode::GetDataFromPCGComponent || Mode == EPCGGetDataFromActorMode::GetDataFromPCGComponentOrParseComponents", EditConditionHides))
	bool bGetDataOnAllGrids = true;

	/** Select which grid sizes to consider when collecting data from partitioned PCG components. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data Retrieval Settings", meta = (Bitmask, BitmaskEnum = "/Script/PCG.EPCGHiGenGrid", EditCondition = "!bGetDataOnAllGrids && Mode == EPCGGetDataFromActorMode::GetDataFromPCGComponent || Mode == EPCGGetDataFromActorMode::GetDataFromPCGComponentOrParseComponents", EditConditionHides))
	int32 AllowedGrids = int32(EPCGHiGenGrid::Uninitialized);

	/** Merges all the single point data outputs into a single point data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data Retrieval Settings", meta = (EditCondition = "Mode == EPCGGetDataFromActorMode::GetSinglePoint", EditConditionHides))
	bool bMergeSinglePointData = false;

	// This can be set false by inheriting nodes to hide the 'Mode' property.
	UPROPERTY(Transient, meta = (EditCondition = false, EditConditionHides))
	bool bDisplayModeSettings = true;

	/** Provide pin names to match against the found component output pins. Data will automatically be wired to the expected pin if the name comparison succeeds. All unmatched pins will go into the standard out pin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data Retrieval Settings", meta = (EditCondition = "Mode == EPCGGetDataFromActorMode::GetDataFromPCGComponent || Mode == EPCGGetDataFromActorMode::GetDataFromPCGComponentOrParseComponents", EditConditionHides))
	TArray<FName> ExpectedPins;

	/** The property name on the found actor to create a data collection from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data Retrieval Settings", meta = (EditCondition = "Mode == EPCGGetDataFromActorMode::GetDataFromProperty", EditConditionHides))
	FName PropertyName = NAME_None;

	/** Silence warnings that attribute names were sanitized to replace invalid characters. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data Retrieval Settings|Advanced")
	bool bSilenceSanitizedAttributeNameWarnings = false;

#if WITH_EDITORONLY_DATA
	/** If this is checked, found actors that are outside component bounds will not trigger a refresh. Only works for tags for now in editor. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Data Retrieval Settings")
	bool bTrackActorsOnlyWithinBounds = true;
#endif // WITH_EDITORONLY_DATA
};

struct FPCGDataFromActorContext : public FPCGContext
{
	TArray<AActor*> FoundActors;
	bool bPerformedQuery = false;

#if WITH_EDITOR
	/** Any change origin ignores we added, to solve dependency issues (like upstream execution cancelling downstream graph). */
	TArray<TObjectKey<UObject>> IgnoredChangeOrigins;
#endif
};

class PCG_API FPCGDataFromActorElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual void GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const override;
	virtual bool ShouldComputeFullOutputDataCrc(FPCGContext* Context) const override { return true; }

protected:
	virtual FPCGContext* CreateContext() override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	void GatherWaitTasks(AActor* FoundActor, FPCGContext* InContext, TArray<FPCGTaskId>& OutWaitTasks) const;
	virtual void ProcessActors(FPCGContext* Context, const UPCGDataFromActorSettings* Settings, const TArray<AActor*>& FoundActors) const;
	virtual void ProcessActor(FPCGContext* Context, const UPCGDataFromActorSettings* Settings, AActor* FoundActor) const;

	void MergeActorsIntoPointData(FPCGContext* Context, const UPCGDataFromActorSettings* Settings, const TArray<AActor*>& FoundActors) const;
};

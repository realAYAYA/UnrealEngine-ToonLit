// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGGraph.h"
#include "PCGSettings.h"

#include "UObject/ObjectPtr.h"

#include "PCGSubgraph.generated.h"

namespace PCGBaseSubgraphConstants
{
	static const FString UserParameterTagData = TEXT("PCGUserParametersTagData");
}

UCLASS(Abstract)
class PCG_API UPCGBaseSubgraphSettings : public UPCGSettings
{
	GENERATED_BODY()
public:
	UPCGGraph* GetSubgraph() const;
	virtual UPCGGraphInterface* GetSubgraphInterface() const { return nullptr; }

	/** Returns true if the subgraphs nodes were not inlined into the parent graphs tasks during compilation. */
	virtual bool IsDynamicGraph() const { return false; }

	// Use this method from the outside to set the subgraph, as it will connect editor callbacks
	virtual void SetSubgraph(UPCGGraphInterface* InGraph);

protected:
	//~Begin UObject interface implementation
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;
	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface implementation

public:
	//~Begin UPCGSettings interface
	virtual bool HasFlippedTitleLines() const override { return true; }
	// The graph may contain nodes that have side effects, don't assume we can cull even when unwired.
	// TODO: For static SGs we could probably compute this value based on the subgraph nodes.
	virtual bool CanCullTaskIfUnwired() const { return false; }

protected:
#if WITH_EDITOR
	virtual void GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
	virtual bool CanDynamicallyTrackKeys() const override { return true; }
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override;
#endif

	TArray<FPCGPinProperties> InputPinProperties() const override;
	TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	virtual void SetSubgraphInternal(UPCGGraphInterface* InGraph) {}

#if WITH_EDITOR
	void SetupCallbacks();
	void TeardownCallbacks();

	void OnSubgraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType);
#endif

protected:
	// Overrides
	virtual void FixingOverridableParamPropertyClass(FPCGSettingsOverridableParam& Param) const;
#if WITH_EDITOR
	virtual TArray<FPCGSettingsOverridableParam> GatherOverridableParams() const override;
#endif // WITH_EDITOR
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class PCG_API UPCGSubgraphSettings : public UPCGBaseSubgraphSettings
{
	GENERATED_BODY()

public:
	UPCGSubgraphSettings(const FObjectInitializer& InObjectInitializer);

protected:
	//~Begin UObject interface implementation
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface implementation

public:
	//~UPCGSettings interface implementation
	virtual UPCGNode* CreateNode() const override;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("Subgraph")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSubgraphSettings", "NodeTitle", "Subgraph"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Subgraph; }
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual bool GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const override { return false; }
#endif

	virtual FString GetAdditionalTitleInformation() const override;

protected:
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override;
#endif
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface implementation

	//~Begin UPCGBaseSubgraphSettings interface
public:
	virtual UPCGGraphInterface* GetSubgraphInterface() const override;
	virtual bool IsDynamicGraph() const override;
protected:
	virtual void SetSubgraphInternal(UPCGGraphInterface* InGraph) override;
	//~End UPCGBaseSubgraphSettings interface

public:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Properties, Instanced, meta = (NoResetToDefault))
	TObjectPtr<UPCGGraphInstance> SubgraphInstance;

	UPROPERTY(BlueprintReadOnly, Category = Properties, meta = (PCG_Overridable))
	TObjectPtr<UPCGGraphInterface> SubgraphOverride;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UPCGGraph> Subgraph_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
};

UCLASS(Abstract)
class PCG_API UPCGBaseSubgraphNode : public UPCGNode
{
	GENERATED_BODY()

public:
	TObjectPtr<UPCGGraph> GetSubgraph() const;
	virtual TObjectPtr<UPCGGraphInterface> GetSubgraphInterface() const { return nullptr; }
};

UCLASS(MinimalAPI, ClassGroup = (Procedural))
class UPCGSubgraphNode : public UPCGBaseSubgraphNode
{
	GENERATED_BODY()

public:
	/** ~Begin UPCGBaseSubgraphNode interface */
	virtual TObjectPtr<UPCGGraphInterface> GetSubgraphInterface() const override;
	/** ~End UPCGBaseSubgraphNode interface */
};

struct FPCGSubgraphContext : public FPCGContext
{
	TArray<FPCGTaskId> SubgraphTaskIds;
	bool bScheduledSubgraph = false;
	FInstancedStruct GraphInstanceParametersOverride;

	// Analyze input data to detect if there is any override for the user parameters. If so will duplicate it to gather overrides.
	void InitializeUserParametersStruct();

	// If we have a subgraph override, update the underlying duplicated parameters with the overrides from the subgraph.
	void UpdateOverridesWithOverriddenGraph();

protected:
	virtual void* GetUnsafeExternalContainerForOverridableParam(const FPCGSettingsOverridableParam& InParam) override;
};

class FPCGSubgraphElement : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsPassthrough(const UPCGSettings* InSettings) const override;
	void PrepareSubgraphData(const UPCGSubgraphSettings* Settings, FPCGSubgraphContext* Context, const FPCGDataCollection& InputData, FPCGDataCollection& OutputData) const;
	void PrepareSubgraphUserParameters(const UPCGSubgraphSettings* Settings, FPCGSubgraphContext* Context, FPCGDataCollection& OutputData) const;
};

class FPCGInputForwardingElement : public IPCGElement
{
public:
	FPCGInputForwardingElement(const FPCGDataCollection& InputToForward);

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsPassthrough(const UPCGSettings* InSettings) const override { return true; }
	FPCGDataCollection Input;

	TArray<UPCGData*> RootedData;
};

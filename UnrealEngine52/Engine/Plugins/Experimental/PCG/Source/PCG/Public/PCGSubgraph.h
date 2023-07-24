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

	// Use this method from the outside to set the subgraph, as it will connect editor callbacks
	virtual void SetSubgraph(UPCGGraphInterface* InGraph);

protected:
	//~Begin UObject interface implementation
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End UObject interface implementation

	//~Begin UPCGSettings interface
	virtual void GetTrackedActorTags(FPCGTagToSettingsMap& OutTagToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
	virtual bool IsStructuralProperty(const FName& InPropertyName) const override;
#endif

	TArray<FPCGPinProperties> InputPinProperties() const override;
	TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	virtual void SetSubgraphInternal(UPCGGraphInterface* InGraph) {}

#if WITH_EDITOR
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
	//~End UObject interface implementation

public:
	//~UPCGSettings interface implementation
	virtual UPCGNode* CreateNode() const override;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("Subgraph")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSubgraphSettings", "NodeTitle", "Subgraph"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Subgraph; }
	virtual UObject* GetJumpTargetForDoubleClick() const override;
#endif

	virtual FName AdditionalTaskName() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface implementation

	//~Begin UPCGBaseSubgraphSettings interface
public:
	virtual UPCGGraphInterface* GetSubgraphInterface() const override { return SubgraphInstance.Get(); }
protected:
	virtual void SetSubgraphInternal(UPCGGraphInterface* InGraph) override;
#if WITH_EDITOR
	virtual bool IsStructuralProperty(const FName& InPropertyName) const override;
#endif
	//~End UPCGBaseSubgraphSettings interface

public:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Properties, Instanced, meta = (NoResetToDefault))
	TObjectPtr<UPCGGraphInstance> SubgraphInstance;

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDynamicGraph = false;

	TObjectPtr<UPCGGraph> GetSubgraph() const;
	virtual TObjectPtr<UPCGGraphInterface> GetSubgraphInterface() const { return nullptr; }
};

UCLASS(ClassGroup = (Procedural))
class PCG_API UPCGSubgraphNode : public UPCGBaseSubgraphNode
{
	GENERATED_BODY()

public:
	/** ~Begin UPCGBaseSubgraphNode interface */
	virtual TObjectPtr<UPCGGraphInterface> GetSubgraphInterface() const override;
	/** ~End UPCGBaseSubgraphNode interface */
};

struct PCG_API FPCGSubgraphContext : public FPCGContext
{
	FPCGTaskId SubgraphTaskId = InvalidPCGTaskId;
	bool bScheduledSubgraph = false;
	FInstancedStruct GraphInstanceParametersOverride;

protected:
	virtual void* GetUnsafeExternalContainerForOverridableParam(const FPCGSettingsOverridableParam& InParam) override;
};

class PCG_API FPCGSubgraphElement : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsPassthrough(const UPCGSettings* InSettings) const override { return !InSettings || InSettings->bEnabled; }
};

class PCG_API FPCGInputForwardingElement : public FSimplePCGElement
{
public:
	FPCGInputForwardingElement(const FPCGDataCollection& InputToForward);

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsPassthrough(const UPCGSettings* InSettings) const override { return true; }
	FPCGDataCollection Input;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

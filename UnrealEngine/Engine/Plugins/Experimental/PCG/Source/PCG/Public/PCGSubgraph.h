// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"

class UPCGGraph;

#include "PCGSubgraph.generated.h"

UCLASS(Abstract)
class PCG_API UPCGBaseSubgraphSettings : public UPCGSettings
{
	GENERATED_BODY()
public:
	virtual UPCGGraph* GetSubgraph() const { return nullptr; }

protected:
	//~Begin UObject interface implementation
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End UObject interface implementation

	//~Begin UPCGSettings interface
	virtual void GetTrackedActorTags(FPCGTagToSettingsMap& OutTagToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
#endif

	TArray<FPCGPinProperties> InputPinProperties() const override;
	TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

#if WITH_EDITOR
	void OnSubgraphChanged(UPCGGraph* InGraph, EPCGChangeType ChangeType);
#endif
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class PCG_API UPCGSubgraphSettings : public UPCGBaseSubgraphSettings
{
	GENERATED_BODY()

public:
	//~UPCGSettings interface implementation
	virtual UPCGNode* CreateNode() const override;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SubgraphNode")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Subgraph; }
	virtual UObject* GetJumpTargetForDoubleClick() const override;
#endif

	virtual FName AdditionalTaskName() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface implementation

	//~Begin UPCGBaseSubgraphSettings interface
public:
	virtual UPCGGraph* GetSubgraph() const override { return Subgraph; }
protected:
#if WITH_EDITOR
	virtual bool IsStructuralProperty(const FName& InPropertyName) const override;
#endif
	//~End UPCGBaseSubgraphSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TObjectPtr<UPCGGraph> Subgraph;
};

UCLASS(Abstract)
class PCG_API UPCGBaseSubgraphNode : public UPCGNode
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDynamicGraph = false;

	virtual TObjectPtr<UPCGGraph> GetSubgraph() const { return nullptr; }
};

UCLASS(ClassGroup = (Procedural))
class PCG_API UPCGSubgraphNode : public UPCGBaseSubgraphNode
{
	GENERATED_BODY()

public:
	/** ~Begin UPCGBaseSubgraphNode interface */
	virtual TObjectPtr<UPCGGraph> GetSubgraph() const override;
	/** ~End UPCGBaseSubgraphNode interface */
};

struct PCG_API FPCGSubgraphContext : public FPCGContext
{
	FPCGTaskId SubgraphTaskId = InvalidPCGTaskId;
	bool bScheduledSubgraph = false;
};

class PCG_API FPCGSubgraphElement : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsPassthrough() const override { return true; }
};

class PCG_API FPCGInputForwardingElement : public FSimplePCGElement
{
public:
	FPCGInputForwardingElement(const FPCGDataCollection& InputToForward);

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsPassthrough() const override { return true; }
	FPCGDataCollection Input;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSubgraph.h"

#include "PCGLoopElement.generated.h"

UCLASS(BlueprintType, ClassGroup=(Procedural))
class PCG_API UPCGLoopSettings : public UPCGSubgraphSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface implementation
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("Loop")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
#endif

	virtual FName AdditionalTaskName() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface implementation

public:
	//~Begin UPCGBaseSubgraphSettings interface
	virtual bool IsDynamicGraph() const override { return true; }
	//~End UPCGBaseSubgraphSettings interface

	/** 
	* Comma-separated list of pin names on which we will loop by-element in a step-wise fashion; if more than one is provided, it is expected that they all have the same number of data. 
	* If none are provided, the first connected pin will taken as the pin to loop on. 
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data)
	FString LoopPins;
};

class PCG_API FPCGLoopElement : public FPCGSubgraphElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	void PrepareLoopDataCollections(FPCGContext* Context, const UPCGLoopSettings* Settings, TArray<FPCGDataCollection>& LoopDataCollection, FPCGDataCollection& FixedInputDataCollection) const;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGExternalData.generated.h"

struct FPCGExternalDataContext;

/** Base class for external data input settings */
UCLASS(Abstract, ClassGroup = (Procedural))
class PCG_API UPCGExternalDataSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::InputOutput; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return DefaultPointOutputPinProperties(); }
	// ~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TMap<FString, FPCGAttributePropertyInputSelector> AttributeMapping;
};

class PCG_API FPCGExternalDataElement : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
protected:
	virtual bool PrepareDataInternal(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;

	virtual bool PrepareLoad(FPCGExternalDataContext* Context) const = 0;
	virtual bool ExecuteLoad(FPCGExternalDataContext* Context) const;
	virtual bool PostExecuteLoad(FPCGExternalDataContext* Context) const { return true; }
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"

#include "PCGDebugElement.generated.h"

namespace PCGDebugElement
{
	void ExecuteDebugDisplay(FPCGContext* Context);
}

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGDebugSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	// ~Begin UPCGSettings interface
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("DebugNode")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return TArray<FPCGPinProperties>(); }

protected:
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface
};

class FPCGDebugElement : public FSimplePCGElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;	
};

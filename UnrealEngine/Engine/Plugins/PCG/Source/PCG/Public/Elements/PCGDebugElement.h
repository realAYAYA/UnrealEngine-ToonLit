// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "GameFramework/Actor.h"

#include "PCGDebugElement.generated.h"

namespace PCGDebugElement
{
	void ExecuteDebugDisplay(FPCGContext* Context, AActor* TargetActor = nullptr);
}

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGDebugSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	// ~Begin UPCGSettings interface
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("Debug")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGDebugSettings", "NodeTitle", "Debug"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
#endif
	

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;
};

class FPCGDebugElement : public IPCGElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

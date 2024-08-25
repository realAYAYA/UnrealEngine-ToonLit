// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Metadata/PCGObjectPropertyOverride.h"

#include "PCGApplyOnActor.generated.h"

class AActor;

/**
* Apply property overrides and executes functions on a target actor.
*/
UCLASS(BlueprintType)
class UPCGApplyOnActorSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("ApplyOnActor")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGApplyOnActorElement", "NodeTitle", "Apply On Actor"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGApplyOnActorElement", "NodeTooltip", "Applies property overrides and executes functions on a target actor."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;

	/** Override the default property values on the target actor. Applied before post-process functions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<FPCGObjectPropertyOverrideDescription> PropertyOverrideDescriptions;

	/** Specify a list of functions to be called on the target actor. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> PostProcessFunctionNames;
};

class FPCGApplyOnActorElement : public IPCGElement
{
public:
	// Calling a function on an actor might not be threadsafe, so taking the safe approach.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGAddTag.generated.h"

/**
 * Applies the specified tags on the output data.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGAddTagSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("AddTags")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGAddTagElement", "NodeTitle", "Add Tags"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGAddTagElement", "NodeTooltip", "Applies the specified tags on the output data."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
	virtual bool HasDynamicPins() const override { return true; }
#endif
	

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Comma-separated list of tags to apply to the node */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	FString TagsToAdd;
};

class FPCGAddTagElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"
#include "Elements/PCGCreateAttributeBase.h"

#include "PCGCreateAttribute.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGCreateAttributeSettings : public UPCGAttributeCreationBaseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bKeepExistingAttributes = false;

protected:
	virtual FPCGElementPtr CreateElement() const override;
};


class FPCGCreateAttributeElement : public FSimplePCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
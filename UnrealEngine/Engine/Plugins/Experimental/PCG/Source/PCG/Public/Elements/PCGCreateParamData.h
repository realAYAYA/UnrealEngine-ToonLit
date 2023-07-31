// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"
#include "Elements/PCGCreateAttributeBase.h"

#include "PCGCreateParamData.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), meta = (Deprecated = "5.2", DeprecationMessage = "Use CreateAttribute instead."))
class PCG_API UPCGCreateParamDataSettings : public UPCGAttributeCreationBaseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

protected:
	virtual FPCGElementPtr CreateElement() const override;
};


class FPCGCreateParamDataElement : public FSimplePCGElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
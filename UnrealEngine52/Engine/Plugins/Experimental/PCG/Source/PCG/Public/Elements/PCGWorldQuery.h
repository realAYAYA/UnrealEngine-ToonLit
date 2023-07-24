// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGPin.h"
#include "PCGSettings.h"
#include "Data/PCGWorldData.h"

#include "PCGWorldQuery.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGWorldQuerySettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("WorldVolumetricQuery")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGWorldQuerySettings", "NodeTitle", "World Volumetric Query"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (ShowOnlyInnerProperties))
	FPCGWorldVolumetricQueryParams QueryParams;
};

class FPCGWorldVolumetricQueryElement : public FSimplePCGElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const { return false; }
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const;
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGWorldRayHitSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("WorldRayHitQuery")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGWorldRayHitSettings", "NodeTitle", "World Ray Hit Query"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (ShowOnlyInnerProperties))
	FPCGWorldRayHitQueryParams QueryParams;
};

class FPCGWorldRayHitQueryElement : public FSimplePCGElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const { return false; }
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const;
};

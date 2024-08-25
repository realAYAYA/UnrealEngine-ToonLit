// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGNumberOfElements.generated.h"

class UPCGParamData;
class UPCGPointData;

/**
* Elements for getting the number of elements in a point data or a param data. Since the whole logic is identical
* except for getting the number of elements, it is factorized in a base class.
*/

// Base class for common elements
UCLASS(Abstract)
class UPCGNumberOfElementsBaseSettings : public UPCGSettings
{
	GENERATED_BODY()

	//~Begin UPCGSettings interface
	

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName OutputAttributeName = TEXT("NumEntries");
};

/**
* Return the number of points in the input point data.
*/

UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGNumberOfPointsSettings : public UPCGNumberOfElementsBaseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetPointsCount")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGNumberOfElementsSettings", "NodeTitlePoint", "Get Points Count"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGNumberOfElementsSettings", "NodeTooltipPoint", "Return the number of points in the input point data."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};

/**
* Return the number of entries in the input attribute sets.
*/

UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGNumberOfEntriesSettings : public UPCGNumberOfElementsBaseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetEntriesCount")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGNumberOfElementsSettings", "NodeTitleEntry", "Get Entries Count"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGNumberOfElementsSettings", "NodeTooltipEntry", "Return the number of entries in the input attribute sets."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};


template <typename DataType>
class FPCGNumberOfElementsBaseElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	virtual int32 GetNum(const DataType* InData) const = 0;
};

class FPCGNumberOfPointsElement : public FPCGNumberOfElementsBaseElement<UPCGPointData>
{
protected:
	virtual int32 GetNum(const UPCGPointData* InData) const override;
};

class FPCGNumberOfEntriesElement : public FPCGNumberOfElementsBaseElement<UPCGParamData>
{
protected:
	virtual int32 GetNum(const UPCGParamData* InData) const override;
};
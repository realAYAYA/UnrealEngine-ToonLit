// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/PCGDataFromActor.h"

#include "PCGTypedGetter.generated.h"

/** Builds a collection of landscape data from the selected actors. */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGGetLandscapeSettings : public UPCGDataFromActorSettings
{
	GENERATED_BODY()

public:
	UPCGGetLandscapeSettings();

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetLandscapeData")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGGetLandscapeSettings", "NodeTitle", "Get Landscape Data"); }
	virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings

public:
	//~Begin UPCGDataFromActorSettings interface
	virtual bool DataFilter(EPCGDataType InDataType) const override { return !!(InDataType & EPCGDataType::Landscape); }
	//~End UPCGDataFromActorSettings
};

/** Builds a collection of spline data from the selected actors. */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGGetSplineSettings : public UPCGDataFromActorSettings
{
	GENERATED_BODY()

public:
	UPCGGetSplineSettings();

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetSplineData")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGGetSplineSettings", "NodeTitle", "Get Spline Data"); }
	virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings

public:
	//~Begin UPCGDataFromActorSettings interface
	virtual bool DataFilter(EPCGDataType InDataType) const override { return !!(InDataType & EPCGDataType::PolyLine); }
	//~End UPCGDataFromActorSettings
};

/** Builds a collection of volume data from the selected actors. */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGGetVolumeSettings : public UPCGDataFromActorSettings
{
	GENERATED_BODY()

public:
	UPCGGetVolumeSettings();

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetVolumeData")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGGetVolumeSettings", "NodeTitle", "Get Volume Data"); }
	virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings

public:
	//~Begin UPCGDataFromActorSettings interface
	virtual bool DataFilter(EPCGDataType InDataType) const override { return !!(InDataType & EPCGDataType::Volume); }
	//~End UPCGDataFromActorSettings
};

/** Builds a collection of primitive data from primitive components on the selected actors. */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGGetPrimitiveSettings : public UPCGDataFromActorSettings
{
	GENERATED_BODY()

public:
	UPCGGetPrimitiveSettings();

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetPrimitiveData")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGGetPrimitiveSettings", "NodeTitle", "Get Primitive Data"); }
	virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings

public:
	//~Begin UPCGDataFromActorSettings interface
	virtual bool DataFilter(EPCGDataType InDataType) const override { return !!(InDataType & EPCGDataType::Primitive); }
	//~End UPCGDataFromActorSettings
};

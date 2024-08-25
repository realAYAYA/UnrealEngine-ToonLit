// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSpatialData.h"

#include "PCGSurfaceData.generated.h"

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSurfaceData : public UPCGSpatialDataWithPointCache
{
	GENERATED_BODY()

public:
	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Surface; }
	// ~End UPCGData interface

	//~Begin UPCGSpatialData interface
	virtual int GetDimension() const override { return 2; }
	virtual bool HasNonTrivialTransform() const override { return true; }
	//~End UPCGSpatialData interface

protected:
	void CopyBaseSurfaceData(UPCGSurfaceData* NewSurfaceData) const;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	FTransform Transform;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

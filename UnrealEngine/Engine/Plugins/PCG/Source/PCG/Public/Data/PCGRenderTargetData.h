// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/PCGTextureData.h"

#include "PCGRenderTargetData.generated.h"

class UTextureRenderTarget2D;

//TODO: It's possible that caching the result in this class is not as efficient as it could be
// if we expect to sample in different ways (e.g. channel) in the same render target
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGRenderTargetData : public UPCGBaseTextureData
{
	GENERATED_BODY()

public:
	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::RenderTarget; }
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// ~End UPCGData interface


	//~Begin UPCGSpatialData interface
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

public:
	UFUNCTION(BlueprintCallable, Category = RenderTarget)
	PCG_API void Initialize(UTextureRenderTarget2D* InRenderTarget, const FTransform& InTransform);

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Properties)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;
};

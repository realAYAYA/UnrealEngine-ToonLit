// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/PCGTextureData.h"

#include "PCGRenderTargetData.generated.h"

class UTextureRenderTarget2D;

//TODO: It's possible that caching the result in this class is not as efficient as it could be
// if we expect to sample in different ways (e.g. channel) in the same render target
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGRenderTargetData : public UPCGBaseTextureData
{
	GENERATED_BODY()

public:
	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::RenderTarget; }
	// ~End UPCGData interface


	//~Begin UPCGSpatialData interface
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

public:
	UFUNCTION(BlueprintCallable, Category = RenderTarget)
	void Initialize(UTextureRenderTarget2D* InRenderTarget, const FTransform& InTransform);

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Properties)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/TextureRenderTarget2D.h"
#endif

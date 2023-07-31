// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/PCGTextureData.h"
#include "Engine/TextureRenderTarget2D.h"

#include "PCGRenderTargetData.generated.h"

//TODO: It's possible that caching the result in this class is not as efficient as it could be
// if we expect to sample in different ways (e.g. channel) in the same render target
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGRenderTargetData : public UPCGBaseTextureData
{
	GENERATED_BODY()

public:
	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::RenderTarget | Super::GetDataType(); }
	// ~End UPCGData interface

	UFUNCTION(BlueprintCallable, Category = RenderTarget)
	void Initialize(UTextureRenderTarget2D* InRenderTarget, const FTransform& InTransform);

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Properties)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;
};

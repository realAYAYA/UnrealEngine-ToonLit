// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "RHI.h"
#include "RHICommandList.h"

class FPFMExporterMesh
{
	TArray<FVector> PFMPoints;
	int DimWidth;
	int DimHeight;
public:
	FPFMExporterMesh(int Width, int Height);
	~FPFMExporterMesh();

	bool BeginExport_RenderThread(FRHICommandListImmediate& RHICmdList);
	bool FinishExport_RenderThread(FRHICommandListImmediate& RHICmdList);
	bool SaveToFile(const FString& FileName);
	
	inline bool IsValid() const 
	{ 
		return PFMPoints.Num()>0; 
	}
	inline FIntRect GetSize() const
	{
		return FIntRect(FIntPoint(0, 0), FIntPoint(DimWidth, DimHeight));
	}

	inline FTexture2DRHIRef GetTargetableTexture() const
	{
		return RenderTargetTexture;
	}

private:
	FTexture2DRHIRef RenderTargetTexture;
	FTexture2DRHIRef ShaderResourceTexture;
};



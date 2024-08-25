// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "RenderMaterial_FX.h"
#include "Data/TiledBlob.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRenderMaterial_FX_MinMax, All, All);

class Tex;
typedef std::shared_ptr<Tex>		TexPtr;

class TEXTUREGRAPHENGINE_API RenderMaterial_FX_MinMax : public RenderMaterial_FX
{
private:
	
	FxMaterialPtr					SecondPassMaterial;
	BufferDescriptor				SourceDesc;
	TArray<TexPtr>					DownsampledResultTargets;		/// used for creating intermediate targets.
	
public:
	RenderMaterial_FX_MinMax(FString InName, FxMaterialPtr InMaterial, FxMaterialPtr InSecondPassMaterial);
	virtual ~RenderMaterial_FX_MinMax() override;

	virtual AsyncPrepareResult		PrepareResources(const TransformArgs& Args) override;
	virtual AsyncTransformResultPtr	Exec(const TransformArgs& Args) override;
	
	void							SetDescriptor(const BufferDescriptor& InDesc) { SourceDesc = InDesc;}
};

typedef std::shared_ptr<RenderMaterial_FX_MinMax> RenderMaterial_FX_MinMaxPtr;

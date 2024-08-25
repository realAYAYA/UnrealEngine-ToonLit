// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "RenderMaterial.h"
#include "UObject/StrongObjectPtr.h"
#include "RenderMaterial_BP.h"

// Subset of RenderMaterial BP, This class will always render a thumbnail from its material instance
class TEXTUREGRAPHENGINE_API RenderMaterial_Thumbnail : public RenderMaterial_BP
{
public:
	static constexpr uint32	GThumbWidth = 128;			/// Width of standard thumbnail image
	static constexpr uint32	GThumbHeight = 128;		/// Height of the standard thumbnail image

private:
	ERHIFeatureLevel::Type FeatureLevel;

public:
	RenderMaterial_Thumbnail(FString InName, UMaterial* InMaterial, UMaterialInstanceDynamic* instance = nullptr);

	virtual void BlitTo(FRHICommandListImmediate& RHI, UTextureRenderTarget2D* DstRT, const RenderMesh* MeshObj, int32 TargetId) const override;
	virtual bool CanHandleTiles() const override { return true; };
	virtual std::shared_ptr<BlobTransform> DuplicateInstance(FString InName) override;
	virtual AsyncPrepareResult PrepareResources(const TransformArgs& Args) override;
};

typedef std::shared_ptr<RenderMaterial_Thumbnail> RenderMaterial_ThumbPtr;
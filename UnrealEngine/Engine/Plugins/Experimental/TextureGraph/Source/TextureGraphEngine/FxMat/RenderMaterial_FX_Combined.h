// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "RenderMaterial_FX.h"
#include "Data/TiledBlob.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRenderMaterial_FX_NoTile, All, All);




class TEXTUREGRAPHENGINE_API RenderMaterial_FX_Combined : public RenderMaterial_FX
{
private:
	
	TArray<TiledBlobPtr>					ToBeCombined;		/// To save under process blobs. This array ensures that multiple SRVs can be passed to the shader

public:
	RenderMaterial_FX_Combined(FString InName, FxMaterialPtr InFXMaterial);
	virtual									~RenderMaterial_FX_Combined() override;

	void									AddTileArgs(TransformArgs& Args);
	virtual AsyncPrepareResult				PrepareResources(const TransformArgs& Args) override;
	virtual bool							CanHandleTiles() const override { return true; };
	virtual std::shared_ptr<BlobTransform>	DuplicateInstance(FString NewName) override;
	virtual AsyncTransformResultPtr			Exec(const TransformArgs& Args) override;
	TiledBlobPtr							AddBlobToCombine(TiledBlobPtr BlobToCombine);
	void									FreeCombinedBlob();

	static const FName						MemberName; /// Default struct parameter for tile
};

typedef std::shared_ptr<RenderMaterial_FX_Combined> RenderMaterial_FX_CombinedPtr;

// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "RenderMaterial_BP.h"

/**
 * 
 */
class TEXTUREGRAPHENGINE_API RenderMaterial_BP_NoTile : public RenderMaterial_BP
{

public:
	RenderMaterial_BP_NoTile(FString InName, UMaterial* InMaterial, UMaterialInstanceDynamic* InMaterialInstance = nullptr) : RenderMaterial_BP(InName, InMaterial, InMaterialInstance) {}
	virtual bool					CanHandleTiles() const override { return false; };
};

typedef std::shared_ptr<RenderMaterial_BP_NoTile> RenderMaterial_BP_NoTilePtr;

class TEXTUREGRAPHENGINE_API RenderMaterial_BP_TileArgs : public RenderMaterial_BP
{
private:
	FName							MemberName = "TileInfo";

public:
	RenderMaterial_BP_TileArgs(FString InName, UMaterial* InMaterial, UMaterialInstanceDynamic* InMaterialInstance = nullptr) : RenderMaterial_BP(InName, InMaterial, InMaterialInstance) {}
	void							AddTileArgs(TransformArgs& Args);
	virtual AsyncPrepareResult		PrepareResources(const TransformArgs& Args) override;
	virtual bool					CanHandleTiles() const override { return true; };
	virtual std::shared_ptr<BlobTransform> DuplicateInstance(FString NewName) override;
};

typedef std::shared_ptr<RenderMaterial_BP_TileArgs> RenderMaterial_BP_TileArgsPtr;
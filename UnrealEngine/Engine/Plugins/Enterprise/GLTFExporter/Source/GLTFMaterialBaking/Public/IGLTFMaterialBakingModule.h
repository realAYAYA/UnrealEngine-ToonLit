// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "GLTFMaterialPropertyEx.h"
#include "PixelFormat.h"
#include "SceneTypes.h"

class UTextureRenderTarget2D;
class UMaterialInterface;

class FGLTFExportMaterialProxy;
class FTextureRenderTargetResource;

struct FGLTFMaterialData; 
struct FGLTFMaterialDataEx;
struct FGLTFMeshRenderData;
struct FGLTFBakeOutput;
struct FGLTFBakeOutputEx;

class GLTFMATERIALBAKING_API IGLTFMaterialBakingModule : public IModuleInterface
{
public:
	/** Bakes out material properties according to MaterialSettings using MeshSettings and stores the output in Output */
	virtual void BakeMaterials(const TArray<FGLTFMaterialData*>& MaterialSettings, const TArray<FGLTFMeshRenderData*>& MeshSettings, TArray<FGLTFBakeOutput>& Output) = 0;

	/** Bakes out material properties according to extended MaterialSettings using MeshSettings and stores the output in Output */
	virtual void BakeMaterials(const TArray<FGLTFMaterialDataEx*>& MaterialSettings, const TArray<FGLTFMeshRenderData*>& MeshSettings, TArray<FGLTFBakeOutputEx>& Output) = 0;

	/** Outputs true HDR version of emissive color */
	virtual void SetEmissiveHDR(bool bHDR) = 0;

	/** Bakes all material properties to linear textures, except for colors */
	virtual void SetLinearBake(bool bCorrectLinear) = 0;

	/** Returns whether a specific material property is baked to a linear texture or not */
	virtual bool IsLinearBake(FGLTFMaterialPropertyEx Property) = 0;
};

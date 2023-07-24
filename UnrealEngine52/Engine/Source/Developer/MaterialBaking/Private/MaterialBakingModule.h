// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "PixelFormat.h"
#include "SceneTypes.h"
#include "IMaterialBakingModule.h"
#include "MaterialPropertyEx.h"

class UTextureRenderTarget2D;
class UMaterialOptions;
class UMaterialInterface;

class FExportMaterialProxy;
class FTextureRenderTargetResource;

struct FMaterialData; 
struct FMeshData;
struct FBakeOutput;

class MATERIALBAKING_API FMaterialBakingModule : public IMaterialBakingModule
{
public:
	/** IModuleInterface overrides begin */
	virtual void StartupModule();
	virtual void ShutdownModule();
	/** IModuleInterface overrides end */

	/** Bakes out material properties according to MaterialSettings using MeshSettings and stores the output in Output */
	virtual void BakeMaterials(const TArray<FMaterialData*>& MaterialSettings, const TArray<FMeshData*>& MeshSettings, TArray<FBakeOutput>& Output) override;

	/** Bakes out material properties according to extended MaterialSettings using MeshSettings and stores the output in Output */
	virtual void BakeMaterials(const TArray<FMaterialDataEx*>& MaterialSettings, const TArray<FMeshData*>& MeshSettings, TArray<FBakeOutputEx>& Output) override;

	/** Promps a slate window to allow the user to populate specific material baking settings used while baking out materials */
	virtual bool SetupMaterialBakeSettings(TArray<TWeakObjectPtr<UObject>>& OptionObjects, int32 NumLODs) override;

	/** Outputs true HDR version of emissive color */
	virtual void SetEmissiveHDR(bool bHDR) override;

	/** Bakes all material properties to linear textures, except for colors */
	virtual void SetLinearBake(bool bCorrectLinear) override;

	/** Returns whether a specific material property is baked to a linear texture or not */
	virtual bool IsLinearBake(FMaterialPropertyEx Property) override;

protected:
	/* Creates and adds or reuses a RenderTarget from the pool */
	UTextureRenderTarget2D* CreateRenderTarget(bool bInForceLinearGamma, EPixelFormat InPixelFormat, const FIntPoint& InTargetSize, const FColor& BackgroundColor);

	/* Creates and adds (or reuses a ExportMaterialProxy from the pool if MaterialBaking.UseMaterialProxyCaching is set to 1) */
	FExportMaterialProxy* CreateMaterialProxy(const FMaterialDataEx* MaterialSettings, const FMaterialPropertyEx& Property);

	/** Helper for emissive color conversion to Output */
	static void ProcessEmissiveOutput(const FFloat16Color* Color16, int32 Color16Pitch, const FIntPoint& OutputSize, TArray<FColor>& Output, float& EmissiveScale, const FColor& BackgroundColor);

	/** Cleans up all cached material proxies in MaterialProxyPool */
	void CleanupMaterialProxies();

	/** Callback for modified objects which should be removed from MaterialProxyPool in that case */
	void OnObjectModified(UObject* Object);

	/** Callback used to clear material proxy cache on garbage collection */
	void OnPreGarbageCollect();

private:
	enum EPropertyColorSpace
	{
		Linear,
		sRGB,
	};

	/** Pool of available render targets, cached for re-using on consecutive property rendering */
	TArray<UTextureRenderTarget2D*> RenderTargetPool;

	/** Pool of cached material proxies to optimize material baking workflow, stays resident when MaterialBaking.UseMaterialProxyCaching is set to 1 */
	typedef TWeakObjectPtr<UMaterialInterface>				FMaterialPoolKey;
	typedef TPair<FMaterialPropertyEx, FExportMaterialProxy*> FMaterialPoolValue;
	typedef TMultiMap<FMaterialPoolKey, FMaterialPoolValue, FDefaultSetAllocator, TWeakObjectPtrMapKeyFuncs<FMaterialPoolKey, FMaterialPoolValue, true /*bInAllowDuplicateKeys*/>> FMaterialPoolMap;
	FMaterialPoolMap MaterialProxyPool;

	/** Pixel formats to use for baking out specific material properties */
	TMap<FMaterialPropertyEx, EPixelFormat> PerPropertyFormat;

	/** Whether or not to enforce gamma correction while baking out specific material properties */
	TMap<FMaterialPropertyEx, EPropertyColorSpace> PerPropertyColorSpace;

	EPropertyColorSpace DefaultColorSpace;

	bool bEmissiveHDR;
};

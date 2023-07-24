// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "RenderResource.h"
#include "RHIImmutableSamplerState.h"

enum class EMaterialParameterType : uint8;

class FMaterial;
class FMaterialRenderProxy;
class FMaterialShaderMap;
class FMaterialVirtualTextureStack;
class FRHIComputeCommandList;
class FUniformExpressionCacheAsyncUpdater;
class FUniformExpressionSet;
class IAllocatedVirtualTexture;
class UMaterialInterface;
class URuntimeVirtualTexture;
class USparseVolumeTexture;
class USubsurfaceProfile;
class UTexture;

struct FMaterialParameterValue;
struct FMaterialRenderContext;

struct FMemoryImageMaterialParameterInfo;
using FHashedMaterialParameterInfo = FMemoryImageMaterialParameterInfo;


/**
 * Cached uniform expression values.
 */
struct FUniformExpressionCache
{
	/** Material uniform buffer. */
	FUniformBufferRHIRef UniformBuffer;
	/** Allocated virtual textures, one for each entry in FUniformExpressionSet::VTStacks */
	TArray<IAllocatedVirtualTexture*> AllocatedVTs;
	/** Allocated virtual textures that will need destroying during a call to ResetAllocatedVTs() */
	TArray<IAllocatedVirtualTexture*> OwnedAllocatedVTs;
	/** Ids of parameter collections needed for rendering. */
	TArray<FGuid> ParameterCollections;
	/** Shader map that was used to cache uniform expressions on this material. This is used for debugging, verifying correct behavior and checking if the cache is up to date. */
	const FMaterialShaderMap* CachedUniformExpressionShaderMap = nullptr;

	/** Destructor. */
	ENGINE_API ~FUniformExpressionCache();

	void ResetAllocatedVTs();
};

struct FUniformExpressionCacheContainer
{
	inline FUniformExpressionCache& operator[](int32 Index)
	{
#if WITH_EDITOR
		return Elements[Index];
#else
		return Elements;
#endif
	}
private:
#if WITH_EDITOR
	FUniformExpressionCache Elements[ERHIFeatureLevel::Num];
#else
	FUniformExpressionCache Elements;
#endif
};

/** Defines a scope to update deferred uniform expression caches using an async task to fill uniform buffers. The scope
 *  attempts to launch async updates immediately, but further async updates can launch within the scope. Only one async
 *  task is launched at a time though. The async task is synced when the scope destructs. The scope enqueues render commands
 *  so it can be used on the game or render threads.
 */
class ENGINE_API FUniformExpressionCacheAsyncUpdateScope
{
public:
	FUniformExpressionCacheAsyncUpdateScope();
	~FUniformExpressionCacheAsyncUpdateScope();

	/** Call if a wait is required within the scope. */
	static void WaitForTask();
};

/**
 * A material render proxy used by the renderer.
 */
class ENGINE_API FMaterialRenderProxy : public FRenderResource, public FNoncopyable
{
public:

	/** Cached uniform expressions. */
	mutable FUniformExpressionCacheContainer UniformExpressionCache;

	/** Cached external texture immutable samplers */
	mutable FImmutableSamplerState ImmutableSamplerState;

	/** Default constructor. */
	FMaterialRenderProxy(FString InMaterialName);

	/** Destructor. */
	virtual ~FMaterialRenderProxy();

	UE_DEPRECATED(5.1, "EvaluateUniformExpressions with a command list is deprecated.")
	void EvaluateUniformExpressions(FUniformExpressionCache& OutUniformExpressionCache, const FMaterialRenderContext& Context, class FRHIComputeCommandList*) const
	{
		EvaluateUniformExpressions(OutUniformExpressionCache, Context);
	}

	/**
	 * Evaluates uniform expressions and stores them in OutUniformExpressionCache.
	 * @param OutUniformExpressionCache - The uniform expression cache to build.
	 * @param MaterialRenderContext - The context for which to cache expressions.
	 */
	void EvaluateUniformExpressions(FUniformExpressionCache& OutUniformExpressionCache, const FMaterialRenderContext& Context, FUniformExpressionCacheAsyncUpdater* Updater = nullptr) const;

	/**
	 * Caches uniform expressions for efficient runtime evaluation.
	 */
	void CacheUniformExpressions(bool bRecreateUniformBuffer);

	/** Cancels an in-flight cache operation. */
	void CancelCacheUniformExpressions();

	/**
	 * Enqueues a rendering command to cache uniform expressions for efficient runtime evaluation.
	 * bRecreateUniformBuffer - whether to recreate the material uniform buffer.
	 *		This is required if the FMaterial is being recompiled (the uniform buffer layout will change).
	 *		This should only be done if the calling code is using FMaterialUpdateContext to recreate the rendering state of primitives using this material, since cached mesh commands also cache uniform buffer pointers.
	 */
	void CacheUniformExpressions_GameThread(bool bRecreateUniformBuffer);

	/**
	 * Invalidates the uniform expression cache.
	 */
	void InvalidateUniformExpressionCache(bool bRecreateUniformBuffer);

	void UpdateUniformExpressionCacheIfNeeded(ERHIFeatureLevel::Type InFeatureLevel) const;


	/** Returns the FMaterial, without using a fallback if the FMaterial doesn't have a valid shader map. Can return NULL. */
	virtual const FMaterial* GetMaterialNoFallback(ERHIFeatureLevel::Type InFeatureLevel) const = 0;
	virtual const FMaterialRenderProxy* GetFallback(ERHIFeatureLevel::Type InFeatureLevel) const = 0;

	// These functions should only be called by the rendering thread.

	/**
	 * Finds the FMaterial to use for rendering this FMaterialRenderProxy.  Will fall back to a default material if needed due to a content error, or async compilation.
	 * The returned FMaterial is guaranteed to have a complete shader map, so all relevant shaders should be available
	 * OutFallbackMaterialRenderProxy - The proxy that corresponds to the returned FMaterial, should be used for further rendering.  May be a fallback material, or 'this' if no fallback was needed
	 */
	const FMaterial& GetMaterialWithFallback(ERHIFeatureLevel::Type InFeatureLevel, const FMaterialRenderProxy*& OutFallbackMaterialRenderProxy) const;

	/**
	 * Finds the FMaterial to use for rendering this FMaterialRenderProxy.  Will fall back to a default material if needed due to a content error, or async compilation.
	 * Will always return a valid FMaterial, but unlike GetMaterialWithFallback, FMaterial's shader map may be incomplete
	 */
	const FMaterial& GetIncompleteMaterialWithFallback(ERHIFeatureLevel::Type InFeatureLevel) const;

	virtual UMaterialInterface* GetMaterialInterface() const { return NULL; }

	bool GetVectorValue(const FHashedMaterialParameterInfo& ParameterInfo, FLinearColor* OutValue, const FMaterialRenderContext& Context) const;
	bool GetScalarValue(const FHashedMaterialParameterInfo& ParameterInfo, float* OutValue, const FMaterialRenderContext& Context) const;
	bool GetTextureValue(const FHashedMaterialParameterInfo& ParameterInfo, const UTexture** OutValue, const FMaterialRenderContext& Context) const;
	bool GetTextureValue(const FHashedMaterialParameterInfo& ParameterInfo, const URuntimeVirtualTexture** OutValue, const FMaterialRenderContext& Context) const;
	bool GetTextureValue(const FHashedMaterialParameterInfo& ParameterInfo, const USparseVolumeTexture** OutValue, const FMaterialRenderContext& Context) const;
	virtual bool GetParameterValue(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, FMaterialParameterValue& OutValue, const FMaterialRenderContext& Context) const = 0;

	bool IsDeleted() const
	{
		return DeletedFlag != 0;
	}

	void MarkForGarbageCollection()
	{
		MarkedForGarbageCollection = 1;
	}

	bool IsMarkedForGarbageCollection() const
	{
		return MarkedForGarbageCollection != 0;
	}

	// FRenderResource interface.
	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;
	virtual void ReleaseResource() override;

#if WITH_EDITOR
	static const TSet<FMaterialRenderProxy*>& GetMaterialRenderProxyMap()
	{
		check(!FPlatformProperties::RequiresCookedData());
		return MaterialRenderProxyMap;
	}

	static FCriticalSection& GetMaterialRenderProxyMapLock()
	{
		return MaterialRenderProxyMapLock;
	}
#endif

	void SetSubsurfaceProfileRT(const USubsurfaceProfile* Ptr) { SubsurfaceProfileRT = Ptr; }
	const USubsurfaceProfile* GetSubsurfaceProfileRT() const { return SubsurfaceProfileRT; }

	static void UpdateDeferredCachedUniformExpressions();

	static bool HasDeferredUniformExpressionCacheRequests();

	int32 GetExpressionCacheSerialNumber() const { return UniformExpressionCacheSerialNumber; }

	const FString& GetMaterialName() const { return MaterialName; }

private:
	IAllocatedVirtualTexture* GetPreallocatedVTStack(const FMaterialRenderContext& Context, const FUniformExpressionSet& UniformExpressionSet, const FMaterialVirtualTextureStack& VTStack) const;
	IAllocatedVirtualTexture* AllocateVTStack(const FMaterialRenderContext& Context, const FUniformExpressionSet& UniformExpressionSet, const FMaterialVirtualTextureStack& VTStack) const;

	virtual void StartCacheUniformExpressions() const {}
	virtual void FinishCacheUniformExpressions() const {}

	/** 0 if not set, game thread pointer, do not dereference, only for comparison */
	const USubsurfaceProfile* SubsurfaceProfileRT;
	FString MaterialName;

	/** Incremented each time UniformExpressionCache is modified */
	mutable int32 UniformExpressionCacheSerialNumber = 0;

	/** For tracking down a bug accessing a deleted proxy. */
	mutable int8 MarkedForGarbageCollection : 1;
	mutable int8 DeletedFlag : 1;
	mutable int8 ReleaseResourceFlag : 1;
	/** If any VT producer destroyed callbacks have been registered */
	mutable int8 HasVirtualTextureCallbacks : 1;

#if WITH_EDITOR
	/**
	 * Tracks all material render proxies in all scenes.
	 * This is used to propagate new shader maps to materials being used for rendering.
	 */
	static TSet<FMaterialRenderProxy*> MaterialRenderProxyMap;

	/**
	 * Lock that guards the access to the render proxy map
	 */
	static FCriticalSection MaterialRenderProxyMapLock;
#endif

	static TSet<FMaterialRenderProxy*> DeferredUniformExpressionCacheRequests;
};

/**
 * An material render proxy which overrides the material's Color vector parameter.
 */
class ENGINE_API FColoredMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const FLinearColor Color;
	FName ColorParamName;

	/** Initialization constructor. */
	FColoredMaterialRenderProxy(const FMaterialRenderProxy* InParent, const FLinearColor& InColor, FName InColorParamName = NAME_Color);

	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterialNoFallback(ERHIFeatureLevel::Type InFeatureLevel) const override;
	virtual const FMaterialRenderProxy* GetFallback(ERHIFeatureLevel::Type InFeatureLevel) const override;
	virtual bool GetParameterValue(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, FMaterialParameterValue& OutValue, const FMaterialRenderContext& Context) const override;
};

/**
 * An material render proxy which overrides the material's Color vector and Texture parameter (mixed together).
 */
class ENGINE_API FColoredTexturedMaterialRenderProxy : public FColoredMaterialRenderProxy
{
public:
	const UTexture* Texture;
	FName TextureParamName;

	/** Initialization constructor. */
	FColoredTexturedMaterialRenderProxy(const FMaterialRenderProxy* InParent, const FLinearColor& InColor, FName InColorParamName, const UTexture* InTexture, FName InTextureParamName);

	virtual bool GetParameterValue(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, FMaterialParameterValue& OutValue, const FMaterialRenderContext& Context) const override;
};

/**
 * A material render proxy which overrides the selection color
 */
class ENGINE_API FOverrideSelectionColorMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const FLinearColor SelectionColor;

	/** Initialization constructor. */
	FOverrideSelectionColorMaterialRenderProxy(const FMaterialRenderProxy* InParent, const FLinearColor& InSelectionColor);

	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterialNoFallback(ERHIFeatureLevel::Type InFeatureLevel) const override;
	virtual const FMaterialRenderProxy* GetFallback(ERHIFeatureLevel::Type InFeatureLevel) const override;
	virtual bool GetParameterValue(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, FMaterialParameterValue& OutValue, const FMaterialRenderContext& Context) const override;
};

/**
 * An material render proxy which overrides the material's Color and Lightmap resolution vector parameter.
 */
class ENGINE_API FLightingDensityMaterialRenderProxy : public FColoredMaterialRenderProxy
{
public:
	const FVector2D LightmapResolution;

	/** Initialization constructor. */
	FLightingDensityMaterialRenderProxy(const FMaterialRenderProxy* InParent, const FLinearColor& InColor, const FVector2D& InLightmapResolution);

	// FMaterialRenderProxy interface.
	virtual bool GetParameterValue(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, FMaterialParameterValue& OutValue, const FMaterialRenderContext& Context) const override;
};

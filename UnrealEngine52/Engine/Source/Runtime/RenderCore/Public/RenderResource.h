// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderResource.h: Render resource definitions.
=============================================================================*/

#pragma once

#include "RHIFwd.h"
#include "RHIShaderPlatform.h"
#include "RHIFeatureLevel.h"
#include "RenderTimer.h"
#include "CoreGlobals.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Containers/Array.h"
#include "Containers/ResourceArray.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "Containers/EnumAsByte.h"
#include "Containers/List.h"
#include "Containers/ResourceArray.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Misc/AssertionMacros.h"
#include "PixelFormat.h"
#include "RenderCore.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RHIDefinitions.h"
#include "RenderingThread.h"
#include "Serialization/MemoryLayout.h"
#include "DataDrivenShaderPlatformInfo.h"
#endif

class FRDGPooledBuffer;
class FResourceArrayInterface;

/** Number of frames after which unused global resource allocations will be discarded. */
extern int32 GGlobalBufferNumFramesUnusedThresold;

/** Experimental: whether we free helper structures after submitting to RHI. */
extern RENDERCORE_API bool GFreeStructuresOnRHIBufferCreation;

enum class ERenderResourceState : uint8
{
	Default,
	BatchReleased,
	Deleted,
};

enum class ERayTracingMode : uint8
{
	Disabled,	
	Enabled,
	Dynamic
};

/**
 * A rendering resource which is owned by the rendering thread.
 * NOTE - Adding new virtual methods to this class may require stubs added to FViewport/FDummyViewport, otherwise certain modules may have link errors
 */
class RENDERCORE_API FRenderResource
{
public:
	////////////////////////////////////////////////////////////////////////////////////
	// The following methods may not be called while asynchronously initializing / releasing render resources.

	/** Release all render resources that are currently initialized. */
	static void ReleaseRHIForAllResources();

	/** Initialize all resources initialized before the RHI was initialized. */
	static void InitPreRHIResources();

	/** Call periodically to coalesce the render resource list. */
	static void CoalesceResourceList();

	/** Reinitializes render resources at a new feature level. */
	static void ChangeFeatureLevel(ERHIFeatureLevel::Type NewFeatureLevel);

	////////////////////////////////////////////////////////////////////////////////////

	/** Default constructor. */
	FRenderResource();

	/** Constructor when we know what feature level this resource should support */
	FRenderResource(ERHIFeatureLevel::Type InFeatureLevel);

	/** Destructor used to catch unreleased resources. */
	virtual ~FRenderResource();

	/**
	 * Initializes the dynamic RHI resource and/or RHI render target used by this resource.
	 * Called when the resource is initialized, or when reseting all RHI resources.
	 * Resources that need to initialize after a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void InitDynamicRHI() {}

	/**
	 * Releases the dynamic RHI resource and/or RHI render target resources used by this resource.
	 * Called when the resource is released, or when reseting all RHI resources.
	 * Resources that need to release before a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseDynamicRHI() {}

	/**
	 * Initializes the RHI resources used by this resource.
	 * Called when entering the state where both the resource and the RHI have been initialized.
	 * This is only called by the rendering thread.
	 */
	virtual void InitRHI() {}

	/**
	 * Releases the RHI resources used by this resource.
	 * Called when leaving the state where both the resource and the RHI have been initialized.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseRHI() {}

	/**
	 * Initializes the resource.
	 * This is only called by the rendering thread.
	 */
	virtual void InitResource();

	/**
	 * Prepares the resource for deletion.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseResource();

	/**
	 * If the resource's RHI resources have been initialized, then release and reinitialize it.  Otherwise, do nothing.
	 * This is only called by the rendering thread.
	 */
	void UpdateRHI();

	/** @return The resource's friendly name.  Typically a UObject name. */
	virtual FString GetFriendlyName() const { return TEXT("undefined"); }

	// Accessors.
	FORCEINLINE bool IsInitialized() const { return ListIndex != INDEX_NONE; }

	int32 GetListIndex() const { return ListIndex; }

	/** SetOwnerName should be called before BeginInitResource for the owner name to be successfully tracked. */
	void SetOwnerName(const FName& InOwnerName);
	FName GetOwnerName() const;

protected:
	// This is used during mobile editor preview refactor, this will eventually be replaced with a parameter to InitRHI() etc..
	void SetFeatureLevel(const FStaticFeatureLevel InFeatureLevel) { FeatureLevel = (ERHIFeatureLevel::Type)InFeatureLevel; }
	const FStaticFeatureLevel GetFeatureLevel() const { return FeatureLevel == ERHIFeatureLevel::Num ? FStaticFeatureLevel(GMaxRHIFeatureLevel) : FeatureLevel; }
	FORCEINLINE bool HasValidFeatureLevel() const { return FeatureLevel < ERHIFeatureLevel::Num; }

	// Helper for submitting a resource array to RHI and freeing eligible CPU memory
	template<bool bRenderThread, typename T>
	FBufferRHIRef CreateRHIBuffer(T& InOutResourceObject, const uint32 ResourceCount, EBufferUsageFlags InBufferUsageFlags, const TCHAR* InDebugName)
	{
		FBufferRHIRef Buffer;
		FResourceArrayInterface* RESTRICT ResourceArray = InOutResourceObject ? InOutResourceObject->GetResourceArray() : nullptr;
		if (ResourceCount != 0)
		{
			Buffer = CreateRHIBufferInternal(InDebugName, GetOwnerName(), ResourceCount, InBufferUsageFlags, ResourceArray, bRenderThread, InOutResourceObject == nullptr);
			if (!bRenderThread)
			{
				return Buffer;
			}
		}

		// If the buffer creation emptied the resource array, delete the containing structure as well
		if (ShouldFreeResourceObject(InOutResourceObject, ResourceArray))
		{
			delete InOutResourceObject;
			InOutResourceObject = nullptr;
		}

		return Buffer;
	}

private:
	static bool ShouldFreeResourceObject(void* ResourceObject, FResourceArrayInterface* ResourceArray);
	static FBufferRHIRef CreateRHIBufferInternal(
		const TCHAR* InDebugName,
		const FName& InOwnerName,
		uint32 ResourceCount,
		EBufferUsageFlags InBufferUsageFlags,
		FResourceArrayInterface* ResourceArray,
		bool bRenderThread,
		bool bWithoutNativeResource
	);

#if RHI_ENABLE_RESOURCE_INFO
	FName OwnerName;
#endif

	int32 ListIndex;
	TEnumAsByte<ERHIFeatureLevel::Type> FeatureLevel;

public:
	ERenderResourceState ResourceState = ERenderResourceState::Default;
};

/**
 * Sends a message to the rendering thread to initialize a resource.
 * This is called in the game thread.
 */
extern RENDERCORE_API void BeginInitResource(FRenderResource* Resource);

/**
 * Sends a message to the rendering thread to update a resource.
 * This is called in the game thread.
 */
extern RENDERCORE_API void BeginUpdateResourceRHI(FRenderResource* Resource);

/**
 * Sends a message to the rendering thread to release a resource.
 * This is called in the game thread.
 */
extern RENDERCORE_API void BeginReleaseResource(FRenderResource* Resource);

/**
* Enables the batching of calls to BeginReleaseResource
* This is called in the game thread.
*/
extern RENDERCORE_API void StartBatchedRelease();

/**
* Disables the batching of calls to BeginReleaseResource
* This is called in the game thread.
*/
extern RENDERCORE_API void EndBatchedRelease();

/**
 * Sends a message to the rendering thread to release a resource, and spins until the rendering thread has processed the message.
 * This is called in the game thread.
 */
extern RENDERCORE_API void ReleaseResourceAndFlush(FRenderResource* Resource);

enum EMipFadeSettings
{
	MipFade_Normal = 0,
	MipFade_Slow,

	MipFade_NumSettings,
};

/** Mip fade settings, selectable by chosing a different EMipFadeSettings. */
struct FMipFadeSettings
{
	FMipFadeSettings( float InFadeInSpeed, float InFadeOutSpeed )
		:	FadeInSpeed( InFadeInSpeed )
		,	FadeOutSpeed( InFadeOutSpeed )
	{
	}

	/** How many seconds to fade in one mip-level. */
	float FadeInSpeed;

	/** How many seconds to fade out one mip-level. */
	float FadeOutSpeed;
};

/** Whether to enable mip-level fading or not: +1.0f if enabled, -1.0f if disabled. */
extern RENDERCORE_API float GEnableMipLevelFading;

/** Global mip fading settings, indexed by EMipFadeSettings. */
extern RENDERCORE_API FMipFadeSettings GMipFadeSettings[MipFade_NumSettings];

/**
 * Functionality for fading in/out texture mip-levels.
 */
struct FMipBiasFade
{
	/** Default constructor that sets all values to default (no mips). */
	FMipBiasFade()
	:	TotalMipCount(0.0f)
	,	MipCountDelta(0.0f)
	,	StartTime(0.0f)
	,	MipCountFadingRate(0.0f)
	,	BiasOffset(0.0f)
	{
	}

	/** Number of mip-levels in the texture. */
	float	TotalMipCount;

	/** Number of mip-levels to fade (negative if fading out / decreasing the mipcount). */
	float	MipCountDelta;

	/** Timestamp when the fade was started. */
	float	StartTime;

	/** Number of seconds to interpolate through all MipCountDelta (inverted). */
	float	MipCountFadingRate;

	/** Difference between total texture mipcount and the starting mipcount for the fade. */
	float	BiasOffset;

	/**
	 *	Sets up a new interpolation target for the mip-bias.
	 *	@param ActualMipCount	Number of mip-levels currently in memory
	 *	@param TargetMipCount	Number of mip-levels we're changing to
	 *	@param LastRenderTime	Timestamp when it was last rendered (FApp::CurrentTime time space)
	 *	@param FadeSetting		Which fade speed settings to use
	 */
	RENDERCORE_API void	SetNewMipCount( float ActualMipCount, float TargetMipCount, double LastRenderTime, EMipFadeSettings FadeSetting );

	/**
	 *	Calculates the interpolated mip-bias based on the current time.
	 *	@return				Interpolated mip-bias value
	 */
	inline float	CalcMipBias() const
	{
		float DeltaTime		= GRenderingRealtimeClock.GetCurrentTime() - StartTime;
		float TimeFactor	= FMath::Min<float>(DeltaTime * MipCountFadingRate, 1.0f);
		float MipBias		= BiasOffset - MipCountDelta*TimeFactor;
		return FMath::FloatSelect(GEnableMipLevelFading, MipBias, 0.0f);
	}

	/**
	 *	Checks whether the mip-bias is still interpolating.
	 *	@return				true if the mip-bias is still interpolating
	 */
	inline bool	IsFading( ) const
	{
		float DeltaTime = GRenderingRealtimeClock.GetCurrentTime() - StartTime;
		float TimeFactor = DeltaTime * MipCountFadingRate;
		return (FMath::Abs<float>(MipCountDelta) > UE_SMALL_NUMBER && TimeFactor < 1.0f);
	}
};

/** A textures resource. */
class RENDERCORE_API FTexture : public FRenderResource
{
public:

	/** The texture's RHI resource. */
	FTextureRHIRef		TextureRHI;

	/** The sampler state to use for the texture. */
	FSamplerStateRHIRef SamplerStateRHI;

	/** Sampler state to be used in deferred passes when discontinuities in ddx / ddy would cause too blurry of a mip to be used. */
	FSamplerStateRHIRef DeferredPassSamplerStateRHI;

	/** The last time the texture has been bound */
	mutable double		LastRenderTime = -FLT_MAX;

	/** Base values for fading in/out mip-levels. */
	FMipBiasFade		MipBiasFade;

	/** bGreyScaleFormat indicates the texture is actually in R channel but should be read as Grey (replicate R to RGBA)
	 *  this is set from CompressionSettings, not PixelFormat
	 *  this is only used by Editor/Debug shaders, not real game materials, which use SamplerType from MaterialExpressions
	 */
	bool				bGreyScaleFormat = false;

	/**
	 * true if the texture is in the same gamma space as the intended rendertarget (e.g. screenshots).
	 * The texture will have sRGB==false and bIgnoreGammaConversions==true, causing a non-sRGB texture lookup
	 * and no gamma-correction in the shader.
	 */
	bool				bIgnoreGammaConversions = false;

	/** 
	 * Is the pixel data in this texture sRGB?
	 **/
	bool				bSRGB = false;

	FTexture();
	virtual ~FTexture();

	const FTextureRHIRef& GetTextureRHI() { return TextureRHI; }

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const;

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const;

	/** Returns the depth of the texture in pixels. */
	virtual uint32 GetSizeZ() const;

	// FRenderResource interface.
	virtual void ReleaseRHI() override;
	virtual FString GetFriendlyName() const override;

protected:
	static FRHISamplerState* GetOrCreateSamplerState(const FSamplerStateInitializerRHI& Initializer);
};

/** A textures resource that includes an SRV. */
class RENDERCORE_API FTextureWithSRV : public FTexture
{
public:
	FTextureWithSRV();
	virtual ~FTextureWithSRV();

	virtual void ReleaseRHI() override;

	/** SRV that views the entire texture */
	FShaderResourceViewRHIRef ShaderResourceViewRHI;
};

/** A texture reference resource. */
class RENDERCORE_API FTextureReference : public FRenderResource
{
public:
	/** The texture reference's RHI resource. */
	FTextureReferenceRHIRef	TextureReferenceRHI;

private:
	/** True if the texture reference has been initialized from the game thread. */
	bool bInitialized_GameThread;

public:
	/** Default constructor. */
	FTextureReference();

	// Destructor
	virtual ~FTextureReference();

	/** Returns the last time the texture has been rendered via this reference. */
	double GetLastRenderTime() const;

	/** Invalidates the last render time. */
	void InvalidateLastRenderTime();

	/** Returns true if the texture reference has been initialized from the game thread. */
	bool IsInitialized_GameThread() const { return bInitialized_GameThread; }

	/** Kicks off the initialization process on the game thread. */
	void BeginInit_GameThread();

	/** Kicks off the release process on the game thread. */
	void BeginRelease_GameThread();

	// FRenderResource interface.
	virtual void InitRHI();
	virtual void ReleaseRHI();
	virtual FString GetFriendlyName() const;
};

/** A vertex buffer resource */
class RENDERCORE_API FVertexBuffer : public FRenderResource
{
public:
	FVertexBuffer();
	virtual ~FVertexBuffer();

	// FRenderResource interface.
	virtual void ReleaseRHI() override;
	virtual FString GetFriendlyName() const override;

	FBufferRHIRef VertexBufferRHI;
};

class RENDERCORE_API FVertexBufferWithSRV : public FVertexBuffer
{
public:
	FVertexBufferWithSRV();
	~FVertexBufferWithSRV();

	virtual void ReleaseRHI() override;

	/** SRV that views the entire texture */
	FShaderResourceViewRHIRef ShaderResourceViewRHI;

	/** *optional* UAV that views the entire texture */
	FUnorderedAccessViewRHIRef UnorderedAccessViewRHI;
};

/** An index buffer resource. */
class RENDERCORE_API FIndexBuffer : public FRenderResource
{
public:
	FIndexBuffer();
	virtual ~FIndexBuffer();

	// FRenderResource interface.
	virtual void ReleaseRHI() override;
	virtual FString GetFriendlyName() const override;

	FBufferRHIRef IndexBufferRHI;
};

class RENDERCORE_API FBufferWithRDG : public FRenderResource
{
public:
	FBufferWithRDG();
	FBufferWithRDG(const FBufferWithRDG& Other);
	FBufferWithRDG& operator=(const FBufferWithRDG& Other);
	~FBufferWithRDG() override;

	void ReleaseRHI() override;

	TRefCountPtr<FRDGPooledBuffer> Buffer;
};

/** Used to declare a render resource that is initialized/released by static initialization/destruction. */
template<class ResourceType>
class TGlobalResource : public ResourceType
{
public:
	/** Default constructor. */
	TGlobalResource()
	{
		InitGlobalResource();
	}

	/** Initialization constructor: 1 parameter. */
	template<typename... Args>
	explicit TGlobalResource(Args... InArgs)
		: ResourceType(InArgs...)
	{
		InitGlobalResource();
	}

	/** Destructor. */
	virtual ~TGlobalResource()
	{
		ReleaseGlobalResource();
	}

private:

	/**
	 * Initialize the global resource.
	 */
	void InitGlobalResource()
	{
		if (IsInRenderingThread())
		{
			// If the resource is constructed in the rendering thread, directly initialize it.
			((ResourceType*)this)->InitResource();
		}
		else
		{
			// If the resource is constructed outside of the rendering thread, enqueue a command to initialize it.
			BeginInitResource((ResourceType*)this);
		}
	}

	/**
	 * Release the global resource.
	 */
	void ReleaseGlobalResource()
	{
		// This should be called in the rendering thread, or at shutdown when the rendering thread has exited.
		// However, it may also be called at shutdown after an error, when the rendering thread is still running.
		// To avoid a second error in that case we don't assert.
#if 0
		check(IsInRenderingThread());
#endif

		// Cleanup the resource.
		((ResourceType*)this)->ReleaseResource();
	}
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RayTracingGeometry.h"
#include "RenderUtils.h"
#include "GlobalRenderResources.h"
#endif

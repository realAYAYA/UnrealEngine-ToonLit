// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	ParticleGpuSimulation.cpp: Implementation of GPU particle simulation.
==============================================================================*/

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "Math/RandomStream.h"
#include "Stats/Stats.h"
#include "Misc/MemStack.h"
#include "HAL/IConsoleManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "ShaderCompilerCore.h"
#include "VertexFactory.h"
#include "RHIStaticStates.h"
#include "GlobalDistanceFieldParameters.h"
#include "StaticBoundShaderState.h"
#include "Materials/Material.h"
#include "ParticleVertexFactory.h"
#include "SceneUtils.h"
#include "SceneManagement.h"
#include "ParticleHelper.h"
#include "ParticleEmitterInstances.h"
#include "Particles/ParticleSystemComponent.h"
#include "VectorField.h"
#include "CanvasTypes.h"
#include "Particles/FXSystemPrivate.h"
#include "Particles/ParticleSortingGPU.h"
#include "Particles/ParticleCurveTexture.h"
#include "ParticleResources.h"
#include "ShaderParameterUtils.h"
#include "GlobalShader.h"
#include "VectorFieldVisualization.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/Spawn/ParticleModuleSpawnPerUnit.h"
#include "Particles/TypeData/ParticleModuleTypeDataGpu.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "VectorField/VectorField.h"
#include "Misc/CoreDelegates.h"
#include "PipelineStateCache.h"
#include "MeshMaterialShader.h"

DECLARE_CYCLE_STAT(TEXT("GPUSpriteEmitterInstance Init GT"), STAT_GPUSpriteEmitterInstance_Init, STATGROUP_Particles);
DECLARE_GPU_STAT_NAMED(ParticleSimulation, TEXT("Particle Simulation"));

#if WITH_MGPU
DECLARE_GPU_STAT(AFRWaitForParticleSimulation);
#endif

/*------------------------------------------------------------------------------
	Constants to tune memory and performance for GPU particle simulation.
------------------------------------------------------------------------------*/

/** Enable this define to permit tracking of tile allocations by GPU emitters. */
#define TRACK_TILE_ALLOCATIONS 0

/** The texture size allocated for GPU simulation. */
int32 GParticleSimulationTextureSizeX = 1024;
static FAutoConsoleVariableRef CVarParticleSimulationSizeX(
	TEXT("fx.GPUSimulationTextureSizeX"),
	GParticleSimulationTextureSizeX,
	TEXT("GPU Particle simulation texture X dimension (default=1024); set in project renderer settings, potentially overridden by device profile."),
	ECVF_ReadOnly
);

int32 GParticleSimulationTextureSizeY = 1024;
FAutoConsoleVariableRef CVarParticleSimulationSizeY(
	TEXT("fx.GPUSimulationTextureSizeY"),
	GParticleSimulationTextureSizeY,
	TEXT("GPU Particle simulation texture Y dimension (default=1024); set in project renderer settings, potentially overridden by device profile."),
	ECVF_ReadOnly
);

/** The tile size. Texture space is allocated in TileSize x TileSize units. */
const int32 GParticleSimulationTileSize = 4;
const int32 GParticlesPerTile = GParticleSimulationTileSize * GParticleSimulationTileSize;

/** Tile size must be power-of-two and <= each dimension of the simulation texture. */
static_assert((GParticleSimulationTileSize & (GParticleSimulationTileSize - 1)) == 0, "Particle simulation tile size is not a power of two.");

/** How many tiles are in the simulation textures. */
int32 GParticleSimulationTileCountX = 0;
int32 GParticleSimulationTileCountY = 0;
int32 GParticleSimulationTileCount = 0;

/** GPU particle rendering code assumes that the number of particles per instanced draw is <= 16. */
static_assert(MAX_PARTICLES_PER_INSTANCE <= 16, "Max particles per instance is greater than 16.");
/** Also, it must be a power of 2. */
static_assert((MAX_PARTICLES_PER_INSTANCE & (MAX_PARTICLES_PER_INSTANCE - 1)) == 0, "Max particles per instance is not a power of two.");

/** Particle tiles are aligned to the same number as when rendering. */
enum { TILES_PER_INSTANCE = 8 };
/** The number of tiles per instance must be <= MAX_PARTICLES_PER_INSTANCE. */
static_assert(TILES_PER_INSTANCE <= MAX_PARTICLES_PER_INSTANCE, "Tiles per instance is greater than max particles per instance.");
/** Also, it must be a power of 2. */
static_assert((TILES_PER_INSTANCE & (TILES_PER_INSTANCE - 1)) == 0, "Tiles per instance is not a power of two.");

/** Maximum number of vector fields that can be evaluated at once. */
#if GPUPARTICLE_LOCAL_VF_ONLY
enum { MAX_VECTOR_FIELDS = 1 };
#else
enum { MAX_VECTOR_FIELDS = 4 };
#endif

// Using a fix step 1/30, allows game targetting 30 fps and 60 fps to have single iteration updates.
static TAutoConsoleVariable<float> CVarGPUParticleFixDeltaSeconds(TEXT("r.GPUParticle.FixDeltaSeconds"), 1.f/30.f,TEXT("GPU particle fix delta seconds."));
static TAutoConsoleVariable<float> CVarGPUParticleFixTolerance(TEXT("r.GPUParticle.FixTolerance"),.1f,TEXT("Delta second tolerance before switching to a fix delta seconds."));
static TAutoConsoleVariable<int32> CVarGPUParticleMaxNumIterations(TEXT("r.GPUParticle.MaxNumIterations"),3,TEXT("Max number of iteration when using a fix delta seconds."));

static TAutoConsoleVariable<int32> CVarSimulateGPUParticles(TEXT("r.GPUParticle.Simulate"), 1, TEXT("Enable or disable GPU particle simulation"));

static TAutoConsoleVariable<int32> CVarGPUParticleAFRReinject(
	TEXT("r.GPUParticle.AFRReinject"),
	1,
	TEXT("Toggle optimization when running in AFR to re-inject particle injections on the next GPU rather than doing a slow GPU->GPU transfer of the texture data\n")	
	TEXT("  0: Reinjection off\n")
	TEXT("  1: Reinjection on"),
	ECVF_ReadOnly);

/*-----------------------------------------------------------------------------
	Allocators used to manage GPU particle resources.
-----------------------------------------------------------------------------*/

/**
 * Stack allocator for managing tile lifetime.
 */
class FParticleTileAllocator
{
public:

	/** Default constructor. */
	FParticleTileAllocator()
		: FreeTileCount(GParticleSimulationTileCount)
	{
		/** Texture size must be power-of-two. */
		check((GParticleSimulationTextureSizeX & (GParticleSimulationTextureSizeX - 1)) == 0); // fx.GPUSimulationTextureSizeX is not a power of two.
		check((GParticleSimulationTextureSizeY & (GParticleSimulationTextureSizeY - 1)) == 0); // fx.GPUSimulationTextureSizeY is not a power of two.

		check(GParticleSimulationTileSize <= GParticleSimulationTextureSizeX); // Particle simulation tile size is larger than fx.GPUSimulationTextureSizeX.
		check(GParticleSimulationTileSize <= GParticleSimulationTextureSizeY); // Particle simulation tile size is larger than fx.GPUSimulationTextureSizeY.

		FreeTiles.AddUninitialized(GParticleSimulationTileCount);

		for ( int32 TileIndex = 0; TileIndex < GParticleSimulationTileCount; ++TileIndex )
		{
			FreeTiles[TileIndex] = GParticleSimulationTileCount - TileIndex - 1;
		}
	}

	/**
	 * Allocate a tile.
	 * @returns the index of the allocated tile, INDEX_NONE if no tiles are available.
	 */
	uint32 Allocate()
	{
		FScopeLock Lock(&CriticalSection);
		if ( FreeTileCount > 0 )
		{
			FreeTileCount--;
			return FreeTiles[FreeTileCount];
		}
		return INDEX_NONE;
	}

	/**
	 * Frees a tile so it may be allocated by another emitter.
	 * @param TileIndex - The index of the tile to free.
	 */
	void Free( int32 TileIndex )
	{
		FScopeLock Lock(&CriticalSection);
		check( TileIndex < GParticleSimulationTileCount );
		check( FreeTileCount < GParticleSimulationTileCount );
		FreeTiles[FreeTileCount] = TileIndex;
		FreeTileCount++;
	}

	/**
	 * Returns the number of free tiles.
	 */
	int32 GetFreeTileCount() const
	{
		FScopeLock Lock(&CriticalSection);
		return FreeTileCount;
	}

private:

	/** List of free tiles. */
	TArray<uint32> FreeTiles;
	/** How many tiles are in the free list. */
	int32 FreeTileCount;

	mutable FCriticalSection CriticalSection;
};

/*-----------------------------------------------------------------------------
	GPU resources required for simulation.
-----------------------------------------------------------------------------*/

/**
 * Per-particle information stored in a vertex buffer for drawing GPU sprites.
 */
struct FParticleIndex
{
	/** The X coordinate of the particle within the texture. */
	FFloat16 X;
	/** The Y coordinate of the particle within the texture. */
	FFloat16 Y;
};

/**
 * Texture resources holding per-particle state required for GPU simulation.
 */
class FParticleStateTextures : public FRenderResource
{
public:

	/** Contains the positions of all simulating particles. */
	FTexture2DRHIRef PositionTextureRHI;
	/** Contains the velocity of all simulating particles. */
	FTexture2DRHIRef VelocityTextureRHI;

	bool bTexturesCleared;
	int32 ParticleStateIndex = 0;

	/**
	 * Initialize RHI resources used for particle simulation.
	 */
	virtual void InitRHI() override
	{
		const int32 SizeX = GParticleSimulationTextureSizeX;
		const int32 SizeY = GParticleSimulationTextureSizeY;

		// 32-bit per channel RGBA texture for position.
		check( !IsValidRef( PositionTextureRHI ) );

#define PARTICLE_STATE_POSITION_TEXTURE_NAME	TEXT("ParticleStatePosition")
#define PARTICLE_STATE_VELOCITY_TEXTURE_NAME	TEXT("ParticleStateVelocity")

		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2D(PARTICLE_STATE_POSITION_TEXTURE_NAME)
				.SetExtent(SizeX, SizeY)
				.SetFormat(PF_A32B32G32R32F)
				.SetClearValue(FClearValueBinding::Transparent)
				.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
				.SetInitialState(ERHIAccess::SRVMask);

			PositionTextureRHI = RHICreateTexture(Desc);
		}

		// 16-bit per channel RGBA texture for velocity.
		check( !IsValidRef( VelocityTextureRHI ) );

		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2D(PARTICLE_STATE_VELOCITY_TEXTURE_NAME)
				.SetExtent(SizeX, SizeY)
				.SetFormat(PF_FloatRGBA)
				.SetClearValue(FClearValueBinding::Transparent)
				.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
				.SetInitialState(ERHIAccess::SRVMask);

			VelocityTextureRHI = RHICreateTexture(Desc);
		}

		// using FName's ability to append a number to a string (..._0) without an extra string allocation, except suffixing is done when number > 0 hence the +1 here : 
		FName PositionTextureName(PARTICLE_STATE_POSITION_TEXTURE_NAME, ParticleStateIndex + 1); 
		FName VelocityTextureName(PARTICLE_STATE_VELOCITY_TEXTURE_NAME, ParticleStateIndex + 1);
		PositionTextureRHI->SetName(PositionTextureName);
		VelocityTextureRHI->SetName(VelocityTextureName);
		RHIBindDebugLabelName(PositionTextureRHI, *PositionTextureName.ToString());
		RHIBindDebugLabelName(VelocityTextureRHI, *VelocityTextureName.ToString());
#undef PARTICLE_STATE_VELOCITY_TEXTURE_NAME
#undef PARTICLE_STATE_POSITION_TEXTURE_NAME

		bTexturesCleared = false;
	}

	/**
	 * Releases RHI resources used for particle simulation.
	 */
	virtual void ReleaseRHI() override
	{
		PositionTextureRHI.SafeRelease();
		VelocityTextureRHI.SafeRelease();
	}
};

/**
 * A texture holding per-particle attributes.
 */
class FParticleAttributesTexture : public FRenderResource
{
public:

	/** Contains the attributes of all simulating particles. */
	FTextureRHIRef TextureRHI;

	/**
	 * Initialize RHI resources used for particle simulation.
	 */
	virtual void InitRHI() override
	{
		const int32 SizeX = GParticleSimulationTextureSizeX;
		const int32 SizeY = GParticleSimulationTextureSizeY;

		const ETextureCreateFlags ExtraFlags = CVarGPUParticleAFRReinject.GetValueOnRenderThread() == 1 ? TexCreate_AFRManual : TexCreate_None;

#define ATTRIBUTES_TEXTURE_NAME	TEXT("ParticleAttributes")

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(ATTRIBUTES_TEXTURE_NAME)
			.SetExtent(SizeX, SizeY)
			.SetFormat(PF_B8G8R8A8)
			.SetClearValue(FClearValueBinding::Transparent)
			.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource | ETextureCreateFlags::NoFastClear | ExtraFlags)
			.SetInitialState(ERHIAccess::RTV);

		TextureRHI = RHICreateTexture(Desc);

		static FName AttributesTextureName(ATTRIBUTES_TEXTURE_NAME);
		TextureRHI->SetName(AttributesTextureName);
		RHIBindDebugLabelName(TextureRHI, ATTRIBUTES_TEXTURE_NAME);
#undef ATTRIBUTES_TEXTURE_NAME

		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		ClearRenderTarget(RHICmdList, TextureRHI);
		RHICmdList.Transition(FRHITransitionInfo(TextureRHI, ERHIAccess::RTV, ERHIAccess::SRVMask));
	}

	/**
	 * Releases RHI resources used for particle simulation.
	 */
	virtual void ReleaseRHI() override
	{
		TextureRHI.SafeRelease();
	}
};

/**
 * Resources required for GPU particle simulation.
 */
class FParticleSimulationResources
{
public:

	/** Textures needed for simulation, double buffered. */
	FParticleStateTextures StateTextures[2];
	/** Texture holding render attributes. */
	FParticleAttributesTexture RenderAttributesTexture;
	/** Texture holding simulation attributes. */
	FParticleAttributesTexture SimulationAttributesTexture;

	/** Frame index used to track double buffered resources on the GPU. */
	int32 FrameIndex = 0;
	/** LWC tile offset, will be 0,0,0 for localspace emitters. */
	FVector3f LWCTile = FVector3f::ZeroVector;

	/**
	 * Initialize resources.
	 */
	void Init()
	{
		// Help debugging by identifying each state :
		StateTextures[0].ParticleStateIndex = 0;
		StateTextures[1].ParticleStateIndex = 1;

		FParticleSimulationResources* ParticleResources = this;
		ENQUEUE_RENDER_COMMAND(FInitParticleSimulationResourcesCommand)([ParticleResources](FRHICommandList& RHICmdList)
		{
			ParticleResources->StateTextures[0].InitResource();
			ParticleResources->StateTextures[1].InitResource();
			ParticleResources->RenderAttributesTexture.InitResource();
			ParticleResources->SimulationAttributesTexture.InitResource();

			RHICmdList.Transition({ 
				FRHITransitionInfo(ParticleResources->RenderAttributesTexture.TextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask),
				FRHITransitionInfo(ParticleResources->SimulationAttributesTexture.TextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask),
				FRHITransitionInfo(ParticleResources->StateTextures[0].PositionTextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask),
				FRHITransitionInfo(ParticleResources->StateTextures[0].VelocityTextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask),
				FRHITransitionInfo(ParticleResources->StateTextures[1].PositionTextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask),
				FRHITransitionInfo(ParticleResources->StateTextures[1].VelocityTextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask)
			});
		});
	}

	/**
	 * Release resources.
	 */
	void Release()
	{
		FParticleSimulationResources* ParticleResources = this;
		ENQUEUE_RENDER_COMMAND(FReleaseParticleSimulationResourcesCommand)([ParticleResources](FRHICommandList& RHICmdList)
		{
			ParticleResources->StateTextures[0].ReleaseResource();
			ParticleResources->StateTextures[1].ReleaseResource();
			ParticleResources->RenderAttributesTexture.ReleaseResource();
			ParticleResources->SimulationAttributesTexture.ReleaseResource();
		});
	}

	/**
	 * Destroy resources.
	 */
	void Destroy()
	{
		FParticleSimulationResources* ParticleResources = this;
		ENQUEUE_RENDER_COMMAND(FDestroyParticleSimulationResourcesCommand)(
			[ParticleResources](FRHICommandList& RHICmdList)
			{
				delete ParticleResources;
			});
	}

	/**
	 * Retrieve texture resources with up-to-date particle state.
	 */
	FParticleStateTextures& GetCurrentStateTextures()
	{
		return StateTextures[FrameIndex];
	}

	/**
	 * Retrieve texture resources with previous particle state.
	 */
	FParticleStateTextures& GetPreviousStateTextures()
	{
		return StateTextures[FrameIndex ^ 0x1];
	}

	FParticleStateTextures& GetVisualizeStateTextures()
	{
		const float FixDeltaSeconds = CVarGPUParticleFixDeltaSeconds.GetValueOnRenderThread();
		if (FixDeltaSeconds > 0)
		{
			return GetPreviousStateTextures();
		}
		else
		{
			return GetCurrentStateTextures();
		}
	}
	/**
	 * Allocate a particle tile.
	 */
	uint32 AllocateTile()
	{
		return TileAllocator.Allocate();
	}

	/**
	 * Free a particle tile.
	 */
	void FreeTile( uint32 Tile )
	{
		TileAllocator.Free( Tile );
	}

	/**
	 * Returns the number of free tiles.
	 */
	int32 GetFreeTileCount() const
	{
		return TileAllocator.GetFreeTileCount();
	}

private:

	/** Allocator for managing particle tiles. */
	FParticleTileAllocator TileAllocator;
};


/*-----------------------------------------------------------------------------
	Vertex factory.
-----------------------------------------------------------------------------*/

/**
 * Uniform buffer for GPU particle sprite emitters.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FGPUSpriteEmitterUniformParameters,)
	SHADER_PARAMETER(FVector4f, ColorCurve)
	SHADER_PARAMETER(FVector4f, ColorScale)
	SHADER_PARAMETER(FVector4f, ColorBias)
	SHADER_PARAMETER(FVector4f, MiscCurve)
	SHADER_PARAMETER(FVector4f, MiscScale)
	SHADER_PARAMETER(FVector4f, MiscBias)
	SHADER_PARAMETER(FVector4f, SizeBySpeed)
	SHADER_PARAMETER(FVector4f, SubImageSize)
	SHADER_PARAMETER(FVector4f, TangentSelector)
	SHADER_PARAMETER(FVector3f, CameraFacingBlend)
	SHADER_PARAMETER(float, RemoveHMDRoll)
	SHADER_PARAMETER(float, RotationRateScale)
	SHADER_PARAMETER(float, RotationBias)
	SHADER_PARAMETER(float, CameraMotionBlurAmount)
	SHADER_PARAMETER(FVector2f, PivotOffset)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FGPUSpriteEmitterUniformParameters, "EmitterUniforms");

typedef TUniformBufferRef<FGPUSpriteEmitterUniformParameters> FGPUSpriteEmitterUniformBufferRef;

/**
 * Uniform buffer to hold dynamic parameters for GPU particle sprite emitters.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FGPUSpriteEmitterDynamicUniformParameters, )
	SHADER_PARAMETER( FVector3f, LWCTile )
	SHADER_PARAMETER( FVector2f, LocalToWorldScale )
	SHADER_PARAMETER( float, EmitterInstRandom)
	SHADER_PARAMETER( FVector4f, AxisLockRight )
	SHADER_PARAMETER( FVector4f, AxisLockUp )
	SHADER_PARAMETER( FVector4f, DynamicColor)
	SHADER_PARAMETER( FVector4f, MacroUVParameters )
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FGPUSpriteEmitterDynamicUniformParameters, "EmitterDynamicUniforms");

typedef TUniformBufferRef<FGPUSpriteEmitterDynamicUniformParameters> FGPUSpriteEmitterDynamicUniformBufferRef;

/**
 * Vertex shader parameters for the particle vertex factory.
 */
class FGPUSpriteVertexFactoryShaderParametersVS : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FGPUSpriteVertexFactoryShaderParametersVS, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ParticleIndices.Bind(ParameterMap, TEXT("ParticleIndices"));
		ParticleIndicesOffset.Bind(ParameterMap, TEXT("ParticleIndicesOffset"));
		PositionTexture.Bind(ParameterMap, TEXT("PositionTexture"));
		PositionTextureSampler.Bind(ParameterMap, TEXT("PositionTextureSampler"));
		VelocityTexture.Bind(ParameterMap, TEXT("VelocityTexture"));
		VelocityTextureSampler.Bind(ParameterMap, TEXT("VelocityTextureSampler"));
		AttributesTexture.Bind(ParameterMap, TEXT("AttributesTexture"));
		AttributesTextureSampler.Bind(ParameterMap, TEXT("AttributesTextureSampler"));
		CurveTexture.Bind(ParameterMap, TEXT("CurveTexture"));
		CurveTextureSampler.Bind(ParameterMap, TEXT("CurveTextureSampler"));
	}

	void GetElementShaderBindings(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const;
private:
	/** Buffer containing particle indices. */
	LAYOUT_FIELD(FShaderResourceParameter, ParticleIndices);
	/** Offset in to the particle indices buffer. */
	LAYOUT_FIELD(FShaderParameter, ParticleIndicesOffset);
	/** Texture containing positions for all particles. */
	LAYOUT_FIELD(FShaderResourceParameter, PositionTexture);
	LAYOUT_FIELD(FShaderResourceParameter, PositionTextureSampler);
	/** Texture containing velocities for all particles. */
	LAYOUT_FIELD(FShaderResourceParameter, VelocityTexture);
	LAYOUT_FIELD(FShaderResourceParameter, VelocityTextureSampler);
	/** Texture containint attributes for all particles. */
	LAYOUT_FIELD(FShaderResourceParameter, AttributesTexture);
	LAYOUT_FIELD(FShaderResourceParameter, AttributesTextureSampler);
	/** Texture containing curves from which attributes are sampled. */
	LAYOUT_FIELD(FShaderResourceParameter, CurveTexture);
	LAYOUT_FIELD(FShaderResourceParameter, CurveTextureSampler);
};

IMPLEMENT_TYPE_LAYOUT(FGPUSpriteVertexFactoryShaderParametersVS);

/**
 * Pixel shader parameters for the particle vertex factory.
 */
class FGPUSpriteVertexFactoryShaderParametersPS : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FGPUSpriteVertexFactoryShaderParametersPS, NonVirtual);
public:
	void GetElementShaderBindings(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const;
};

IMPLEMENT_TYPE_LAYOUT(FGPUSpriteVertexFactoryShaderParametersPS);

/**
 * GPU Sprite vertex factory vertex declaration.
 */
class FGPUSpriteVertexDeclaration : public FRenderResource
{
public:

	/** The vertex declaration for GPU sprites. */
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;

		/** The stream to read the texture coordinates from. */
		Elements.Add(FVertexElement(0, 0, VET_Float2, 0, sizeof(FVector2f), false));

		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** Global GPU sprite vertex declaration. */
TGlobalResource<FGPUSpriteVertexDeclaration> GGPUSpriteVertexDeclaration;

/**
 * Optional user data passed into each Mesh Batch
 */
struct FGPUSpriteMeshDataUserData  : public FOneFrameResource
{
	int32 SortedOffset = 0;
	FShaderResourceViewRHIRef SortedParticleIndicesSRV;
};

/**
 * Return the vertex elements from the fixed GGPUSpriteVertexDeclaration used by this factory
 */
void FGPUSpriteVertexFactory::GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements) 
{ 
	GGPUSpriteVertexDeclaration.VertexDeclarationRHI->GetInitializer(Elements);
}

/**
 * Constructs render resources for this vertex factory.
 */
void FGPUSpriteVertexFactory::InitRHI()
{
	FVertexStream Stream;

	// No streams should currently exist.
	check(Streams.Num() == 0);

	// Stream 0: Global particle texture coordinate buffer.
	Stream.VertexBuffer = &GParticleTexCoordVertexBuffer;
	Stream.Stride = sizeof(FVector2f);
	Stream.Offset = 0;
	Streams.Add( Stream );

	// Set the declaration.
	SetDeclaration(GGPUSpriteVertexDeclaration.VertexDeclarationRHI);
}

/**
 * Should we cache the material's shadertype on this platform with this vertex factory?
 */
bool FGPUSpriteVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	return (Parameters.MaterialParameters.bIsUsedWithParticleSprites || Parameters.MaterialParameters.bIsSpecialEngineMaterial) && SupportsGPUParticles(Parameters.Platform);
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FGPUSpriteVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FParticleVertexFactoryBase::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("PARTICLES_PER_INSTANCE"), MAX_PARTICLES_PER_INSTANCE);

	// Set a define so we can tell in MaterialTemplate.usf when we are compiling a sprite vertex factory
	OutEnvironment.SetDefine(TEXT("PARTICLE_SPRITE_FACTORY"), TEXT("1"));
}

void FGPUSpriteVertexFactoryShaderParametersVS::GetElementShaderBindings(
	const FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader,
	const EVertexInputStreamType InputStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory,
	const FMeshBatchElement& BatchElement,
	class FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams) const 
{
	FGPUSpriteVertexFactory* GPUVF = (FGPUSpriteVertexFactory*)VertexFactory;
	FRHISamplerState* SamplerStatePoint = TStaticSamplerState<SF_Point>::GetRHI();
	FRHISamplerState* SamplerStateLinear = TStaticSamplerState<SF_Bilinear>::GetRHI();
	ShaderBindings.Add(Shader->GetUniformBufferParameter<FGPUSpriteEmitterUniformParameters>(), GPUVF->EmitterUniformBuffer);
	ShaderBindings.Add(Shader->GetUniformBufferParameter<FGPUSpriteEmitterDynamicUniformParameters>(), GPUVF->EmitterDynamicUniformBuffer);

	const FGPUSpriteMeshDataUserData* UserData = reinterpret_cast<const FGPUSpriteMeshDataUserData*>(BatchElement.UserData);
	if (UserData != nullptr)
	{
		ShaderBindings.Add(ParticleIndices, UserData->SortedParticleIndicesSRV);
		ShaderBindings.Add(ParticleIndicesOffset, UserData->SortedOffset);
	}
	else
	{
		ShaderBindings.Add(ParticleIndices, GPUVF->UnsortedParticleIndicesSRV ? GPUVF->UnsortedParticleIndicesSRV : (FRHIShaderResourceView*)GNullColorVertexBuffer.VertexBufferSRV);
		ShaderBindings.Add(ParticleIndicesOffset, 0);
	}

	ShaderBindings.AddTexture(PositionTexture, PositionTextureSampler, SamplerStatePoint, GPUVF->PositionTextureRHI);
	ShaderBindings.AddTexture(VelocityTexture, VelocityTextureSampler, SamplerStatePoint, GPUVF->VelocityTextureRHI);
	ShaderBindings.AddTexture(AttributesTexture, AttributesTextureSampler, SamplerStatePoint, GPUVF->AttributesTextureRHI);
	ShaderBindings.AddTexture(CurveTexture, CurveTextureSampler, SamplerStateLinear, GParticleCurveTexture.GetCurveTexture());
}

void FGPUSpriteVertexFactoryShaderParametersPS::GetElementShaderBindings(
	const FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader,
	const EVertexInputStreamType InputStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory,
	const FMeshBatchElement& BatchElement,
	class FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams) const 
{
	FGPUSpriteVertexFactory* GPUVF = (FGPUSpriteVertexFactory*)VertexFactory;
	ShaderBindings.Add(Shader->GetUniformBufferParameter<FGPUSpriteEmitterDynamicUniformParameters>(), GPUVF->EmitterDynamicUniformBuffer);
}

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGPUSpriteVertexFactory, SF_Vertex, FGPUSpriteVertexFactoryShaderParametersVS);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGPUSpriteVertexFactory, SF_Pixel, FGPUSpriteVertexFactoryShaderParametersPS);
IMPLEMENT_VERTEX_FACTORY_TYPE(FGPUSpriteVertexFactory,"/Engine/Private/ParticleGPUSpriteVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPSOPrecaching
);

/*-----------------------------------------------------------------------------
	Shaders used for simulation.
-----------------------------------------------------------------------------*/

/**
 * Uniform buffer to hold parameters for particle simulation.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FParticleSimulationParameters,)
	SHADER_PARAMETER(FVector4f, AttributeCurve)
	SHADER_PARAMETER(FVector4f, AttributeCurveScale)
	SHADER_PARAMETER(FVector4f, AttributeCurveBias)
	SHADER_PARAMETER(FVector4f, AttributeScale)
	SHADER_PARAMETER(FVector4f, AttributeBias)
	SHADER_PARAMETER(FVector4f, MiscCurve)
	SHADER_PARAMETER(FVector4f, MiscScale)
	SHADER_PARAMETER(FVector4f, MiscBias)
	SHADER_PARAMETER(FVector3f, Acceleration)
	SHADER_PARAMETER(FVector3f, OrbitOffsetBase)
	SHADER_PARAMETER(FVector3f, OrbitOffsetRange)
	SHADER_PARAMETER(FVector3f, OrbitFrequencyBase)
	SHADER_PARAMETER(FVector3f, OrbitFrequencyRange)
	SHADER_PARAMETER(FVector3f, OrbitPhaseBase)
	SHADER_PARAMETER(FVector3f, OrbitPhaseRange)
	SHADER_PARAMETER(float, CollisionRadiusScale)
	SHADER_PARAMETER(float, CollisionRadiusBias)
	SHADER_PARAMETER(float, CollisionTimeBias)
	SHADER_PARAMETER(float, CollisionRandomSpread)
	SHADER_PARAMETER(float, CollisionRandomDistribution)
	SHADER_PARAMETER(float, OneMinusFriction)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FParticleSimulationParameters, "Simulation");

typedef TUniformBufferRef<FParticleSimulationParameters> FParticleSimulationBufferRef;

/**
 * Per-frame parameters for particle simulation.
 */
struct FParticlePerFrameSimulationParameters
{
	/** Position (XYZ) and squared radius (W) of the point attractor. */
	FVector4f PointAttractor;
	/** Position offset (XYZ) to add to particles and strength of the attractor (W). */
	FVector4f PositionOffsetAndAttractorStrength;
	/** Amount by which to scale bounds for collision purposes. */
	FVector2f LocalToWorldScale;

	/** Amount of time by which to simulate particles in the fix dt pass. */
	float DeltaSecondsInFix;
	/** Nbr of iterations to use in the fix dt pass. */
	int32  NumIterationsInFix;

	/** Amount of time by which to simulate particles in the variable dt pass. */
	float DeltaSecondsInVar;
	/** Nbr of iterations to use in the variable dt pass. */
	int32 NumIterationsInVar;

	/** Amount of time by which to simulate particles. */
	float DeltaSeconds;

	/** LWC tile offset, will be 0,0,0 for localspace emitters. */
	FVector3f LWCTile;

	FParticlePerFrameSimulationParameters()
		: PointAttractor(FVector3f::ZeroVector,0.0f)
		, PositionOffsetAndAttractorStrength(FVector3f::ZeroVector,0.0f)
		, LocalToWorldScale(1.0f, 1.0f)
		, DeltaSecondsInFix(0.0f)
		, NumIterationsInFix(0)
		, DeltaSecondsInVar(0.0f)
		, NumIterationsInVar(0)
		, DeltaSeconds(0.0f)
		, LWCTile(FVector3f::ZeroVector)

	{
	}

	void ResetDeltaSeconds() 
	{
		DeltaSecondsInFix = 0.0f;
		NumIterationsInFix = 0;
		DeltaSecondsInVar = 0.0f;
		NumIterationsInVar = 0;
		DeltaSeconds = 0.0f;
	}

};

/**
 * Per-frame shader parameters for particle simulation.
 */
struct FParticlePerFrameSimulationShaderParameters
{
	DECLARE_INLINE_TYPE_LAYOUT(FParticlePerFrameSimulationShaderParameters, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		PointAttractor.Bind(ParameterMap,TEXT("PointAttractor"));
		PositionOffsetAndAttractorStrength.Bind(ParameterMap,TEXT("PositionOffsetAndAttractorStrength"));
		LocalToWorldScale.Bind(ParameterMap,TEXT("LocalToWorldScale"));
		DeltaSeconds.Bind(ParameterMap,TEXT("DeltaSeconds"));
		NumIterations.Bind(ParameterMap,TEXT("NumIterations"));
		LWCTile.Bind(ParameterMap,TEXT("LWCTile"));
	}

	template <typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef& ShaderRHI, const FParticlePerFrameSimulationParameters& Parameters, bool bUseFixDT) const
	{
		// The offset must only be applied once in the frame, and be stored in the persistent data (not the interpolated one).
		const float FixDeltaSeconds = CVarGPUParticleFixDeltaSeconds.GetValueOnRenderThread();
		const bool bApplyOffset = FixDeltaSeconds <= 0 || bUseFixDT;
		const FVector4f OnlyAttractorStrength = FVector4f(0, 0, 0, Parameters.PositionOffsetAndAttractorStrength.W);

		SetShaderValue(RHICmdList,ShaderRHI,PointAttractor,Parameters.PointAttractor);
		SetShaderValue(RHICmdList,ShaderRHI,PositionOffsetAndAttractorStrength, bApplyOffset ? Parameters.PositionOffsetAndAttractorStrength : OnlyAttractorStrength);
		SetShaderValue(RHICmdList,ShaderRHI,LocalToWorldScale,Parameters.LocalToWorldScale);
		SetShaderValue(RHICmdList,ShaderRHI,DeltaSeconds, bUseFixDT ? Parameters.DeltaSecondsInFix : Parameters.DeltaSecondsInVar);
		SetShaderValue(RHICmdList,ShaderRHI,NumIterations, bUseFixDT ? Parameters.NumIterationsInFix : Parameters.NumIterationsInVar);
		SetShaderValue(RHICmdList,ShaderRHI,LWCTile, Parameters.LWCTile);
	}

	LAYOUT_FIELD(FShaderParameter, PointAttractor);
	LAYOUT_FIELD(FShaderParameter, PositionOffsetAndAttractorStrength);
	LAYOUT_FIELD(FShaderParameter, LocalToWorldScale);
	LAYOUT_FIELD(FShaderParameter, DeltaSeconds);
	LAYOUT_FIELD(FShaderParameter, NumIterations);
	LAYOUT_FIELD(FShaderParameter, LWCTile);
	
};

FArchive& operator<<(FArchive& Ar, FParticlePerFrameSimulationShaderParameters& PerFrameParameters)
{
	Ar << PerFrameParameters.PointAttractor;
	Ar << PerFrameParameters.PositionOffsetAndAttractorStrength;
	Ar << PerFrameParameters.LocalToWorldScale;
	Ar << PerFrameParameters.DeltaSeconds;
	Ar << PerFrameParameters.NumIterations;
	Ar << PerFrameParameters.LWCTile;
	return Ar;
}

/**
 * Uniform buffer to hold parameters for vector fields sampled during particle
 * simulation.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FVectorFieldUniformParameters,)
	SHADER_PARAMETER( int32, Count )
	SHADER_PARAMETER_ARRAY( FMatrix44f, WorldToVolume, [MAX_VECTOR_FIELDS] )
	SHADER_PARAMETER_ARRAY( FMatrix44f, VolumeToWorld, [MAX_VECTOR_FIELDS] )
	SHADER_PARAMETER_ARRAY( FVector4f, IntensityAndTightness, [MAX_VECTOR_FIELDS] )
	SHADER_PARAMETER_ARRAY( FVector4f, VolumeSize, [MAX_VECTOR_FIELDS] )
	SHADER_PARAMETER_ARRAY( FVector4f, TilingAxes, [MAX_VECTOR_FIELDS] )
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FVectorFieldUniformParameters, "VectorFields");

typedef TUniformBufferRef<FVectorFieldUniformParameters> FVectorFieldUniformBufferRef;

/**
 * Vertex shader for drawing particle tiles on the GPU.
 */
class FParticleTileVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleTileVS,Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SupportsGPUParticles(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		OutEnvironment.SetDefine(TEXT("TILES_PER_INSTANCE"), TILES_PER_INSTANCE);
	}

	/** Default constructor. */
	FParticleTileVS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleTileVS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		TileOffsets.Bind(Initializer.ParameterMap, TEXT("TileOffsets"));
		TileSizeX.Bind(Initializer.ParameterMap, TEXT("TileSizeX"));
		TileSizeY.Bind(Initializer.ParameterMap, TEXT("TileSizeY"));
	}
	/** Set parameters. */
	void SetParameters(FRHICommandList& RHICmdList, FParticleShaderParamRef TileOffsetsRef)
	{
		FRHIVertexShader* VertexShaderRHI = RHICmdList.GetBoundVertexShader();
		if (TileOffsets.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(VertexShaderRHI, TileOffsets.GetBaseIndex(), TileOffsetsRef);
		}
		SetShaderValue(RHICmdList, VertexShaderRHI, TileSizeX, (float)GParticleSimulationTileSize / (float)GParticleSimulationTextureSizeX);
		SetShaderValue(RHICmdList, VertexShaderRHI, TileSizeY, (float)GParticleSimulationTileSize / (float)GParticleSimulationTextureSizeY);
	}

private:
	/** Buffer from which to read tile offsets. */
	LAYOUT_FIELD(FShaderResourceParameter, TileOffsets);
	LAYOUT_FIELD(FShaderParameter, TileSizeX);
	LAYOUT_FIELD(FShaderParameter, TileSizeY);
};

/**
 * Pixel shader for simulating particles on the GPU.
 */
template <EParticleCollisionShaderMode CollisionMode>
class TParticleSimulationPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TParticleSimulationPS,Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SupportsGPUParticles(Parameters.Platform) && IsParticleCollisionModeSupported(Parameters.Platform, CollisionMode);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PARTICLE_SIMULATION_PIXELSHADER"), 1);
		OutEnvironment.SetDefine(TEXT("MAX_VECTOR_FIELDS"), MAX_VECTOR_FIELDS);
		OutEnvironment.SetDefine(TEXT("DEPTH_BUFFER_COLLISION"), CollisionMode == PCM_DepthBuffer);
		OutEnvironment.SetDefine(TEXT("DISTANCE_FIELD_COLLISION"), CollisionMode == PCM_DistanceField);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);
	}

	/** Default constructor. */
	TParticleSimulationPS()
	{
	}

	/** Initialization constructor. */
	explicit TParticleSimulationPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PositionTexture.Bind(Initializer.ParameterMap, TEXT("PositionTexture"));
		PositionTextureSampler.Bind(Initializer.ParameterMap, TEXT("PositionTextureSampler"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
		VelocityTextureSampler.Bind(Initializer.ParameterMap, TEXT("VelocityTextureSampler"));
		AttributesTexture.Bind(Initializer.ParameterMap, TEXT("AttributesTexture"));
		AttributesTextureSampler.Bind(Initializer.ParameterMap, TEXT("AttributesTextureSampler"));
		RenderAttributesTexture.Bind(Initializer.ParameterMap, TEXT("RenderAttributesTexture"));
		RenderAttributesTextureSampler.Bind(Initializer.ParameterMap, TEXT("RenderAttributesTextureSampler"));
		CurveTexture.Bind(Initializer.ParameterMap, TEXT("CurveTexture"));
		CurveTextureSampler.Bind(Initializer.ParameterMap, TEXT("CurveTextureSampler"));
		for (int32 i = 0; i < MAX_VECTOR_FIELDS; ++i)
		{
			VectorFieldTextures[i].Bind(Initializer.ParameterMap, *FString::Printf(TEXT("VectorFieldTextures%d"), i));
			VectorFieldTexturesSamplers[i].Bind(Initializer.ParameterMap, *FString::Printf(TEXT("VectorFieldTexturesSampler%d"), i));
		}
		CollisionDepthBounds.Bind(Initializer.ParameterMap,TEXT("CollisionDepthBounds"));
		PerFrameParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);
	}

	/**
	 * Set parameters for this shader.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FParticleStateTextures& TextureResources,
		const FParticleAttributesTexture& InAttributesTexture,
		const FParticleAttributesTexture& InRenderAttributesTexture,
		FRHIUniformBuffer* ViewUniformBuffer,
		ERHIFeatureLevel::Type FeatureLevel,
		const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData
		)
	{
		FRHIPixelShader* PixelShaderRHI = RHICmdList.GetBoundPixelShader();
		FRHISamplerState* SamplerStatePoint = TStaticSamplerState<SF_Point>::GetRHI();
		FRHISamplerState* SamplerStateLinear = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		SetTextureParameter(RHICmdList, PixelShaderRHI, PositionTexture, PositionTextureSampler, SamplerStatePoint, TextureResources.PositionTextureRHI);
		SetTextureParameter(RHICmdList, PixelShaderRHI, VelocityTexture, VelocityTextureSampler, SamplerStatePoint, TextureResources.VelocityTextureRHI);
		SetTextureParameter(RHICmdList, PixelShaderRHI, AttributesTexture, AttributesTextureSampler, SamplerStatePoint, InAttributesTexture.TextureRHI);
		SetTextureParameter(RHICmdList, PixelShaderRHI, CurveTexture, CurveTextureSampler, SamplerStateLinear, GParticleCurveTexture.GetCurveTexture());

		if (CollisionMode == PCM_DepthBuffer)
		{
			check(ViewUniformBuffer != NULL);
			FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, PixelShaderRHI, ViewUniformBuffer);

			SetTextureParameter(
				RHICmdList, 
				PixelShaderRHI,
				RenderAttributesTexture,
				RenderAttributesTextureSampler,
				SamplerStatePoint,
				InRenderAttributesTexture.TextureRHI
				);
			SetShaderValue(RHICmdList, PixelShaderRHI, CollisionDepthBounds, FXConsoleVariables::GPUCollisionDepthBounds);
		}
		else if (CollisionMode == PCM_DistanceField)
		{
			check(ViewUniformBuffer != NULL);
			FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, PixelShaderRHI, ViewUniformBuffer);

			GlobalDistanceFieldParameters.Set(RHICmdList, PixelShaderRHI, *GlobalDistanceFieldParameterData);

			SetTextureParameter(
				RHICmdList, 
				PixelShaderRHI,
				RenderAttributesTexture,
				RenderAttributesTextureSampler,
				SamplerStatePoint,
				InRenderAttributesTexture.TextureRHI
				);
		}
	}

	/**
	 * Set parameters for the vector fields sampled by this shader.
	 * @param VectorFieldParameters -Parameters needed to sample local vector fields.
	 */
	void SetVectorFieldParameters(FRHICommandList& RHICmdList, const FVectorFieldUniformBufferRef& UniformBuffer, FRHITexture3D* const* VolumeTexturesRHI)
	{
		FRHIPixelShader* PixelShaderRHI = RHICmdList.GetBoundPixelShader();
			SetUniformBufferParameter(RHICmdList, PixelShaderRHI, GetUniformBufferParameter<FVectorFieldUniformParameters>(), UniformBuffer);
		
			FRHISamplerState* SamplerStateLinear = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

		for (int32 i = 0; i < MAX_VECTOR_FIELDS; ++i)
		{
			SetSamplerParameter(RHICmdList, PixelShaderRHI, VectorFieldTexturesSamplers[i], SamplerStateLinear);
			SetTextureParameter(RHICmdList, PixelShaderRHI, VectorFieldTextures[i], VolumeTexturesRHI[i]);
		}
	}

	/**
	 * Set per-instance parameters for this shader.
	 */
	void SetInstanceParameters(FRHICommandList& RHICmdList, FRHIUniformBuffer* UniformBuffer, const FParticlePerFrameSimulationParameters& InPerFrameParameters, bool bUseFixDT)
	{
		FRHIPixelShader* PixelShaderRHI = RHICmdList.GetBoundPixelShader();
		SetUniformBufferParameter(RHICmdList, PixelShaderRHI, GetUniformBufferParameter<FParticleSimulationParameters>(), UniformBuffer);
		PerFrameParameters.Set(RHICmdList, PixelShaderRHI, InPerFrameParameters, bUseFixDT);
	}

	/**
	 * Unbinds buffers that may need to be bound as UAVs.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FRHIPixelShader* PixelShaderRHI = RHICmdList.GetBoundPixelShader();
		FRHIShaderResourceView* NullSRV = nullptr;
		for (int32 i = 0; i < MAX_VECTOR_FIELDS; ++i)
		{
			if (VectorFieldTextures[i].IsBound())
			{
				RHICmdList.SetShaderResourceViewParameter(PixelShaderRHI, VectorFieldTextures[i].GetBaseIndex(), NullSRV);
			}
		}
	}

private:
	/** The position texture parameter. */
	LAYOUT_FIELD(FShaderResourceParameter, PositionTexture);
	LAYOUT_FIELD(FShaderResourceParameter, PositionTextureSampler);
	/** The velocity texture parameter. */
	LAYOUT_FIELD(FShaderResourceParameter, VelocityTexture);
	LAYOUT_FIELD(FShaderResourceParameter, VelocityTextureSampler);
	/** The simulation attributes texture parameter. */
	LAYOUT_FIELD(FShaderResourceParameter, AttributesTexture);
	LAYOUT_FIELD(FShaderResourceParameter, AttributesTextureSampler);
	/** The render attributes texture parameter. */
	LAYOUT_FIELD(FShaderResourceParameter, RenderAttributesTexture);
	LAYOUT_FIELD(FShaderResourceParameter, RenderAttributesTextureSampler);
	/** The curve texture parameter. */
	LAYOUT_FIELD(FShaderResourceParameter, CurveTexture);
	LAYOUT_FIELD(FShaderResourceParameter, CurveTextureSampler);
	/** Vector fields. */
	LAYOUT_ARRAY(FShaderResourceParameter, VectorFieldTextures, MAX_VECTOR_FIELDS);
	LAYOUT_ARRAY(FShaderResourceParameter, VectorFieldTexturesSamplers, MAX_VECTOR_FIELDS);
	/** Per frame simulation parameters. */
	LAYOUT_FIELD(FParticlePerFrameSimulationShaderParameters, PerFrameParameters);
	/** Collision depth bounds. */
	LAYOUT_FIELD(FShaderParameter, CollisionDepthBounds);
	LAYOUT_FIELD(FGlobalDistanceFieldParameters, GlobalDistanceFieldParameters);
};

/**
 * Pixel shader for clearing particle simulation data on the GPU.
 */
class FParticleSimulationClearPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleSimulationClearPS,Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SupportsGPUParticles(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("PARTICLE_CLEAR_PIXELSHADER"), 1 );
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);
	}

	/** Default constructor. */
	FParticleSimulationClearPS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleSimulationClearPS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
	}
};

/** Implementation for all shaders used for simulation. */
IMPLEMENT_SHADER_TYPE(,FParticleTileVS,TEXT("/Engine/Private/ParticleSimulationShader.usf"),TEXT("VertexMain"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>,TParticleSimulationPS<PCM_None>,TEXT("/Engine/Private/ParticleSimulationShader.usf"),TEXT("PixelMain"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TParticleSimulationPS<PCM_DepthBuffer>,TEXT("/Engine/Private/ParticleSimulationShader.usf"),TEXT("PixelMain"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TParticleSimulationPS<PCM_DistanceField>,TEXT("/Engine/Private/ParticleSimulationShader.usf"),TEXT("PixelMain"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FParticleSimulationClearPS,TEXT("/Engine/Private/ParticleSimulationShader.usf"),TEXT("PixelMain"),SF_Pixel);

/**
 * Vertex declaration for drawing particle tiles.
 */
class FParticleTileVertexDeclaration : public FRenderResource
{
public:

	/** The vertex declaration. */
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		// TexCoord.
		Elements.Add(FVertexElement(0, 0, VET_Float2, 0, sizeof(FVector2f), /*bUseInstanceIndex=*/ false));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration( Elements );
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** Global vertex declaration resource for particle sim visualization. */
TGlobalResource<FParticleTileVertexDeclaration> GParticleTileVertexDeclaration;

/**
 * Vertex declaration for drawing particle tile with per-instance data (used on mobile)
 */
class FInstancedParticleTileVertexDeclaration : public FRenderResource
{
public:

	/** The vertex declaration. */
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		// TexCoord.
		Elements.Add(FVertexElement(0, 0, VET_Float2, 0, sizeof(FVector2f), /*bUseInstanceIndex=*/ false));
		// TileOffsets
		Elements.Add(FVertexElement(1, 0, VET_Float2, 1, sizeof(FVector2f), /*bUseInstanceIndex=*/ true));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration( Elements );
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FInstancedParticleTileVertexDeclaration> GInstancedParticleTileVertexDeclaration;

static FRHIVertexDeclaration* GetParticleTileVertexDeclaration(ERHIFeatureLevel::Type FeatureLevel)
{
	if (FeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
		return GInstancedParticleTileVertexDeclaration.VertexDeclarationRHI;
	}
	else
	{
		return GParticleTileVertexDeclaration.VertexDeclarationRHI;
	}
}

/**
 * Computes the aligned tile count.
 */
FORCEINLINE int32 ComputeAlignedTileCount(int32 TileCount)
{
	return (TileCount + (TILES_PER_INSTANCE-1)) & (~(TILES_PER_INSTANCE-1));
}

/**
 * Builds a vertex buffer containing the offsets for a set of tiles.
 * @param TileOffsetsRef - The vertex buffer to fill. Must be at least TileCount * sizeof(FVector4f) in size.
 * @param Tiles - The tiles which will be drawn.
 * @param TileCount - The number of tiles in the array.
 * @param AlignedTileCount - The number of tiles to create in buffer for aligned rendering.
 */
static void BuildTileVertexBuffer(FParticleBufferParamRef TileOffsetsRef, const uint32* Tiles, int32 TileCount, int32 AlignedTileCount)
{
	FVector2f* TileOffset = (FVector2f*)RHILockBuffer( TileOffsetsRef, 0, AlignedTileCount * sizeof(FVector2f), RLM_WriteOnly );
	for ( int32 Index = 0; Index < TileCount; ++Index )
	{
		const uint32 TileIndex = Tiles[Index];
		TileOffset[Index].X = FMath::Fractional( (float)TileIndex / (float)GParticleSimulationTileCountX );
		TileOffset[Index].Y = FMath::Fractional( FMath::TruncToFloat( (float)TileIndex / (float)GParticleSimulationTileCountX ) / (float)GParticleSimulationTileCountY );
	}
	for ( int32 Index = TileCount; Index < AlignedTileCount; ++Index )
	{
		TileOffset[Index].X = 100.0f;
		TileOffset[Index].Y = 100.0f;
	}
	RHIUnlockBuffer( TileOffsetsRef );
}

/**
 * Issues a draw call for an aligned set of tiles.
 * @param TileCount - The number of tiles to be drawn.
 */
static void DrawAlignedParticleTiles(FRHICommandList& RHICmdList, int32 TileCount)
{
	check((TileCount & (TILES_PER_INSTANCE-1)) == 0);

	// Stream 0: TexCoord.
	RHICmdList.SetStreamSource(
		0,
		GParticleTexCoordVertexBuffer.VertexBufferRHI,
		/*Offset=*/ 0
		);

	// Draw tiles.
	RHICmdList.DrawIndexedPrimitive(
		GParticleIndexBuffer.IndexBufferRHI,
		/*BaseVertexIndex=*/ 0,
		/*MinIndex=*/ 0,
		/*NumVertices=*/ 4,
		/*StartIndex=*/ 0,
		/*NumPrimitives=*/ 2 * TILES_PER_INSTANCE,
		/*NumInstances=*/ TileCount / TILES_PER_INSTANCE
		);
}

static void DrawParticleTiles(FRHICommandList& RHICmdList, FParticleBufferParamRef TileOffsetsRef, int32 TileCount)
{
	// Stream 0: TexCoord.
	RHICmdList.SetStreamSource(
		0,
		GParticleTexCoordVertexBuffer.VertexBufferRHI,
		/*Offset=*/ 0
		);
	
	// Stream 1: TileOffsets
	RHICmdList.SetStreamSource(
		1,
		TileOffsetsRef,
		/*Offset=*/ 0
		);
	
	// Draw tiles.
	RHICmdList.DrawIndexedPrimitive(
		GParticleIndexBuffer.IndexBufferRHI,
		/*BaseVertexIndex=*/0,
		/*MinIndex=*/ 0,
		/*NumVertices=*/ 4,
		/*StartIndex=*/ 0,
		/*NumPrimitives=*/ 2,
		/*NumInstances=*/ TileCount
		);
}

/**
 * The data needed to simulate a set of particle tiles on the GPU.
 */
struct FSimulationCommandGPU
{
	/** Buffer containing the offsets of each tile. */
	FParticleShaderParamRef TileOffsetsShaderRef;
	FParticleBufferParamRef TileOffsetsBufferRef;
	/** Uniform buffer containing simulation parameters. */
	FRHIUniformBuffer* UniformBuffer;
	/** Uniform buffer containing per-frame simulation parameters. */
	FParticlePerFrameSimulationParameters PerFrameParameters;
	/** Parameters to sample the local vector field for this simulation. */
	FVectorFieldUniformBufferRef VectorFieldsUniformBuffer;
	/** Vector field volume textures for this simulation. */
	FRHITexture3D* VectorFieldTexturesRHI[MAX_VECTOR_FIELDS];
	/** The number of tiles to simulate. */
	int32 UnalignedTileCount;

	/** Initialization constructor. */
	FSimulationCommandGPU(FParticleShaderParamRef InTileOffsetsShaderRef, FParticleBufferParamRef InTileOffsetsBufferRef, FRHIUniformBuffer* InUniformBuffer, const FParticlePerFrameSimulationParameters& InPerFrameParameters, FVectorFieldUniformBufferRef& InVectorFieldsUniformBuffer, int32 InTileCount)
		: TileOffsetsShaderRef(InTileOffsetsShaderRef)
		, TileOffsetsBufferRef(InTileOffsetsBufferRef)
		, UniformBuffer(InUniformBuffer)
		, PerFrameParameters(InPerFrameParameters)
		, VectorFieldsUniformBuffer(InVectorFieldsUniformBuffer)
		, UnalignedTileCount(InTileCount)
	{
		FRHITexture3D* BlackVolumeTextureRHI = (FRHITexture3D*)(FRHITexture*)GBlackVolumeTexture->TextureRHI;
		for (int32 i = 0; i < MAX_VECTOR_FIELDS; ++i)
		{
			VectorFieldTexturesRHI[i] = BlackVolumeTextureRHI;
		}
	}
};

/**
 * Executes each command invoking the simulation pixel shader for each particle.
 * calling with empty SimulationCommands is a waste of performance
 * @param SimulationCommands The list of simulation commands to execute.
 * @param TextureResources	The resources from which the current state can be read.
 * @param AttributeTexture	The texture from which particle simulation attributes can be read.
 * @param CollisionView		The view to use for collision, if any.
 */
template <EParticleCollisionShaderMode CollisionMode>
void ExecuteSimulationCommands(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	ERHIFeatureLevel::Type FeatureLevel,
	const TArray<FSimulationCommandGPU>& SimulationCommands,
	FParticleSimulationResources* ParticleSimulationResources,
	FRHIUniformBuffer* ViewUniformBuffer,
	const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData,
	FRHIUniformBuffer* SceneTexturesUniformBuffer,
	bool bUseFixDT)
{
	if (!CVarSimulateGPUParticles.GetValueOnAnyThread())
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_GPUParticlesSimulationCommands);
	SCOPED_DRAW_EVENT(RHICmdList, ParticleSimulation);
	SCOPED_GPU_STAT(RHICmdList, ParticleSimulation);

	FUniformBufferStaticBindings StaticUniformBuffers;
	if (SceneTexturesUniformBuffer)
	{
		StaticUniformBuffers.AddUniformBuffer(SceneTexturesUniformBuffer);
	}
	SCOPED_UNIFORM_BUFFER_STATIC_BINDINGS(RHICmdList, StaticUniformBuffers);

	const float FixDeltaSeconds = CVarGPUParticleFixDeltaSeconds.GetValueOnRenderThread();
	const FParticleStateTextures& TextureResources = (FixDeltaSeconds <= 0 || bUseFixDT) ? ParticleSimulationResources->GetPreviousStateTextures() : ParticleSimulationResources->GetCurrentStateTextures();
	const FParticleAttributesTexture& AttributeTexture = ParticleSimulationResources->SimulationAttributesTexture;
	const FParticleAttributesTexture& RenderAttributeTexture = ParticleSimulationResources->RenderAttributesTexture;

	// Grab shaders.
	TShaderMapRef<FParticleTileVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
	TShaderMapRef<TParticleSimulationPS<CollisionMode> > PixelShader(GetGlobalShaderMap(FeatureLevel));

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetParticleTileVertexDeclaration(FeatureLevel);
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	PixelShader->SetParameters(RHICmdList, TextureResources, AttributeTexture, RenderAttributeTexture, ViewUniformBuffer, FeatureLevel, GlobalDistanceFieldParameterData);

	// Draw tiles to perform the simulation step.
	const int32 CommandCount = SimulationCommands.Num();
	for (int32 CommandIndex = 0; CommandIndex < CommandCount; ++CommandIndex)
	{
		const FSimulationCommandGPU& Command = SimulationCommands[CommandIndex];
		VertexShader->SetParameters(RHICmdList, Command.TileOffsetsShaderRef);
		PixelShader->SetInstanceParameters(RHICmdList, Command.UniformBuffer, Command.PerFrameParameters, bUseFixDT);
		PixelShader->SetVectorFieldParameters(
			RHICmdList, 
			Command.VectorFieldsUniformBuffer,
			Command.VectorFieldTexturesRHI
		);
		
		if (FeatureLevel <= ERHIFeatureLevel::ES3_1)
		{
			DrawParticleTiles(RHICmdList, Command.TileOffsetsBufferRef, Command.UnalignedTileCount);
		}
		else
		{
			int32 AlignedTileCount = ComputeAlignedTileCount(Command.UnalignedTileCount);
			DrawAlignedParticleTiles(RHICmdList, AlignedTileCount);
		}
	}

	// Unbind input buffers.
	PixelShader->UnbindBuffers(RHICmdList);
}


void ExecuteSimulationCommands(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	ERHIFeatureLevel::Type FeatureLevel,
	const TArray<FSimulationCommandGPU>& SimulationCommands,
	FParticleSimulationResources* ParticleSimulationResources,
	FRHIUniformBuffer* ViewUniformBuffer,
	const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData,
	FRHIUniformBuffer* SceneTexturesUniformBuffer,
	EParticleSimulatePhase::Type Phase,
	bool bUseFixDT)
{
	if (Phase == EParticleSimulatePhase::CollisionDepthBuffer && ViewUniformBuffer)
	{
		ExecuteSimulationCommands<PCM_DepthBuffer>(
			RHICmdList,
			GraphicsPSOInit,
			FeatureLevel,
			SimulationCommands,
			ParticleSimulationResources,
			ViewUniformBuffer,
			GlobalDistanceFieldParameterData,
			SceneTexturesUniformBuffer,
			bUseFixDT);
	}
	else if (Phase == EParticleSimulatePhase::CollisionDistanceField && GlobalDistanceFieldParameterData)
	{
		ExecuteSimulationCommands<PCM_DistanceField>(
			RHICmdList,
			GraphicsPSOInit,
			FeatureLevel,
			SimulationCommands,
			ParticleSimulationResources,
			ViewUniformBuffer,
			GlobalDistanceFieldParameterData,
			SceneTexturesUniformBuffer,
			bUseFixDT);
	}
	else
	{
		ExecuteSimulationCommands<PCM_None>(
			RHICmdList,
			GraphicsPSOInit,
			FeatureLevel,
			SimulationCommands,
			ParticleSimulationResources,
			NULL,
			GlobalDistanceFieldParameterData,
			SceneTexturesUniformBuffer,
			bUseFixDT);
	}
}

/**
 * Invokes the clear simulation shader for each particle in each tile.
 * @param Tiles - The list of tiles to clear.
 */
void ClearTiles(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type FeatureLevel, const TArray<uint32>& Tiles)
{
	if (!CVarSimulateGPUParticles.GetValueOnAnyThread())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, ClearTiles);
	SCOPED_GPU_STAT(RHICmdList, ParticleSimulation);

	FParticleShaderParamRef ShaderParam = GParticleScratchVertexBuffer.GetShaderParam();
	check(ShaderParam);
	FParticleBufferParamRef BufferParam = GParticleScratchVertexBuffer.GetBufferParam();
	check(BufferParam);
	
	// Grab shaders.
	TShaderMapRef<FParticleTileVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
	TShaderMapRef<FParticleSimulationClearPS> PixelShader(GetGlobalShaderMap(FeatureLevel));
	
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetParticleTileVertexDeclaration(FeatureLevel);
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
	
	const int32 MaxTilesPerDrawCallUnaligned = GParticleScratchVertexBufferSize / sizeof(FVector2f);
	const int32 MaxTilesPerDrawCall = (FeatureLevel <= ERHIFeatureLevel::ES3_1 ? MaxTilesPerDrawCallUnaligned : MaxTilesPerDrawCallUnaligned & (~(TILES_PER_INSTANCE-1)));
	int32 TileCount = Tiles.Num();
	int32 FirstTile = 0;

	while (TileCount > 0)
	{
		// Copy new particles in to the vertex buffer.
		const int32 TilesThisDrawCall = FMath::Min<int32>( TileCount, MaxTilesPerDrawCall );
		const uint32* TilesPtr = Tiles.GetData() + FirstTile;
				
		if (FeatureLevel <= ERHIFeatureLevel::ES3_1)
		{
			BuildTileVertexBuffer(BufferParam, TilesPtr, TilesThisDrawCall, TilesThisDrawCall);
			VertexShader->SetParameters(RHICmdList, ShaderParam);
			DrawParticleTiles(RHICmdList, BufferParam, TilesThisDrawCall);
		}
		else
		{
			const int32 AlignedTilesThisDrawCall = ComputeAlignedTileCount(TilesThisDrawCall);
			BuildTileVertexBuffer(BufferParam, TilesPtr, TilesThisDrawCall, AlignedTilesThisDrawCall);
			VertexShader->SetParameters(RHICmdList, ShaderParam);
			DrawAlignedParticleTiles(RHICmdList, AlignedTilesThisDrawCall);
		}
		
		TileCount -= TilesThisDrawCall;
		FirstTile += TilesThisDrawCall;
	}
}

/**
 * Uniform buffer to hold parameters for particle simulation.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FParticleInjectionParameters, )
	SHADER_PARAMETER( FVector2f, PixelScale )
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FParticleInjectionParameters, "ParticleInjection");

typedef TUniformBufferRef<FParticleInjectionParameters> FParticleInjectionBufferRef;

/**
 * Vertex shader for simulating particles on the GPU.
 */
class FParticleInjectionVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleInjectionVS,Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SupportsGPUParticles(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}

	/** Default constructor. */
	FParticleInjectionVS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleInjectionVS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
	}

	/**
	 * Sets parameters for particle injection.
	 */
	void SetParameters(FRHICommandList& RHICmdList)
	{
		FParticleInjectionParameters Parameters;
		Parameters.PixelScale.X = 1.0f / GParticleSimulationTextureSizeX;
		Parameters.PixelScale.Y = 1.0f / GParticleSimulationTextureSizeY;
		FParticleInjectionBufferRef UniformBuffer = FParticleInjectionBufferRef::CreateUniformBufferImmediate( Parameters, UniformBuffer_SingleDraw );
		FRHIVertexShader* VertexShader = RHICmdList.GetBoundVertexShader();
		SetUniformBufferParameter(RHICmdList, VertexShader, GetUniformBufferParameter<FParticleInjectionParameters>(), UniformBuffer );
	}
};

/**
 * Pixel shader for simulating particles on the GPU.
 */
template <bool StaticPropertiesOnly>
class TParticleInjectionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TParticleInjectionPS,Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SupportsGPUParticles(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("STATIC_PROPERTIES_ONLY"), StaticPropertiesOnly);

		OutEnvironment.SetRenderTargetOutputFormat(0, StaticPropertiesOnly ? PF_A8R8G8B8 : PF_A32B32G32R32F);
	}

	/** Default constructor. */
	TParticleInjectionPS()
	{
	}

	/** Initialization constructor. */
	explicit TParticleInjectionPS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
	}
};

/** Implementation for all shaders used for particle injection. */
IMPLEMENT_SHADER_TYPE(,FParticleInjectionVS,TEXT("/Engine/Private/ParticleInjectionShader.usf"),TEXT("VertexMain"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>, TParticleInjectionPS<false>, TEXT("/Engine/Private/ParticleInjectionShader.usf"), TEXT("PixelMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TParticleInjectionPS<true>, TEXT("/Engine/Private/ParticleInjectionShader.usf"), TEXT("PixelMain"), SF_Pixel);


/**
 * Vertex declaration for injecting particles.
 */
class FParticleInjectionVertexDeclaration : public FRenderResource
{
public:

	/** The vertex declaration. */
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;

		// Stream 0.
		{
			int32 Offset = 0;
			uint16 Stride = sizeof(FNewParticle);
			// InitialPosition.
			Elements.Add(FVertexElement(0, Offset, VET_Float4, 0, Stride, /*bUseInstanceIndex=*/ true));
			Offset += sizeof(FVector4f);
			// InitialVelocity.
			Elements.Add(FVertexElement(0, Offset, VET_Float4, 1, Stride, /*bUseInstanceIndex=*/ true));
			Offset += sizeof(FVector4f);
			// RenderAttributes.
			Elements.Add(FVertexElement(0, Offset, VET_Float4, 2, Stride, /*bUseInstanceIndex=*/ true));
			Offset += sizeof(FVector4f);
			// SimulationAttributes.
			Elements.Add(FVertexElement(0, Offset, VET_Float4, 3, Stride, /*bUseInstanceIndex=*/ true));
			Offset += sizeof(FVector4f);
			// ParticleIndex.
			Elements.Add(FVertexElement(0, Offset, VET_Float2, 4, Stride, /*bUseInstanceIndex=*/ true));
			Offset += sizeof(FVector2f);
		}

		// Stream 1.
		{
			int32 Offset = 0;
			// TexCoord.
			Elements.Add(FVertexElement(1, Offset, VET_Float2, 5, sizeof(FVector2f), /*bUseInstanceIndex=*/ false));
			Offset += sizeof(FVector2f);
		}

		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration( Elements );
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** The global particle injection vertex declaration. */
TGlobalResource<FParticleInjectionVertexDeclaration> GParticleInjectionVertexDeclaration;

/**
 * Injects new particles in to the GPU simulation.
 * @param NewParticles - A list of particles to inject in to the simulation.
 */
template<bool StaticPropertiesOnly>
void InjectNewParticles(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit,  ERHIFeatureLevel::Type FeatureLevel, const TArray<FNewParticle>& NewParticles)
{
	if (GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed) || !CVarSimulateGPUParticles.GetValueOnAnyThread())
	{
		return;
	}

	const int32 MaxParticlesPerDrawCall = GParticleScratchVertexBufferSize / sizeof(FNewParticle);
	FRHIBuffer* ScratchVertexBufferRHI = GParticleScratchVertexBuffer.VertexBufferRHI;
	int32 ParticleCount = NewParticles.Num();
	int32 FirstParticle = 0;

	
	while ( ParticleCount > 0 )
	{
		// Copy new particles in to the vertex buffer.
		const int32 ParticlesThisDrawCall = FMath::Min<int32>( ParticleCount, MaxParticlesPerDrawCall );
		const void* Src = NewParticles.GetData() + FirstParticle;
		void* Dest = RHILockBuffer( ScratchVertexBufferRHI, 0, ParticlesThisDrawCall * sizeof(FNewParticle), RLM_WriteOnly );
		FMemory::Memcpy( Dest, Src, ParticlesThisDrawCall * sizeof(FNewParticle) );
		RHIUnlockBuffer( ScratchVertexBufferRHI );
		ParticleCount -= ParticlesThisDrawCall;
		FirstParticle += ParticlesThisDrawCall;

		// Grab shaders.
		TShaderMapRef<FParticleInjectionVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
		TShaderMapRef<TParticleInjectionPS<StaticPropertiesOnly> > PixelShader(GetGlobalShaderMap(FeatureLevel));

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GParticleInjectionVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		
		VertexShader->SetParameters(RHICmdList);

		// Stream 0: New particles.
		RHICmdList.SetStreamSource(
			0,
			ScratchVertexBufferRHI,
			/*Offset=*/ 0
			);

		// Stream 1: TexCoord.
		RHICmdList.SetStreamSource(
			1,
			GParticleTexCoordVertexBuffer.VertexBufferRHI,
			/*Offset=*/ 0
			);

		// Inject particles.
		RHICmdList.DrawIndexedPrimitive(
			GParticleIndexBuffer.IndexBufferRHI,
			/*BaseVertexIndex=*/ 0,
			/*MinIndex=*/ 0,
			/*NumVertices=*/ 4,
			/*StartIndex=*/ 0,
			/*NumPrimitives=*/ 2,
			/*NumInstances=*/ ParticlesThisDrawCall
			);
	}
}

/*-----------------------------------------------------------------------------
	Shaders used for visualizing the state of particle simulation on the GPU.
-----------------------------------------------------------------------------*/

/**
 * Uniform buffer to hold parameters for visualizing particle simulation.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FParticleSimVisualizeParameters, )
	SHADER_PARAMETER( FVector4f, ScaleBias )
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FParticleSimVisualizeParameters,"PSV");

typedef TUniformBufferRef<FParticleSimVisualizeParameters> FParticleSimVisualizeBufferRef;

/**
 * Vertex shader for visualizing particle simulation.
 */
class FParticleSimVisualizeVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleSimVisualizeVS,Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SupportsGPUParticles(Parameters.Platform);
	}

	/** Default constructor. */
	FParticleSimVisualizeVS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleSimVisualizeVS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
	}

	/**
	 * Set parameters for this shader.
	 */
	void SetParameters(FRHICommandList& RHICmdList, const FParticleSimVisualizeBufferRef& UniformBuffer )
	{
		FRHIVertexShader* VertexShader = RHICmdList.GetBoundVertexShader();
		SetUniformBufferParameter(RHICmdList, VertexShader, GetUniformBufferParameter<FParticleSimVisualizeParameters>(), UniformBuffer );
	}
};

/**
 * Pixel shader for visualizing particle simulation.
 */
class FParticleSimVisualizePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleSimVisualizePS,Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SupportsGPUParticles(Parameters.Platform);
	}

	/** Default constructor. */
	FParticleSimVisualizePS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleSimVisualizePS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		VisualizationMode.Bind( Initializer.ParameterMap, TEXT("VisualizationMode") );
		PositionTexture.Bind( Initializer.ParameterMap, TEXT("PositionTexture") );
		PositionTextureSampler.Bind( Initializer.ParameterMap, TEXT("PositionTextureSampler") );
		CurveTexture.Bind( Initializer.ParameterMap, TEXT("CurveTexture") );
		CurveTextureSampler.Bind( Initializer.ParameterMap, TEXT("CurveTextureSampler") );
	}

	/**
	 * Set parameters for this shader.
	 */
	void SetParameters(FRHICommandList& RHICmdList, int32 InVisualizationMode, FRHITexture2D* PositionTextureRHI, FRHITexture2D* CurveTextureRHI )
	{
		FRHIPixelShader* PixelShader = RHICmdList.GetBoundPixelShader();
		SetShaderValue(RHICmdList, PixelShader, VisualizationMode, InVisualizationMode );
		FRHISamplerState* SamplerStatePoint = TStaticSamplerState<SF_Point>::GetRHI();
		SetTextureParameter(RHICmdList, PixelShader, PositionTexture, PositionTextureSampler, SamplerStatePoint, PositionTextureRHI );
		SetTextureParameter(RHICmdList, PixelShader, CurveTexture, CurveTextureSampler, SamplerStatePoint, CurveTextureRHI );
	}

private:

	LAYOUT_FIELD(FShaderParameter, VisualizationMode);
	LAYOUT_FIELD(FShaderResourceParameter, PositionTexture);
	LAYOUT_FIELD(FShaderResourceParameter, PositionTextureSampler);
	LAYOUT_FIELD(FShaderResourceParameter, CurveTexture);
	LAYOUT_FIELD(FShaderResourceParameter, CurveTextureSampler);
};

/** Implementation for all shaders used for visualization. */
IMPLEMENT_SHADER_TYPE(,FParticleSimVisualizeVS,TEXT("/Engine/Private/ParticleSimVisualizeShader.usf"),TEXT("VertexMain"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FParticleSimVisualizePS,TEXT("/Engine/Private/ParticleSimVisualizeShader.usf"),TEXT("PixelMain"),SF_Pixel);

/**
 * Vertex declaration for particle simulation visualization.
 */
class FParticleSimVisualizeVertexDeclaration : public FRenderResource
{
public:

	/** The vertex declaration. */
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float2, 0, sizeof(FVector2f)));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration( Elements );
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** Global vertex declaration resource for particle sim visualization. */
TGlobalResource<FParticleSimVisualizeVertexDeclaration> GParticleSimVisualizeVertexDeclaration;

/**
 * Visualizes the current state of simulation on the GPU.
 * @param RenderTarget - The render target on which to draw the visualization.
 */
static void VisualizeGPUSimulation(
	FRHICommandList& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	int32 VisualizationMode,
	FRenderTarget* RenderTarget,
	const FParticleStateTextures& StateTextures,
	FRHITexture2D* CurveTextureRHI
	)
{
	check(IsInRenderingThread());
	SCOPED_DRAW_EVENT(RHICmdList, ParticleSimDebugDraw);

	// Some constants for laying out the debug view.
	const float DisplaySizeX = 256.0f;
	const float DisplaySizeY = 256.0f;
	const float DisplayOffsetX = 60.0f;
	const float DisplayOffsetY = 60.0f;
	
	FGraphicsPipelineStateInitializer GraphicsPSOInit;

	// Setup render states.
	FIntPoint TargetSize = RenderTarget->GetSizeXY();

	FRHIRenderPassInfo RPInfo(RenderTarget->GetRenderTargetTexture(), ERenderTargetActions::Load_Store);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("VisualizeGPUSimulation"));
	{
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, (float)TargetSize.X, (float)TargetSize.Y, 1.0f);
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

		// Grab shaders.
		TShaderMapRef<FParticleSimVisualizeVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
		TShaderMapRef<FParticleSimVisualizePS> PixelShader(GetGlobalShaderMap(FeatureLevel));

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GParticleSimVisualizeVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		// Parameters for the visualization.
		FParticleSimVisualizeParameters Parameters;
		Parameters.ScaleBias.X = 2.0f * DisplaySizeX / (float)TargetSize.X;
		Parameters.ScaleBias.Y = 2.0f * DisplaySizeY / (float)TargetSize.Y;
		Parameters.ScaleBias.Z = 2.0f * DisplayOffsetX / (float)TargetSize.X - 1.0f;
		Parameters.ScaleBias.W = 2.0f * DisplayOffsetY / (float)TargetSize.Y - 1.0f;
		FParticleSimVisualizeBufferRef UniformBuffer = FParticleSimVisualizeBufferRef::CreateUniformBufferImmediate(Parameters, UniformBuffer_SingleDraw);
		VertexShader->SetParameters(RHICmdList, UniformBuffer);
		PixelShader->SetParameters(RHICmdList, VisualizationMode, StateTextures.PositionTextureRHI, CurveTextureRHI);

		const int32 VertexStride = sizeof(FVector2f);

		// Bind vertex stream.
		RHICmdList.SetStreamSource(
			0,
			GParticleTexCoordVertexBuffer.VertexBufferRHI,
			/*VertexOffset=*/ 0
		);

		// Draw.
		RHICmdList.DrawIndexedPrimitive(
			GParticleIndexBuffer.IndexBufferRHI,
			/*BaseVertexIndex=*/ 0,
			/*MinIndex=*/ 0,
			/*NumVertices=*/ 4,
			/*StartIndex=*/ 0,
			/*NumPrimitives=*/ 2,
			/*NumInstances=*/ 1
		);
	}
	RHICmdList.EndRenderPass();
}

/**
 * Constructs a particle vertex buffer on the CPU for a given set of tiles.
 * @param VertexBuffer - The buffer with which to fill with particle indices.
 * @param InTiles - The list of tiles for which to generate indices.
 */
static void BuildParticleVertexBuffer(FRHIBuffer* VertexBufferRHI, const TArray<uint32>& InTiles )
{
	check( IsInRenderingThread() );

	const int32 TileCount = InTiles.Num();
	const int32 IndexCount = TileCount * GParticlesPerTile;
	const int32 BufferSize = IndexCount * sizeof(FParticleIndex);
	const int32 Stride = 1;
	FParticleIndex* RESTRICT ParticleIndices = (FParticleIndex*)RHILockBuffer( VertexBufferRHI, 0, BufferSize, RLM_WriteOnly );

	for ( int32 Index = 0; Index < TileCount; ++Index )
	{
		const uint32 TileIndex = InTiles[Index];
		const FVector2D TileOffset(
			FMath::Fractional( (float)TileIndex / (float)GParticleSimulationTileCountX ),
			FMath::Fractional( FMath::TruncToFloat( (float)TileIndex / (float)GParticleSimulationTileCountX ) / (float)GParticleSimulationTileCountY )
			);
		for ( int32 ParticleY = 0; ParticleY < GParticleSimulationTileSize; ++ParticleY )
		{
			for ( int32 ParticleX = 0; ParticleX < GParticleSimulationTileSize; ++ParticleX )
			{
				const float IndexX = TileOffset.X + ((float)ParticleX / (float)GParticleSimulationTextureSizeX) + (0.5f / (float)GParticleSimulationTextureSizeX);
				const float IndexY = TileOffset.Y + ((float)ParticleY / (float)GParticleSimulationTextureSizeY) + (0.5f / (float)GParticleSimulationTextureSizeY);

				// @todo faster float32 -> float16 conversion
				//	use AVX2/F16C for _mm_cvtps_ph
				ParticleIndices->X.Set(IndexX);
				ParticleIndices->Y.Set(IndexY);					

				// move to next particle
				ParticleIndices += Stride;
			}
		}
	}
	RHIUnlockBuffer( VertexBufferRHI );
}

/*-----------------------------------------------------------------------------
	Determine bounds for GPU particles.
-----------------------------------------------------------------------------*/

/** The number of threads per group used to generate particle keys. */
#define PARTICLE_BOUNDS_THREADS 64

/**
 * Uniform buffer parameters for generating particle bounds.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FParticleBoundsParameters, )
	SHADER_PARAMETER( uint32, ChunksPerGroup )
	SHADER_PARAMETER( uint32, ExtraChunkCount )
	SHADER_PARAMETER( uint32, ParticleCount )
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FParticleBoundsParameters,"ParticleBounds");

typedef TUniformBufferRef<FParticleBoundsParameters> FParticleBoundsUniformBufferRef;

/**
 * Compute shader used to generate particle bounds.
 */
class FParticleBoundsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleBoundsCS,Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("THREAD_COUNT"), PARTICLE_BOUNDS_THREADS );
		OutEnvironment.CompilerFlags.Add( CFLAG_StandardOptimization );
	}

	/** Default constructor. */
	FParticleBoundsCS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleBoundsCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		InParticleIndices.Bind( Initializer.ParameterMap, TEXT("InParticleIndices") );
		PositionTexture.Bind( Initializer.ParameterMap, TEXT("PositionTexture") );
		PositionTextureSampler.Bind( Initializer.ParameterMap, TEXT("PositionTextureSampler") );
		OutBounds.Bind( Initializer.ParameterMap, TEXT("OutBounds") );
		TextureSizeX.Bind(Initializer.ParameterMap, TEXT("TextureSizeX"));
		TextureSizeY.Bind(Initializer.ParameterMap, TEXT("TextureSizeY"));
	}

	/**
	 * Set output buffers for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList, FRHIUnorderedAccessView* OutBoundsUAV )
	{
		FRHIComputeShader* ComputeShaderRHI = RHICmdList.GetBoundComputeShader();
		if ( OutBounds.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutBounds.GetBaseIndex(), OutBoundsUAV);
		}
	}

	/**
	 * Set input parameters.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList,
		FParticleBoundsUniformBufferRef& UniformBuffer,
		FRHIShaderResourceView* InIndicesSRV,
		FRHITexture2D* PositionTextureRHI
		)
	{
		FRHIComputeShader* ComputeShaderRHI = RHICmdList.GetBoundComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FParticleBoundsParameters>(), UniformBuffer );
		if ( InParticleIndices.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), InIndicesSRV);
		}
		if ( PositionTexture.IsBound() )
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, PositionTexture.GetBaseIndex(), PositionTextureRHI);
		}

		SetShaderValue(RHICmdList, ComputeShaderRHI, TextureSizeX, GParticleSimulationTextureSizeX);
		SetShaderValue(RHICmdList, ComputeShaderRHI, TextureSizeY, GParticleSimulationTextureSizeY);
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FRHIComputeShader* ComputeShaderRHI = RHICmdList.GetBoundComputeShader();
		if ( InParticleIndices.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), nullptr);
		}
		if ( OutBounds.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutBounds.GetBaseIndex(), nullptr);
		}
	}

private:
	/** Input buffer containing particle indices. */
	LAYOUT_FIELD(FShaderResourceParameter, InParticleIndices);
	/** Texture containing particle positions. */
	LAYOUT_FIELD(FShaderResourceParameter, PositionTexture);
	LAYOUT_FIELD(FShaderResourceParameter, PositionTextureSampler);
	/** Output key buffer. */
	LAYOUT_FIELD(FShaderResourceParameter, OutBounds);
	LAYOUT_FIELD(FShaderParameter, TextureSizeX);
	LAYOUT_FIELD(FShaderParameter, TextureSizeY);
};
IMPLEMENT_SHADER_TYPE(,FParticleBoundsCS,TEXT("/Engine/Private/ParticleBoundsShader.usf"),TEXT("ComputeParticleBounds"),SF_Compute);

/**
 * Returns true if the Mins and Maxs consistutue valid bounds, i.e. Mins <= Maxs.
 */
static bool AreBoundsValid( const FVector& Mins, const FVector& Maxs )
{
	return Mins.X <= Maxs.X && Mins.Y <= Maxs.Y && Mins.Z <= Maxs.Z;
}

/**
 * Computes bounds for GPU particles. Note that this is slow as it requires
 * syncing with the GPU!
 * @param VertexBufferSRV - Vertex buffer containing particle indices.
 * @param PositionTextureRHI - Texture holding particle positions.
 * @param ParticleCount - The number of particles in the emitter.
 */
static FBox ComputeParticleBounds(
	FRHICommandListImmediate& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	FRHIShaderResourceView* VertexBufferSRV,
	FRHITexture2D* PositionTextureRHI,
	int32 ParticleCount )
{
	FBox BoundingBox;
	FParticleBoundsParameters Parameters;
	FParticleBoundsUniformBufferRef UniformBuffer;

	if (ParticleCount > 0 && FeatureLevel >= ERHIFeatureLevel::SM5)
	{
		// Determine how to break the work up over individual work groups.
		const uint32 MaxGroupCount = 128;
		const uint32 AlignedParticleCount = ((ParticleCount + PARTICLE_BOUNDS_THREADS - 1) & (~(PARTICLE_BOUNDS_THREADS - 1)));
		const uint32 ChunkCount = AlignedParticleCount / PARTICLE_BOUNDS_THREADS;
		const uint32 GroupCount = FMath::Clamp<uint32>( ChunkCount, 1, MaxGroupCount );

		// Create the uniform buffer.
		Parameters.ChunksPerGroup = ChunkCount / GroupCount;
		Parameters.ExtraChunkCount = ChunkCount % GroupCount;
		Parameters.ParticleCount = ParticleCount;
		UniformBuffer = FParticleBoundsUniformBufferRef::CreateUniformBufferImmediate( Parameters, UniformBuffer_SingleFrame );

		// Create a buffer for storing bounds.
		const int32 BufferSize = GroupCount * 2 * sizeof(FVector4f);
		FRHIResourceCreateInfo CreateInfo(TEXT("BoundsVertexBuffer"));
		FBufferRHIRef BoundsVertexBufferRHI = RHICreateVertexBuffer(
			BufferSize,
			BUF_Static | BUF_UnorderedAccess | BUF_KeepCPUAccessible,
			CreateInfo);
		FUnorderedAccessViewRHIRef BoundsVertexBufferUAV = RHICreateUnorderedAccessView(
			BoundsVertexBufferRHI,
			PF_A32B32G32R32F );

		// Grab the shader.
		TShaderMapRef<FParticleBoundsCS> ParticleBoundsCS(GetGlobalShaderMap(FeatureLevel));
		SetComputePipelineState(RHICmdList, ParticleBoundsCS.GetComputeShader());

		// Dispatch shader to compute bounds.
		ParticleBoundsCS->SetOutput(RHICmdList, BoundsVertexBufferUAV);
		ParticleBoundsCS->SetParameters(RHICmdList, UniformBuffer, VertexBufferSRV, PositionTextureRHI);
		DispatchComputeShader(
			RHICmdList, 
			ParticleBoundsCS.GetShader(), 
			GroupCount,
			1,
			1 );
		ParticleBoundsCS->UnbindBuffers(RHICmdList);

		// Read back bounds.
		FVector4f* GroupBounds = (FVector4f*)RHILockBuffer( BoundsVertexBufferRHI, 0, BufferSize, RLM_ReadOnly );

		// Find valid starting bounds.
		uint32 GroupIndex = 0;
		do
		{
			BoundingBox.Min = FVector(FVector4(GroupBounds[GroupIndex * 2 + 0]));
			BoundingBox.Max = FVector(FVector4(GroupBounds[GroupIndex * 2 + 1]));
			GroupIndex++;
		} while ( GroupIndex < GroupCount && !AreBoundsValid( BoundingBox.Min, BoundingBox.Max ) );

		if ( GroupIndex == GroupCount )
		{
			// No valid bounds!
			BoundingBox.Init();
		}
		else
		{
			// Bounds are valid. Add any other valid bounds.
			BoundingBox.IsValid = true;
			while ( GroupIndex < GroupCount )
			{
				FVector Mins( (FVector4)GroupBounds[GroupIndex * 2 + 0] );
				FVector Maxs( (FVector4)GroupBounds[GroupIndex * 2 + 1] );
				if ( AreBoundsValid( Mins, Maxs ) )
				{
					BoundingBox += Mins;
					BoundingBox += Maxs;
				}
				GroupIndex++;
			}
		}

		// Release buffer.
		RHICmdList.UnlockBuffer(BoundsVertexBufferRHI);
		BoundsVertexBufferUAV.SafeRelease();
		BoundsVertexBufferRHI.SafeRelease();
	}
	else
	{
		BoundingBox.Init();
	}

	return BoundingBox;
}

/*-----------------------------------------------------------------------------
	Per-emitter GPU particle simulation.
-----------------------------------------------------------------------------*/

/**
 * Per-emitter resources for simulation.
 */
struct FParticleEmitterSimulationResources
{
	/** Emitter uniform buffer used for simulation. */
	FParticleSimulationBufferRef SimulationUniformBuffer;
	/** Scale to apply to global vector fields. */
	float GlobalVectorFieldScale;
	/** Tightness override value to apply to global vector fields. */
	float GlobalVectorFieldTightness;
};

/**
 * Vertex buffer used to hold tile offsets.
 */
class FParticleTileVertexBuffer : public FVertexBuffer
{
public:
	/** Shader resource of the vertex buffer. */
	FShaderResourceViewRHIRef VertexBufferSRV;
	/** The number of tiles held by this vertex buffer. */
	int32 TileCount;
	/** The number of tiles held by this vertex buffer, aligned for tile rendering. */
	int32 AlignedTileCount;

	/** Default constructor. */
	FParticleTileVertexBuffer()
		: TileCount(0)
		, AlignedTileCount(0)
	{
	}
	
	
	FParticleShaderParamRef GetShaderParam() { return VertexBufferSRV; }

	/**
	 * Initializes the vertex buffer from a list of tiles.
	 */
	void Init( const TArray<uint32>& Tiles )
	{
		check( IsInRenderingThread() );
		TileCount = Tiles.Num();
		AlignedTileCount = ComputeAlignedTileCount(TileCount);
		InitResource();
		if (Tiles.Num())
		{
			int32 BufferAlignedTileCount = (GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1 ? TileCount : AlignedTileCount);
			BuildTileVertexBuffer(VertexBufferRHI, Tiles.GetData(), Tiles.Num(), BufferAlignedTileCount);
		}
	}

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		if ( AlignedTileCount > 0 )
		{
			int32 BufferAlignedTileCount = (GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1 ? TileCount : AlignedTileCount);
			const int32 TileBufferSize = BufferAlignedTileCount * sizeof(FVector2f);
			check(TileBufferSize > 0);
			FRHIResourceCreateInfo CreateInfo(TEXT("FParticleTileVertexBuffer"));
			VertexBufferRHI = RHICreateVertexBuffer( TileBufferSize, BUF_Static | BUF_KeepCPUAccessible | BUF_ShaderResource, CreateInfo );
			VertexBufferSRV = RHICreateShaderResourceView( VertexBufferRHI, /*Stride=*/ sizeof(FVector2f), PF_G32R32F );
		}
	}

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override
	{
		TileCount = 0;
		AlignedTileCount = 0;
		VertexBufferSRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}
};

/**
 * Vertex buffer used to hold particle indices.
 */
class FGPUParticleVertexBuffer : public FVertexBuffer
{
public:

	/** Shader resource view of the vertex buffer. */
	FShaderResourceViewRHIRef VertexBufferSRV;

	/** The number of particles referenced by this vertex buffer. */
	int32 ParticleCount;

	/** Default constructor. */
	FGPUParticleVertexBuffer()
		: ParticleCount(0)
	{
	}

	/**
	 * Initializes the vertex buffer from a list of tiles.
	 */
	void Init( const TArray<uint32>& Tiles )
	{
		check( IsInRenderingThread() );
		ParticleCount = Tiles.Num() * GParticlesPerTile;
		InitResource();
		if ( Tiles.Num() )
		{
			BuildParticleVertexBuffer( VertexBufferRHI, Tiles );
		}
	}

	/** Initialize RHI resources. */
	virtual void InitRHI() override
	{
		if ( RHISupportsGPUParticles() )
		{
			// Metal *requires* that a buffer be bound - you cannot protect access with a branch in the shader.
			int32 Count = FMath::Max(ParticleCount, 1);
			const int32 BufferStride = sizeof(FParticleIndex);
			const int32 BufferSize = Count * BufferStride;
			const EBufferUsageFlags Flags = BUF_Static | /*BUF_KeepCPUAccessible | */BUF_ShaderResource;
			FRHIResourceCreateInfo CreateInfo(TEXT("FGPUParticleVertexBuffer"));
			VertexBufferRHI = RHICreateVertexBuffer(BufferSize, Flags, CreateInfo);
			VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, BufferStride, PF_G16R16F);
		}
	}

	/** Release RHI resources. */
	virtual void ReleaseRHI() override
	{
		VertexBufferSRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}
};

/**
 * Resources for simulating a set of particles on the GPU.
 */
class FParticleSimulationGPU
{
public:

	/** The vertex buffer used to access tiles in the simulation. */
	FParticleTileVertexBuffer TileVertexBuffer;
	/** Reference to the GPU sprite resources. */
	TRefCountPtr<FGPUSpriteResources> GPUSpriteResources;
	/** The per-emitter simulation resources. */
	const FParticleEmitterSimulationResources* EmitterSimulationResources;
	/** The per-frame simulation uniform buffer. */
	FParticlePerFrameSimulationParameters PerFrameSimulationParameters;
	/** Bounds for particles in the simulation. */
	FBox Bounds;

	/** A list of new particles to inject in to the simulation for this emitter. */
	TArray<FNewParticle> NewParticles;
	/** A list of tiles to clear that were newly allocated for this emitter. */
	TArray<uint32> TilesToClear;

	/** Local vector field. */
	FVectorFieldInstance LocalVectorField;

	/** The vertex buffer used to access particles in the simulation. */
	FGPUParticleVertexBuffer VertexBuffer;
	/** The vertex factory for visualizing the local vector field. */
	FVectorFieldVisualizationVertexFactory* VectorFieldVisualizationVertexFactory;

#if GPUPARTICLE_LOCAL_VF_ONLY
	/** If we only support local VF's then we can cache this uniform buffer */
	FVectorFieldUniformBufferRef LocalVectorFieldUniformBuffer;
	float LocalIntensity;
#endif

	/** The simulation index within the associated FX system. */
	int32 SimulationIndex;

	/**
	 * The phase in which these particles should simulate.
	 */
	EParticleSimulatePhase::Type SimulationPhase;

	/** True if the simulation wants collision enabled. */
	bool bWantsCollision;

	EParticleCollisionMode::Type CollisionMode;

	/** Flag that specifies the simulation's resources are dirty and need to be updated. */
	bool bDirty_GameThread;
	bool bReleased_GameThread;
	bool bDestroyed_GameThread;

	/** Allows disabling of simulation. */
	bool bEnabled;

	/** Default constructor. */
	FParticleSimulationGPU()
		: EmitterSimulationResources(NULL)
		, VectorFieldVisualizationVertexFactory(NULL)
#if GPUPARTICLE_LOCAL_VF_ONLY
		, LocalIntensity(0.0f)
#endif
		, SimulationIndex(INDEX_NONE)
		, SimulationPhase(EParticleSimulatePhase::Main)
		, bWantsCollision(false)
		, CollisionMode(EParticleCollisionMode::SceneDepth)
		, bDirty_GameThread(true)
		, bReleased_GameThread(true)
		, bDestroyed_GameThread(false)
		, bEnabled(true)
	{
	}

	/** Destructor. */
	~FParticleSimulationGPU()
	{
		delete VectorFieldVisualizationVertexFactory;
		VectorFieldVisualizationVertexFactory = NULL;
	}

	/**
	 * Initializes resources for simulating particles on the GPU.
	 * @param Tiles							The list of tiles to include in the simulation.
	 * @param InEmitterSimulationResources	The emitter resources used by this simulation.
	 */
	void InitResources(const TArray<uint32>& Tiles, FGPUSpriteResources* InGPUSpriteResources);

	/**
	 * Create and initializes a visualization vertex factory if needed.
	 */
	void CreateVectorFieldVisualizationVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
	{
		if (VectorFieldVisualizationVertexFactory == NULL)
		{
			check(IsInRenderingThread());
			VectorFieldVisualizationVertexFactory = new FVectorFieldVisualizationVertexFactory(InFeatureLevel);
			VectorFieldVisualizationVertexFactory->InitResource();
		}
	}

	/**
	 * Release and destroy simulation resources.
	 */
	void Destroy()
	{
		bDestroyed_GameThread = true;
		FParticleSimulationGPU* Simulation = this;
		ENQUEUE_RENDER_COMMAND(FReleaseParticleSimulationGPUCommand)(
			[Simulation](FRHICommandList& RHICmdList)
			{
				Simulation->Destroy_RenderThread();
			});
	}

	/**
	 * Destroy the simulation on the rendering thread.
	 */
	void Destroy_RenderThread()
	{
		// The check for IsEngineExitRequested() is done because at shut down UWorld can be destroyed before particle emitters(?)
		check(IsEngineExitRequested() || SimulationIndex == INDEX_NONE);
		ReleaseRenderResources();
		delete this;
	}

	/**
	 * Enqueues commands to release render resources.
	 */
	void BeginReleaseResources()
	{
		bReleased_GameThread = true;
		FParticleSimulationGPU* Simulation = this;
		ENQUEUE_RENDER_COMMAND(FReleaseParticleSimulationResourcesGPUCommand)(
			[Simulation](FRHICommandList& RHICmdList)
			{
				Simulation->ReleaseRenderResources();
			});
	}

private:

	/**
	 * Release resources on the rendering thread.
	 */
	void ReleaseRenderResources()
	{
		check( IsInRenderingThread() );
		VertexBuffer.ReleaseResource();
		TileVertexBuffer.ReleaseResource();
		if ( VectorFieldVisualizationVertexFactory )
		{
			VectorFieldVisualizationVertexFactory->ReleaseResource();
		}
	}
};

/*-----------------------------------------------------------------------------
	Dynamic emitter data for GPU sprite particles.
-----------------------------------------------------------------------------*/

/**
 * Per-emitter resources for GPU sprites.
 */
class FGPUSpriteResources : public FRenderResource
{
public:

	/** Emitter uniform buffer used for rendering. */
	FGPUSpriteEmitterUniformBufferRef UniformBuffer;
	/** Emitter simulation resources. */
	FParticleEmitterSimulationResources EmitterSimulationResources;
	/** Texel allocation for the color curve. */
	FTexelAllocation ColorTexelAllocation;
	/** Texel allocation for the misc attributes curve. */
	FTexelAllocation MiscTexelAllocation;
	/** Texel allocation for the simulation attributes curve. */
	FTexelAllocation SimulationAttrTexelAllocation;
	/** Emitter uniform parameters used for rendering. */
	FGPUSpriteEmitterUniformParameters UniformParameters;
	/** Emitter uniform parameters used for simulation. */
	FParticleSimulationParameters SimulationParameters;

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		UniformBuffer = FGPUSpriteEmitterUniformBufferRef::CreateUniformBufferImmediate( UniformParameters, UniformBuffer_MultiFrame );
		EmitterSimulationResources.SimulationUniformBuffer =
			FParticleSimulationBufferRef::CreateUniformBufferImmediate( SimulationParameters, UniformBuffer_MultiFrame );
	}

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override
	{
		UniformBuffer.SafeRelease();
		EmitterSimulationResources.SimulationUniformBuffer.SafeRelease();
	}

	inline uint32 AddRef()
	{
		return NumRefs.Increment();
	}

	inline uint32 Release()
	{
		int32 Refs = NumRefs.Decrement();
		check(Refs >= 0);

		if (Refs == 0)
		{
			// When all references are released, we need the render thread
			// to release RHI resources and delete this instance.
			FRenderResource* Resource = this;
			ENQUEUE_RENDER_COMMAND(ReleaseCommand)(
				[Resource](FRHICommandList& RHICmdList)
				{
					Resource->ReleaseResource();
					delete Resource;
				});
		}
		return Refs;
	}

private:
	FThreadSafeCounter NumRefs;
};

void FParticleSimulationGPU::InitResources(const TArray<uint32>& Tiles, FGPUSpriteResources* InGPUSpriteResources)
{
	ensure(InGPUSpriteResources);

	if (InGPUSpriteResources)
	{
		TRefCountPtr<FGPUSpriteResources> InGPUSpriteResourcesRef = InGPUSpriteResources;
		FParticleSimulationGPU* Simulation = this;
		ENQUEUE_RENDER_COMMAND(FInitParticleSimulationGPUCommand)(
			[Simulation, Tiles, InGPUSpriteResourcesRef](FRHICommandListImmediate& RHICmdList)
			{
				// Release vertex buffers.
				Simulation->VertexBuffer.ReleaseResource();
				Simulation->TileVertexBuffer.ReleaseResource();

				// Initialize new buffers with list of tiles.
				Simulation->VertexBuffer.Init(Tiles);
				Simulation->TileVertexBuffer.Init(Tiles);

				// Store simulation resources for this emitter.
				Simulation->GPUSpriteResources = InGPUSpriteResourcesRef;
				Simulation->EmitterSimulationResources = &InGPUSpriteResourcesRef->EmitterSimulationResources;

				// If a visualization vertex factory has been created, initialize it.
				if (Simulation->VectorFieldVisualizationVertexFactory)
				{
					Simulation->VectorFieldVisualizationVertexFactory->InitResource();
				}
		});
	}

	bDirty_GameThread = false;
	bReleased_GameThread = false;
}

class FGPUSpriteCollectorResources : public FOneFrameResource
{
public:
	FGPUSpriteCollectorResources(ERHIFeatureLevel::Type InFeatureLevel)
		: VertexFactory(InFeatureLevel)
	{
	}

	virtual ~FGPUSpriteCollectorResources()
	{
		VertexFactory.ReleaseResource();
	}

	FGPUSpriteVertexFactory VertexFactory;
	FGPUSpriteEmitterDynamicUniformBufferRef UniformBuffer;
};

// recycle memory blocks for the NewParticle array
static void FreeNewParticleArray(TArray<FNewParticle>& NewParticles)
{
	NewParticles.Reset();
}

static void GetNewParticleArray(TArray<FNewParticle>& NewParticles, int32 NumParticlesNeeded = -1)
{
	if (NumParticlesNeeded > 0)
	{
		NewParticles.Reserve(NumParticlesNeeded);
	}
}

/**
 * Dynamic emitter data for Cascade.
 */
struct FGPUSpriteDynamicEmitterData : FDynamicEmitterDataBase
{
public:
	FMaterialRenderProxy *MaterialProxy;
	// translucent?
	bool bIsMaterialTranslucent;
	/** FX system. */
	FFXSystem* FXSystem;
	/** Per-emitter resources. */
	FGPUSpriteResources* Resources;
	/** Simulation resources. */
	FParticleSimulationGPU* Simulation;
	/** Bounds for particles in the simulation. */
	FBox SimulationBounds;
	/** A list of new particles to inject in to the simulation for this emitter. */
	TArray<FNewParticle> NewParticles;
	/** A list of tiles to clear that were newly allocated for this emitter. */
	TArray<uint32> TilesToClear;
	/** Vector field-to-world transform. */
	FMatrix LocalVectorFieldToWorld;
	/** Vector field scale. */
	float LocalVectorFieldIntensity;
	/** Vector field tightness. */
	float LocalVectorFieldTightness;
	/** Per-frame simulation parameters. */
	FParticlePerFrameSimulationParameters PerFrameSimulationParameters;
	/** Per-emitter parameters that may change*/
	FGPUSpriteEmitterDynamicUniformParameters EmitterDynamicParameters;
	/** How the particles should be sorted, if at all. */
	EParticleSortMode SortMode;
	/** Whether to render particles in local space or world space. */
	bool bUseLocalSpace;	
	/** Tile vector field in x axis? */
	uint32 bLocalVectorFieldTileX : 1;
	/** Tile vector field in y axis? */
	uint32 bLocalVectorFieldTileY : 1;
	/** Tile vector field in z axis? */
	uint32 bLocalVectorFieldTileZ : 1;
	/** Tile vector field in z axis? */
	uint32 bLocalVectorFieldUseFixDT : 1;


	/** Current MacroUV override settings */
	FMacroUVOverride MacroUVOverride;

	/** Constructor. */
	explicit FGPUSpriteDynamicEmitterData( const UParticleModuleRequired* InRequiredModule )
		: FDynamicEmitterDataBase( InRequiredModule )
		, MaterialProxy(nullptr)
		, bIsMaterialTranslucent(true)
		, FXSystem(nullptr)
		, Resources(nullptr)
		, Simulation(nullptr)
		, SortMode(PSORTMODE_None)
		, bLocalVectorFieldTileX(false)
		, bLocalVectorFieldTileY(false)
		, bLocalVectorFieldTileZ(false)
		, bLocalVectorFieldUseFixDT(false)
	{
		GetNewParticleArray(NewParticles);
	}
	~FGPUSpriteDynamicEmitterData()
	{
		FreeNewParticleArray(NewParticles);
	}

	bool RendersWithTranslucentMaterial() const
	{
		return bIsMaterialTranslucent;
	}

	/**
	 * Called to create render thread resources.
	 */
	virtual void UpdateRenderThreadResourcesEmitter(const FParticleSystemSceneProxy* InOwnerProxy) override
	{
		check(Simulation);

		// Update the per-frame simulation parameters with those provided from the game thread.
		Simulation->PerFrameSimulationParameters = PerFrameSimulationParameters;

		// Local vector field parameters.
		Simulation->LocalVectorField.Intensity = LocalVectorFieldIntensity;
		Simulation->LocalVectorField.Tightness = LocalVectorFieldTightness;
		Simulation->LocalVectorField.bTileX = bLocalVectorFieldTileX;
		Simulation->LocalVectorField.bTileY = bLocalVectorFieldTileY;
		Simulation->LocalVectorField.bTileZ = bLocalVectorFieldTileZ;
		Simulation->LocalVectorField.bUseFixDT = bLocalVectorFieldUseFixDT;

		if (Simulation->LocalVectorField.Resource)
		{
			Simulation->LocalVectorField.UpdateTransforms(LocalVectorFieldToWorld);
		}

		// Update world bounds.
		Simulation->Bounds = SimulationBounds;

		// Transfer ownership of new data.
		if (NewParticles.Num())
		{
			Exchange(Simulation->NewParticles, NewParticles);
		}
		if (TilesToClear.Num())
		{
			Exchange(Simulation->TilesToClear, TilesToClear);
		}

		const bool bTranslucent = RendersWithTranslucentMaterial();
		const bool bSupportsDepthBufferCollision = IsParticleCollisionModeSupported(FXSystem->GetShaderPlatform(), PCM_DepthBuffer);

		// If the simulation wants to collide against the depth buffer
		// and we're not rendering with an opaque material put the 
		// simulation in the collision phase.
		const EParticleSimulatePhase::Type PrevPhase = Simulation->SimulationPhase;
		if (bTranslucent && Simulation->bWantsCollision && Simulation->CollisionMode == EParticleCollisionMode::SceneDepth)
		{
			Simulation->SimulationPhase = bSupportsDepthBufferCollision ? EParticleSimulatePhase::CollisionDepthBuffer : EParticleSimulatePhase::Main;
		}
		else if (Simulation->bWantsCollision && Simulation->CollisionMode == EParticleCollisionMode::DistanceField)
		{
			if (IsParticleCollisionModeSupported(FXSystem->GetShaderPlatform(), PCM_DistanceField))
			{
				Simulation->SimulationPhase = EParticleSimulatePhase::CollisionDistanceField;
			}
			else if (bTranslucent && bSupportsDepthBufferCollision)
			{
				// Fall back to scene depth collision if translucent
				Simulation->SimulationPhase = EParticleSimulatePhase::CollisionDepthBuffer;
			}
			else
			{
				Simulation->SimulationPhase = EParticleSimulatePhase::Main;
			}
		}
		if (Simulation->SimulationPhase != PrevPhase)
		{
			FXSystem->OnSimulationPhaseChanged(Simulation, PrevPhase);
		}
	}

	/**
	 * Called to release render thread resources.
	 */
	virtual void ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy) override
	{		
	}

	virtual void GetDynamicMeshElementsEmitter(const FParticleSystemSceneProxy* Proxy, const FSceneView* View, const FSceneViewFamily& ViewFamily, int32 ViewIndex, FMeshElementCollector& Collector) const override
	{
		auto FeatureLevel = ViewFamily.GetFeatureLevel();

		if (RHISupportsGPUParticles())
		{
			SCOPE_CYCLE_COUNTER(STAT_GPUSpritePreRenderTime);

			check(Simulation);

			// Do not render orphaned emitters. This can happen if the emitter
			// instance has been destroyed but we are rendering before the
			// scene proxy has received the update to clear dynamic data.
			if (Simulation->SimulationIndex != INDEX_NONE
				&& Simulation->VertexBuffer.ParticleCount > 0)
			{
				FGPUSpriteEmitterDynamicUniformParameters PerViewDynamicParameters = EmitterDynamicParameters;
				FVector2D ObjectNDCPosition;
				FVector2D ObjectMacroUVScales;
				Proxy->GetObjectPositionAndScale(*View,ObjectNDCPosition, ObjectMacroUVScales);
				PerViewDynamicParameters.MacroUVParameters = FVector4f(ObjectNDCPosition.X, ObjectNDCPosition.Y, ObjectMacroUVScales.X, ObjectMacroUVScales.Y);

				if (bUseLocalSpace == false)
				{
					Proxy->UpdateWorldSpacePrimitiveUniformBuffer();
				}

				const bool bTranslucent = RendersWithTranslucentMaterial();
				const bool bAllowSorting = FXConsoleVariables::bAllowGPUSorting
					&& bTranslucent;

				// Iterate over views and assign parameters for each.
				FParticleSimulationResources* SimulationResources = FXSystem->GetParticleSimulationResources();
				FGPUSpriteCollectorResources& CollectorResources = Collector.AllocateOneFrameResource<FGPUSpriteCollectorResources>(FeatureLevel);
				FGPUSpriteVertexFactory& VertexFactory = CollectorResources.VertexFactory;
				VertexFactory.InitResource();

				// Do here rather than in CreateRenderThreadResources because in some cases Render can be called before CreateRenderThreadResources
				// Create per-emitter uniform buffer for dynamic parameters
				CollectorResources.UniformBuffer = FGPUSpriteEmitterDynamicUniformBufferRef::CreateUniformBufferImmediate(PerViewDynamicParameters, UniformBuffer_SingleFrame);

				FGPUSpriteMeshDataUserData* MeshBatchUserData = nullptr;
				if (bAllowSorting && SortMode == PSORTMODE_DistanceToView)
				{
					// Extensibility TODO: This call to AddSortedGPUSimulation is very awkward. When rendering a frame we need to
					// accumulate all GPU particle emitters that need to be sorted. That is so they can be sorted in one big radix
					// sort for efficiency. Ideally that state is per-scene renderer but the renderer doesn't know anything about particles.
					FGPUSortManager::FAllocationInfo SortedIndicesInfo;
					if (FXSystem->AddSortedGPUSimulation(Simulation, View->ViewMatrices.GetViewOrigin(), bTranslucent, SortedIndicesInfo))
					{
						MeshBatchUserData = &Collector.AllocateOneFrameResource<FGPUSpriteMeshDataUserData>();
						MeshBatchUserData->SortedOffset = SortedIndicesInfo.BufferOffset;
						MeshBatchUserData->SortedParticleIndicesSRV = SortedIndicesInfo.BufferSRV;
					}
				}
				check(Simulation->VertexBuffer.IsInitialized());
				VertexFactory.SetUnsortedParticleIndicesSRV(Simulation->VertexBuffer.VertexBufferSRV);

				const int32 ParticleCount = Simulation->VertexBuffer.ParticleCount;
				const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;

				{
					SCOPE_CYCLE_COUNTER(STAT_GPUSpriteRenderingTime);

					FParticleSimulationResources* ParticleSimulationResources = FXSystem->GetParticleSimulationResources();
					FParticleStateTextures& StateTextures = ParticleSimulationResources->GetVisualizeStateTextures();
							
					VertexFactory.EmitterUniformBuffer = Resources->UniformBuffer;
					VertexFactory.EmitterDynamicUniformBuffer = CollectorResources.UniformBuffer;
					VertexFactory.PositionTextureRHI = StateTextures.PositionTextureRHI;
					VertexFactory.VelocityTextureRHI = StateTextures.VelocityTextureRHI;
					VertexFactory.AttributesTextureRHI = ParticleSimulationResources->RenderAttributesTexture.TextureRHI;
					VertexFactory.LWCTile = ParticleSimulationResources->LWCTile;

					FMeshBatch& Mesh = Collector.AllocateMesh();
					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement.IndexBuffer = &GParticleIndexBuffer;
					BatchElement.NumPrimitives = MAX_PARTICLES_PER_INSTANCE * 2;
					BatchElement.NumInstances = ParticleCount / MAX_PARTICLES_PER_INSTANCE;
					BatchElement.FirstIndex = 0;
					BatchElement.UserData = MeshBatchUserData;
					Mesh.VertexFactory = &VertexFactory;
					Mesh.LCI = NULL;
					if ( bUseLocalSpace )
					{
						BatchElement.PrimitiveUniformBuffer = Proxy->GetUniformBuffer();
					}
					else
					{
						BatchElement.PrimitiveUniformBuffer = Proxy->GetWorldSpacePrimitiveUniformBuffer();
					}
					BatchElement.MinVertexIndex = 0;
					BatchElement.MaxVertexIndex = 3;
					Mesh.ReverseCulling = Proxy->IsLocalToWorldDeterminantNegative();
					Mesh.CastShadow = Proxy->GetCastShadow();
					Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)Proxy->GetDepthPriorityGroup(View);
					Mesh.MaterialRenderProxy = GetMaterialRenderProxy();
					Mesh.Type = PT_TriangleList;
					Mesh.bCanApplyViewModeOverrides = true;
					Mesh.bUseWireframeSelectionColoring = Proxy->IsSelected();

					Collector.AddMesh(ViewIndex, Mesh);
				}

				const bool bHaveLocalVectorField = Simulation && Simulation->LocalVectorField.Resource;
				if (bHaveLocalVectorField && ViewFamily.EngineShowFlags.VectorFields)
				{
					// Create a vertex factory for visualization if needed.
					Simulation->CreateVectorFieldVisualizationVertexFactory(FeatureLevel);
					check(Simulation->VectorFieldVisualizationVertexFactory);
					DrawVectorFieldBounds(Collector.GetPDI(ViewIndex), View, &Simulation->LocalVectorField);
					GetVectorFieldMesh(Simulation->VectorFieldVisualizationVertexFactory, &Simulation->LocalVectorField, ViewIndex, Collector);
				}
			}
		}
	}

	/**
	 * Retrieves the material render proxy with which to render sprites.
	 * Const version of the virtual below, needed because GetDynamicMeshElementsemitter is const
	 */
	const FMaterialRenderProxy* GetMaterialRenderProxy() const
	{
		check(MaterialProxy);
		return MaterialProxy;
	}

	virtual const FMaterialRenderProxy* GetMaterialRenderProxy() override
	{
		check(MaterialProxy);
		return MaterialProxy;
	}

	/**
	 * Emitter replay data. A dummy value is returned as data is stored on the GPU.
	 */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const override
	{
		static FDynamicEmitterReplayDataBase DummyData;
		return DummyData;
	}

	/** Returns the current macro uv override. */
	virtual const FMacroUVOverride& GetMacroUVOverride() const { return MacroUVOverride; }

};

/*-----------------------------------------------------------------------------
	Particle emitter instance for GPU particles.
-----------------------------------------------------------------------------*/

#if TRACK_TILE_ALLOCATIONS
TMap<FFXSystem*,TSet<class FGPUSpriteParticleEmitterInstance*> > GPUSpriteParticleEmitterInstances;
#endif // #if TRACK_TILE_ALLOCATIONS

/**
 * Particle emitter instance for Cascade.
 */
class FGPUSpriteParticleEmitterInstance : public FParticleEmitterInstance
{
	/** Weak pointer to the world we are in, if this is invalid the FXSystem is also invalid. */
	TWeakObjectPtr<UWorld> WeakWorld;
	/** Pointer the the FX system with which the instance is associated. */
	FFXSystem* FXSystem;
	/** Information on how to emit and simulate particles. */
	FGPUSpriteEmitterInfo& EmitterInfo;
	/** GPU simulation resources. */
	FParticleSimulationGPU* Simulation;
	/** The list of tiles active for this emitter. */
	TArray<uint32> AllocatedTiles;
	/** Bit array specifying which tiles are free for spawning new particles. */
	TBitArray<> ActiveTiles;
	/** The time at which each active tile will no longer have active particles. */
	TArray<float> TileTimeOfDeath;
	/** The list of tiles that need to be cleared. */
	TArray<uint32> TilesToClear;
	/** The list of new particles generated this time step. */
	TArray<FNewParticle> NewParticles;
	/** The list of force spawned particles from events */
	TArray<FNewParticle> ForceSpawnedParticles;
	/** The list of force spawned particles from events using Bursts */
	TArray<FNewParticle> ForceBurstSpawnedParticles;
	/** The rotation to apply to the local vector field. */
	FRotator LocalVectorFieldRotation;
	/** The strength of the point attractor. */
	float PointAttractorStrength;
	/** The amount of time by which the GPU needs to simulate particles during its next update. */
	float PendingDeltaSeconds;
	/** The offset for simulation time, used when we are not updating time FrameIndex. */
	float OffsetSeconds;

	/** Tile to allocate new particles from. */
	int32 TileToAllocateFrom;
	/** How many particles are free in the most recently allocated tile. */
	int32 FreeParticlesInTile;
	/** Random stream for this emitter. */
	FRandomStream RandomStream;
	/** The number of times this emitter should loop. */
	int32 AllowedLoopCount;
	/** Random number per emitter to allow each emitter to be randomized */
	float EmitterInstRandom;

	/**
	 * Information used to spawn particles.
	 */
	struct FSpawnInfo
	{
		/** Number of particles to spawn. */
		int32 Count;
		/** Time at which the first particle spawned. */
		float StartTime;
		/** Amount by which to increment time for each subsequent particle. */
		float Increment;

		/** Default constructor. */
		FSpawnInfo()
			: Count(0)
			, StartTime(0.0f)
			, Increment(0.0f)
		{
		}
	};

public:

/** Initialization constructor. */
FGPUSpriteParticleEmitterInstance(FFXSystem* InFXSystem, FGPUSpriteEmitterInfo& InEmitterInfo)
	: FXSystem(InFXSystem)
	, EmitterInfo(InEmitterInfo)
	, LocalVectorFieldRotation(FRotator::ZeroRotator)
	, PointAttractorStrength(0.0f)
	, PendingDeltaSeconds(0.0f)
	, OffsetSeconds(0.0f)
	, TileToAllocateFrom(INDEX_NONE)
	, FreeParticlesInTile(0)
	, AllowedLoopCount(0)
	{
		Simulation = new FParticleSimulationGPU();
		if (EmitterInfo.LocalVectorField.Field)
		{
			EmitterInfo.LocalVectorField.Field->InitInstance(&Simulation->LocalVectorField, /*bPreviewInstance=*/ false);
		}
		Simulation->bWantsCollision = InEmitterInfo.bEnableCollision;
		Simulation->CollisionMode = InEmitterInfo.CollisionMode;

#if TRACK_TILE_ALLOCATIONS
		TSet<class FGPUSpriteParticleEmitterInstance*>* EmitterSet = GPUSpriteParticleEmitterInstances.Find(FXSystem);
		if (!EmitterSet)
		{
			EmitterSet = &GPUSpriteParticleEmitterInstances.Add(FXSystem,TSet<class FGPUSpriteParticleEmitterInstance*>());
		}
		EmitterSet->Add(this);
#endif // #if TRACK_TILE_ALLOCATIONS
	}

	/** Destructor. */
	virtual ~FGPUSpriteParticleEmitterInstance()
	{
		ReleaseSimulationResources();
		Simulation->Destroy();
		Simulation = NULL;

#if TRACK_TILE_ALLOCATIONS
		TSet<class FGPUSpriteParticleEmitterInstance*>* EmitterSet = GPUSpriteParticleEmitterInstances.Find(FXSystem);
		check(EmitterSet);
		EmitterSet->Remove(this);
		if (EmitterSet->Num() == 0)
		{
			GPUSpriteParticleEmitterInstances.Remove(FXSystem);
		}
#endif // #if TRACK_TILE_ALLOCATIONS
	}

	/**
	 * Returns the number of tiles allocated to this emitter.
	 */
	int32 GetAllocatedTileCount() const
	{
		return AllocatedTiles.Num();
	}

	/**
	 *	Checks some common values for GetDynamicData validity
	 *
	 *	@return	bool		true if GetDynamicData should continue, false if it should return NULL
	 */
	virtual bool IsDynamicDataRequired(UParticleLODLevel* InCurrentLODLevel) override
	{
		bool bShouldRender = (ActiveParticles >= 0 || TilesToClear.Num() || NewParticles.Num());
		bool bCanRender = (FXSystem != NULL) && (Component != NULL) && (Component->FXSystem == FXSystem);
		return bShouldRender && bCanRender;
	}

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected, ERHIFeatureLevel::Type InFeatureLevel) override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicEmitterDataBase_GetDynamicData);
		check(Component);
		check(SpriteTemplate);
		check(FXSystem);
		check(Component->FXSystem == FXSystem);

		// Grab the current LOD level
		UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
		if (LODLevel->bEnabled == false || !bEnabled)
		{
			return NULL;
		}

		UParticleSystem *Template = Component->Template;

		const bool bLocalSpace = EmitterInfo.RequiredModule->bUseLocalSpace;
		const bool bUseTileOffset = bLocalSpace == false && EmitterInfo.RequiredModule->bSupportLargeWorldCoordinates;
		FTransform ComponentTransform = Component->GetComponentTransform();
		if (bUseTileOffset)
		{
			ComponentTransform.AddToTranslation(FVector(Component->GetLWCTile()) * -FLargeWorldRenderScalar::GetTileSize());	
		}
		const FMatrix ComponentToWorldMatrix = ComponentTransform.ToMatrixWithScale();
		const FMatrix ComponentToWorld = (bLocalSpace || EmitterInfo.LocalVectorField.bIgnoreComponentTransform) ? FMatrix::Identity : ComponentToWorldMatrix;
		
		FGPUSpriteDynamicEmitterData* DynamicData = new FGPUSpriteDynamicEmitterData(EmitterInfo.RequiredModule);
		DynamicData->FXSystem = FXSystem;
		DynamicData->Resources = EmitterInfo.Resources;
		DynamicData->MaterialProxy = GetCurrentMaterial()->GetRenderProxy();
		DynamicData->bIsMaterialTranslucent = IsTranslucentBlendMode(GetCurrentMaterial()->GetBlendMode());
		DynamicData->Simulation = Simulation;
		DynamicData->SimulationBounds = Template->bUseFixedRelativeBoundingBox ? Template->FixedRelativeBoundingBox.TransformBy(Component->GetComponentTransform()) : Component->Bounds.GetBox();
		DynamicData->SortMode = EmitterInfo.RequiredModule->SortMode;
		DynamicData->bSelected = bSelected;
		DynamicData->bUseLocalSpace = EmitterInfo.RequiredModule->bUseLocalSpace;

		// set up vector field data
		const FMatrix VectorFieldComponentToWorld = (bLocalSpace || EmitterInfo.LocalVectorField.bIgnoreComponentTransform) ? FMatrix::Identity : Component->GetComponentTransform().ToMatrixWithScale();
		const FRotationMatrix VectorFieldTransform(LocalVectorFieldRotation);
		const FMatrix VectorFieldToWorld = VectorFieldTransform * EmitterInfo.LocalVectorField.Transform.ToMatrixWithScale() * VectorFieldComponentToWorld;
		DynamicData->LocalVectorFieldToWorld = VectorFieldToWorld;
		DynamicData->LocalVectorFieldIntensity = EmitterInfo.LocalVectorField.Intensity;
		DynamicData->LocalVectorFieldTightness = EmitterInfo.LocalVectorField.Tightness;	
		DynamicData->bLocalVectorFieldTileX = EmitterInfo.LocalVectorField.bTileX;	
		DynamicData->bLocalVectorFieldTileY = EmitterInfo.LocalVectorField.bTileY;	
		DynamicData->bLocalVectorFieldTileZ = EmitterInfo.LocalVectorField.bTileZ;	
		DynamicData->bLocalVectorFieldUseFixDT = EmitterInfo.LocalVectorField.bUseFixDT;

		// Get LWC tile
		DynamicData->EmitterDynamicParameters.LWCTile = bUseTileOffset ? Component->GetLWCTile() : FVector3f::ZeroVector;

		// Account for LocalToWorld scaling
		FVector ComponentScale = ComponentTransform.GetScale3D();
		// Figure out if we need to replicate the X channel of size to Y.
		const bool bSquare = (EmitterInfo.ScreenAlignment == PSA_Square)
			|| (EmitterInfo.ScreenAlignment == PSA_FacingCameraPosition)
			|| (EmitterInfo.ScreenAlignment == PSA_FacingCameraDistanceBlend);

		DynamicData->EmitterDynamicParameters.LocalToWorldScale.X = ComponentScale.X;
		DynamicData->EmitterDynamicParameters.LocalToWorldScale.Y = (bSquare) ? ComponentScale.X : ComponentScale.Y;

		// Setup axis lock parameters if required.
		const FMatrix& LocalToWorld = ComponentToWorld;
		const EParticleAxisLock LockAxisFlag = (EParticleAxisLock)EmitterInfo.LockAxisFlag;
		DynamicData->EmitterDynamicParameters.AxisLockRight = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		DynamicData->EmitterDynamicParameters.AxisLockUp = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);

		if(LockAxisFlag != EPAL_NONE)
		{
			FVector AxisLockUp, AxisLockRight;
			const FMatrix& AxisLocalToWorld = bLocalSpace ? LocalToWorld : FMatrix::Identity;
			extern void ComputeLockedAxes(EParticleAxisLock, const FMatrix&, FVector&, FVector&);
			ComputeLockedAxes( LockAxisFlag, AxisLocalToWorld, AxisLockUp, AxisLockRight );

			DynamicData->EmitterDynamicParameters.AxisLockRight = (FVector3f)AxisLockRight; // LWC_TODO: precision loss
			DynamicData->EmitterDynamicParameters.AxisLockRight.W = 1.0f;
			DynamicData->EmitterDynamicParameters.AxisLockUp = (FVector3f)AxisLockUp; // LWC_TODO: precision loss
			DynamicData->EmitterDynamicParameters.AxisLockUp.W = 1.0f;
		}

		
		// Setup dynamic color parameter. Only set when using particle parameter distributions.
		FVector4 ColorOverLife(1.0f, 1.0f, 1.0f, 1.0f);
		FVector4 ColorScaleOverLife(1.0f, 1.0f, 1.0f, 1.0f);
		if( EmitterInfo.DynamicColorScale.IsCreated() )
		{
			ColorScaleOverLife = EmitterInfo.DynamicColorScale.GetValue(0.0f,Component);
		}
		if( EmitterInfo.DynamicAlphaScale.IsCreated() )
		{
			ColorScaleOverLife.W = EmitterInfo.DynamicAlphaScale.GetValue(0.0f,Component);
		}

		if( EmitterInfo.DynamicColor.IsCreated() )
		{
			ColorOverLife = EmitterInfo.DynamicColor.GetValue(0.0f,Component);
		}
		if( EmitterInfo.DynamicAlpha.IsCreated() )
		{
			ColorOverLife.W = EmitterInfo.DynamicAlpha.GetValue(0.0f,Component);
		}
		DynamicData->EmitterDynamicParameters.DynamicColor = FVector4f(ColorOverLife * ColorScaleOverLife); // LWC_TODO: precision loss

		DynamicData->MacroUVOverride.bOverride = LODLevel->RequiredModule->bOverrideSystemMacroUV;
		DynamicData->MacroUVOverride.Radius = LODLevel->RequiredModule->MacroUVRadius;
		DynamicData->MacroUVOverride.Position = FVector3f(LODLevel->RequiredModule->MacroUVPosition);	// LWC_TODO: Precision loss

		DynamicData->EmitterDynamicParameters.EmitterInstRandom = EmitterInstRandom;

		const bool bSimulateGPUParticles = 
			FXConsoleVariables::bFreezeGPUSimulation == false &&
			FXConsoleVariables::bFreezeParticleSimulation == false &&
			RHISupportsGPUParticles();

		if (bSimulateGPUParticles)
		{
			float& DeltaSecondsInFix = DynamicData->PerFrameSimulationParameters.DeltaSecondsInFix;
			int32& NumIterationsInFix = DynamicData->PerFrameSimulationParameters.NumIterationsInFix;

			float& DeltaSecondsInVar = DynamicData->PerFrameSimulationParameters.DeltaSecondsInVar;
			int32& NumIterationsInVar = DynamicData->PerFrameSimulationParameters.NumIterationsInVar;
			
			const float FixDeltaSeconds = CVarGPUParticleFixDeltaSeconds.GetValueOnAnyThread();
			const float FixTolerance = CVarGPUParticleFixTolerance.GetValueOnAnyThread();
			const int32 MaxNumIterations = CVarGPUParticleMaxNumIterations.GetValueOnAnyThread();

			DeltaSecondsInFix = FixDeltaSeconds;
			NumIterationsInFix = 0;

			DeltaSecondsInVar = PendingDeltaSeconds + OffsetSeconds;
			NumIterationsInVar = 1;
			OffsetSeconds = 0;

			// If using fixDT strategy
			if (FixDeltaSeconds > 0)
			{
				if (!DynamicData->bLocalVectorFieldUseFixDT)
				{
					// With FixDeltaSeconds > 0, "InFix" is the persistent delta time, while "InVar" is only used for interpolation.
					Swap(DeltaSecondsInFix, DeltaSecondsInVar);
					Swap(NumIterationsInFix, NumIterationsInVar);
				}
				else
				{
					// Move some time from varying DT to fix DT simulation.
					NumIterationsInFix = FMath::FloorToInt(DeltaSecondsInVar / FixDeltaSeconds);
					DeltaSecondsInVar -= NumIterationsInFix * FixDeltaSeconds;

					float SecondsInFix = NumIterationsInFix * FixDeltaSeconds;

					const float RelativeVar = DeltaSecondsInVar / FixDeltaSeconds;

					// If we had some fixed steps, try to move a small value from var dt to fix dt as an optimization (skips on full simulation step)
					if (NumIterationsInFix > 0 && RelativeVar < FixTolerance)
					{
						SecondsInFix += DeltaSecondsInVar;
						DeltaSecondsInVar = 0;
						NumIterationsInVar = 0;
					}
					// Also check if there is almost one full step.
					else if (1.f - RelativeVar < FixTolerance) 
					{
						SecondsInFix += DeltaSecondsInVar;
						NumIterationsInFix += 1;
						DeltaSecondsInVar = 0;
						NumIterationsInVar = 0;
					}
					// Otherwise, transfer a part from the varying time to the fix time. At this point, we know we will have both fix and var iterations.
					// This prevents DT that are multiple of FixDT, from keeping an non zero OffsetSeconds.
					else if (NumIterationsInFix > 0)
					{
						const float TransferedSeconds = FixTolerance * FixDeltaSeconds;
						DeltaSecondsInVar -= TransferedSeconds;
						SecondsInFix += TransferedSeconds;
					}

					if (NumIterationsInFix > 0)
					{
						// Here we limit the iteration count to prevent long frames from taking even longer.
						NumIterationsInFix = FMath::Min<int32>(NumIterationsInFix, MaxNumIterations);
						DeltaSecondsInFix = SecondsInFix / (float)NumIterationsInFix;
					}

					OffsetSeconds = DeltaSecondsInVar;
				}

			#if STATS
				if (NumIterationsInFix + NumIterationsInVar == 1)
				{
					INC_DWORD_STAT_BY(STAT_GPUSingleIterationEmitters, 1);
				}
				else if (NumIterationsInFix + NumIterationsInVar > 1)
				{
					INC_DWORD_STAT_BY(STAT_GPUMultiIterationsEmitters, 1);
				}
			#endif

			}
			
			FVector3f PointAttractorPosition((FVector4f)ComponentToWorld.TransformPosition(EmitterInfo.PointAttractorPosition));	// LWC_TODO: Precision loss
			DynamicData->PerFrameSimulationParameters.PointAttractor = FVector4f(PointAttractorPosition, EmitterInfo.PointAttractorRadiusSq);
			DynamicData->PerFrameSimulationParameters.PositionOffsetAndAttractorStrength = FVector4f(FVector3f(PositionOffsetThisTick), PointAttractorStrength);
			DynamicData->PerFrameSimulationParameters.LocalToWorldScale = DynamicData->EmitterDynamicParameters.LocalToWorldScale;
			DynamicData->PerFrameSimulationParameters.DeltaSeconds = PendingDeltaSeconds; // This value is used when updating vector fields.
			DynamicData->PerFrameSimulationParameters.LWCTile = bUseTileOffset ? Component->GetLWCTile() : FVector3f::ZeroVector;
			Exchange(DynamicData->TilesToClear, TilesToClear);
			Exchange(DynamicData->NewParticles, NewParticles);
		}
		FreeNewParticleArray(NewParticles);
		PendingDeltaSeconds = 0.0f;
		PositionOffsetThisTick = FVector::ZeroVector;

		if (Simulation->bDirty_GameThread)
		{
			Simulation->InitResources(AllocatedTiles, EmitterInfo.Resources);
		}
		check(!Simulation->bReleased_GameThread);
		check(!Simulation->bDestroyed_GameThread);

		return DynamicData;
	}

	/**
	 * Initializes the emitter.
	 */
	virtual void Init() override
	{
		SCOPE_CYCLE_COUNTER(STAT_GPUSpriteEmitterInstance_Init);

		FParticleEmitterInstance::Init();

		if (EmitterInfo.RequiredModule)
		{
			MaxActiveParticles = 0;
			ActiveParticles = 0;
			AllowedLoopCount = EmitterInfo.RequiredModule->EmitterLoops;
		}
		else
		{
			MaxActiveParticles = 0;
			ActiveParticles = 0;
			AllowedLoopCount = 0;
		}

		check(AllocatedTiles.Num() == TileTimeOfDeath.Num());
		FreeParticlesInTile = 0;

		RandomStream.Initialize(Component->RandomStream.FRand());
		EmitterInstRandom = RandomStream.GetFraction();

		FParticleSimulationResources* ParticleSimulationResources = FXSystem->GetParticleSimulationResources();
		const bool bUseTileOffset = EmitterInfo.RequiredModule->bUseLocalSpace == false && EmitterInfo.RequiredModule->bSupportLargeWorldCoordinates;
		ParticleSimulationResources->LWCTile = bUseTileOffset ? Component->GetLWCTile() : FVector3f::ZeroVector;
		const int32 MinTileCount = GetMinTileCount();
		int32 NumAllocated = 0;
		{
			while (AllocatedTiles.Num() < MinTileCount)
			{
				uint32 TileIndex = ParticleSimulationResources->AllocateTile();
				if ( TileIndex != INDEX_NONE )
				{
					AllocatedTiles.Add( TileIndex );
					TileTimeOfDeath.Add( 0.0f );
					NumAllocated++;
				}
				else
				{
					break;
				}
			}
		}
		
#if TRACK_TILE_ALLOCATIONS
		UE_LOG(LogParticles,VeryVerbose,
			TEXT("%s|%s|0x%016x [Init] %d tiles"),
			*Component->GetName(),*Component->Template->GetName(),(PTRINT)this, AllocatedTiles.Num());
#endif // #if TRACK_TILE_ALLOCATIONS

		bool bClearExistingParticles = false;
		UParticleLODLevel* LODLevel = SpriteTemplate->LODLevels[0];
		if (LODLevel)
		{
			UParticleModuleTypeDataGpu* TypeDataModule = CastChecked<UParticleModuleTypeDataGpu>(LODLevel->TypeDataModule);
			bClearExistingParticles = TypeDataModule->bClearExistingParticlesOnInit;
		}

		if (bClearExistingParticles || ActiveTiles.Num() != AllocatedTiles.Num())
		{
			ActiveTiles.Init(false, AllocatedTiles.Num());
			ClearAllocatedTiles();
		}

		Simulation->bDirty_GameThread = true;
		FXSystem->AddGPUSimulation(Simulation);

		CurrentMaterial = EmitterInfo.RequiredModule ? EmitterInfo.RequiredModule->Material : UMaterial::GetDefaultMaterial(MD_Surface);

		InitLocalVectorField();

		WeakWorld = Component->GetWorld();
	}

	FORCENOINLINE void ReserveNewParticles(int32 Num)
	{
		if (Num)
		{
			if (!(NewParticles.Num() + NewParticles.GetSlack()))
			{
				GetNewParticleArray(NewParticles, Num);
			}
			else
			{
				NewParticles.Reserve(Num);
			}
		}
	}

	/**
	 * Simulates the emitter forward by the specified amount of time.
	 */
	virtual void Tick(float DeltaSeconds, bool bSuppressSpawning) override
	{
		FreeNewParticleArray(NewParticles);

		SCOPE_CYCLE_COUNTER(STAT_GPUSpriteTickTime);

		check(AllocatedTiles.Num() == TileTimeOfDeath.Num());

		if (FXConsoleVariables::bFreezeGPUSimulation ||
			FXConsoleVariables::bFreezeParticleSimulation ||
			!RHISupportsGPUParticles())
		{
			return;
		}

		// Grab the current LOD level
		UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();

		// Handle EmitterTime setup, looping, etc.
		float EmitterDelay = Tick_EmitterTimeSetup( DeltaSeconds, LODLevel );

		Simulation->bEnabled = bEnabled;
		if (bEnabled)
		{
			// If the emitter is warming up but any particle spawned now will die
			// anyway, suppress spawning.
			if (Component && Component->bWarmingUp &&
				Component->WarmupTime - SecondsSinceCreation > EmitterInfo.MaxLifetime)
			{
				bSuppressSpawning = true;
			}

			// Mark any tiles with all dead particles as free.
			int32 ActiveTileCount = MarkTilesInactive();

			// Update modules
			Tick_ModuleUpdate(DeltaSeconds, LODLevel);

			// Spawn particles.
			bool bRefreshTiles = false;
			const bool bPreventSpawning = bHaltSpawning || bHaltSpawningExternal || bSuppressSpawning;
			const bool bValidEmitterTime = (EmitterTime >= 0.0f);
			const bool bValidLoop = AllowedLoopCount == 0 || LoopCount < AllowedLoopCount;
			if (!bPreventSpawning && bValidEmitterTime && bValidLoop)
			{
				SCOPE_CYCLE_COUNTER(STAT_GPUSpriteSpawnTime);

				// Determine burst count.
				FSpawnInfo BurstInfo;
				int32 LeftoverBurst = 0;
				{
					float BurstDeltaTime = DeltaSeconds;
					GetCurrentBurstRateOffset(BurstDeltaTime, BurstInfo.Count);

					BurstInfo.Count += ForceBurstSpawnedParticles.Num();

					if (BurstInfo.Count > FXConsoleVariables::MaxGPUParticlesSpawnedPerFrame)
					{
						LeftoverBurst = BurstInfo.Count - FXConsoleVariables::MaxGPUParticlesSpawnedPerFrame;
						BurstInfo.Count = FXConsoleVariables::MaxGPUParticlesSpawnedPerFrame;
					}
				}


				// Determine spawn count based on rate.
				FSpawnInfo SpawnInfo = GetNumParticlesToSpawn(DeltaSeconds);
				SpawnInfo.Count += ForceSpawnedParticles.Num();

				float SpawnRateMult = SpriteTemplate->GetQualityLevelSpawnRateMult();
				SpawnInfo.Count *= SpawnRateMult;
				BurstInfo.Count *= SpawnRateMult;

				int32 FirstBurstParticleIndex = NewParticles.Num();

				ReserveNewParticles(FirstBurstParticleIndex + BurstInfo.Count + SpawnInfo.Count);

				BurstInfo.Count = AllocateTilesForParticles(NewParticles, BurstInfo.Count, ActiveTileCount);

				int32 FirstSpawnParticleIndex = NewParticles.Num();
				SpawnInfo.Count = AllocateTilesForParticles(NewParticles, SpawnInfo.Count, ActiveTileCount);
				SpawnFraction += LeftoverBurst;

				if (BurstInfo.Count > 0)
				{
					// Spawn burst particles.
					BuildNewParticles(NewParticles.GetData() + FirstBurstParticleIndex, BurstInfo, ForceBurstSpawnedParticles);
				}

				if (SpawnInfo.Count > 0)
				{
					// Spawn normal particles.
					BuildNewParticles(NewParticles.GetData() + FirstSpawnParticleIndex, SpawnInfo, ForceSpawnedParticles);
				}

				FreeNewParticleArray(ForceSpawnedParticles);
				FreeNewParticleArray(ForceBurstSpawnedParticles);

				int32 NewParticleCount = BurstInfo.Count + SpawnInfo.Count;
				INC_DWORD_STAT_BY(STAT_GPUSpritesSpawned, NewParticleCount);
	#if STATS
				if (NewParticleCount > FXConsoleVariables::GPUSpawnWarningThreshold)
				{
					UE_LOG(LogParticles,Warning,TEXT("Spawning %d GPU particles in one frame[%d]: %s/%s"),
						NewParticleCount,
						GFrameNumber,
						*SpriteTemplate->GetOuter()->GetName(),
						*SpriteTemplate->EmitterName.ToString()
						);

				}
	#endif

				if (Component && Component->bWarmingUp)
				{
					SimulateWarmupParticles(
						NewParticles.GetData() + (NewParticles.Num() - NewParticleCount),
						NewParticleCount,
						Component->WarmupTime - SecondsSinceCreation );
				}
			}
			else if (bFakeBurstsWhenSpawningSupressed)
			{
				FakeBursts();
			}

			// Free any tiles that we no longer need.
			FreeInactiveTiles();

			// Update current material.
			if (EmitterInfo.RequiredModule->Material)
			{
				CurrentMaterial = EmitterInfo.RequiredModule->Material;
			}

			// Update the local vector field.
			TickLocalVectorField(DeltaSeconds);

			// Look up the strength of the point attractor.
			EmitterInfo.PointAttractorStrength.GetValue(EmitterTime, &PointAttractorStrength);

			// Store the amount of time by which the GPU needs to update the simulation.
			PendingDeltaSeconds = DeltaSeconds;

			// Store the number of active particles.
			ActiveParticles = ActiveTileCount * GParticlesPerTile;
			INC_DWORD_STAT_BY(STAT_GPUSpriteParticles, ActiveParticles);

			// 'Reset' the emitter time so that the delay functions correctly
			EmitterTime += EmitterDelay;

			// Update the bounding box.
			UpdateBoundingBox(DeltaSeconds);

			// Final update for modules.
			Tick_ModuleFinalUpdate(DeltaSeconds, LODLevel);

			// Queue an update to the GPU simulation if needed.
			if (Simulation->bDirty_GameThread)
			{
				Simulation->InitResources(AllocatedTiles, EmitterInfo.Resources);
			}

			CheckEmitterFinished();
		}
		else
		{
			// 'Reset' the emitter time so that the delay functions correctly
			EmitterTime += EmitterDelay;

			FakeBursts();
		}

		check(AllocatedTiles.Num() == TileTimeOfDeath.Num());
	}

	/**
	 * Clears all active particle tiles.
	 */
	void ClearAllocatedTiles()
	{
		TilesToClear.Reset();
		TilesToClear = AllocatedTiles;
		TileToAllocateFrom = INDEX_NONE;
		FreeParticlesInTile = 0;
		ActiveTiles.Init(false,ActiveTiles.Num());
	}

	/**
	 *	Force kill all particles in the emitter.
	 *	@param	bFireEvents		If true, fire events for the particles being killed.
	 */
	virtual void KillParticlesForced(bool bFireEvents) override
	{
		// Clear all active tiles. This will effectively kill all particles.
		ClearAllocatedTiles();

		FreeInactiveTiles();
		// Queue an update to the GPU simulation if needed.
		if (Simulation->bDirty_GameThread)
		{
			Simulation->InitResources(AllocatedTiles, EmitterInfo.Resources);
		}
	}

	/**
	 *	Called when the particle system is deactivating...
	 */
	virtual void OnDeactivateSystem() override
	{
	}

	virtual void Rewind() override
	{
		FParticleEmitterInstance::Rewind();
		InitLocalVectorField();

		FreeInactiveTiles();
		// Queue an update to the GPU simulation if needed.
		if (Simulation->bDirty_GameThread)
		{
			Simulation->InitResources(AllocatedTiles, EmitterInfo.Resources);
		}
	}

	/**
	 * Returns true if the emitter has completed.
	 */
	virtual bool HasCompleted() override
	{
		if ( AllowedLoopCount == 0 || LoopCount < AllowedLoopCount )
		{
			return false;
		}
		return ActiveParticles == 0;
	}

	/**
	 * Force the bounding box to be updated.
	 *		WARNING: This is an expensive operation for GPU particles. It
	 *		requires syncing with the GPU to read back the emitter's bounds.
	 *		This function should NEVER be called at runtime!
	 */
	virtual void ForceUpdateBoundingBox() override
	{
		if ( !GIsEditor )
		{
			UE_LOG(LogParticles, Warning, TEXT("ForceUpdateBoundingBox called on a GPU sprite emitter outside of the Editor!") );
			return;
		}

		ERHIFeatureLevel::Type FeatureLevel = FXSystem->GetFeatureLevel();

		FGPUSpriteParticleEmitterInstance* EmitterInstance = this;
		ENQUEUE_RENDER_COMMAND(FComputeGPUSpriteBoundsCommand)(
			[EmitterInstance, FeatureLevel](FRHICommandListImmediate& RHICmdList)
			{
				EmitterInstance->ParticleBoundingBox = ComputeParticleBounds(
					RHICmdList,
					FeatureLevel,
					EmitterInstance->Simulation->VertexBuffer.VertexBufferSRV,
					EmitterInstance->FXSystem->GetParticleSimulationResources()->GetVisualizeStateTextures().PositionTextureRHI,
					EmitterInstance->Simulation->VertexBuffer.ParticleCount
					);
			});
		FlushRenderingCommands();

		// Take the size of sprites in to account.
		const float MaxSizeX = EmitterInfo.Resources->UniformParameters.MiscScale.X + EmitterInfo.Resources->UniformParameters.MiscBias.X;
		const float MaxSizeY = EmitterInfo.Resources->UniformParameters.MiscScale.Y + EmitterInfo.Resources->UniformParameters.MiscBias.Y;
		const float MaxSize = FMath::Max<float>( MaxSizeX, MaxSizeY );
		ParticleBoundingBox = ParticleBoundingBox.ExpandBy( MaxSize );
	}

private:

	/**
	 * Mark tiles as inactive if all particles in them have died.
	 */
	int32 MarkTilesInactive()
	{
		int32 ActiveTileCount = 0;
		for (TBitArray<>::FConstIterator BitIt(ActiveTiles); BitIt; ++BitIt)
		{
			const int32 BitIndex = BitIt.GetIndex();
			if (TileTimeOfDeath[BitIndex] <= SecondsSinceCreation)
			{
				ActiveTiles.AccessCorrespondingBit(BitIt) = false;
				if ( TileToAllocateFrom == BitIndex )
				{
					TileToAllocateFrom = INDEX_NONE;
					FreeParticlesInTile = 0;
				}
			}
			else
			{
				ActiveTileCount++;
			}
		}
		return ActiveTileCount;
	}

	/**
	 * Initialize the local vector field.
	 */
	void InitLocalVectorField()
	{
		LocalVectorFieldRotation = FMath::LerpRange(
			EmitterInfo.LocalVectorField.MinInitialRotation,
			EmitterInfo.LocalVectorField.MaxInitialRotation,
			RandomStream.GetFraction() );

		// [op] This could cause trouble if the sim destroy render command gets	submitted before we call init, which ends up 
		//		calling this; don't bother initializing the vector field if we're about to obliterate the sim and its resources anyway
		//		FORT-79970
		//
		if (!Simulation->bDestroyed_GameThread)
		{
			FVectorFieldResource* Resource = Simulation->LocalVectorField.Resource.GetReference();
			if (Resource)
			{
			    ENQUEUE_RENDER_COMMAND(FResetVectorFieldCommand)(
				    [Resource](FRHICommandList& RHICmdList)
				    {
						Resource->ResetVectorField();
					});
		    }
		}
	}

	/**
	 * Computes the minimum number of tiles that should be allocated for this emitter.
	 */
	int32 GetMinTileCount()
	{
		if (AllowedLoopCount == 0 && Component->IsActive() && GetCurrentLODLevelChecked()->bEnabled)
		{
			const int32 EstMaxTiles = (EmitterInfo.MaxParticleCount + GParticlesPerTile - 1) / GParticlesPerTile;
			const int32 SlackTiles = FMath::CeilToInt(FXConsoleVariables::ParticleSlackGPU * (float)EstMaxTiles);
			return FMath::Min<int32>(EstMaxTiles + SlackTiles, FXConsoleVariables::MaxParticleTilePreAllocation);
		}
		return 0;
	}

	/**
	 * Release any inactive tiles.
	 * @returns the number of tiles released.
	 */
	int32 FreeInactiveTiles()
	{
		const int32 MinTileCount = GetMinTileCount();
		int32 TilesToFree = 0;
		TBitArray<>::FConstReverseIterator BitIter(ActiveTiles);
		while (BitIter && BitIter.GetIndex() >= MinTileCount)
		{
			if (BitIter.GetValue())
			{
				break;
			}
			++TilesToFree;
			++BitIter;
		}
		if (TilesToFree > 0)
		{
			FParticleSimulationResources* SimulationResources = FXSystem->GetParticleSimulationResources();
			const int32 FirstTileIndex = AllocatedTiles.Num() - TilesToFree;
			const int32 LastTileIndex = FirstTileIndex + TilesToFree;
			for (int32 TileIndex = FirstTileIndex; TileIndex < LastTileIndex; ++TileIndex)
			{
				SimulationResources->FreeTile(AllocatedTiles[TileIndex]);
			}
			ActiveTiles.RemoveAt(FirstTileIndex, TilesToFree);
			AllocatedTiles.RemoveAt(FirstTileIndex, TilesToFree);
			TileTimeOfDeath.RemoveAt(FirstTileIndex, TilesToFree);
			Simulation->bDirty_GameThread = true;
		}
		return TilesToFree;
	}

	/**
	 * Releases resources allocated for GPU simulation.
	 */
	void ReleaseSimulationResources()
	{
		auto GetWorldFXSystem =
			[](TWeakObjectPtr<UWorld> InWeakWorld) -> FFXSystem*
			{
				UWorld* World = InWeakWorld.Get();
				if (World && World->Scene)
				{
					if (FFXSystemInterface* FXSystemInterface = World->Scene->GetFXSystem())
					{
						return static_cast<FFXSystem*>(FXSystemInterface->GetInterface(FFXSystem::Name));
					}
				}
				return nullptr;
			};

		if (FXSystem)
		{
			// There are edge cases where the UWorld that contains the FFXSystem could have been destroyed before us.
			// We therefore see if our World is still valid and that the FFXSystem matches what we cached, if it does
			// not we can not remove the simulation or free the tiles as they don't belong to us.
			FFXSystem* WorldFXSystem = GetWorldFXSystem(WeakWorld);
			if ( WorldFXSystem == FXSystem )
			{
				FXSystem->RemoveGPUSimulation(Simulation);
				if ( FParticleSimulationResources* ParticleSimulationResources = FXSystem->GetParticleSimulationResources() )
				{
					const int32 TileCount = AllocatedTiles.Num();
					for (int32 ActiveTileIndex = 0; ActiveTileIndex < TileCount; ++ActiveTileIndex)
					{
						const uint32 TileIndex = AllocatedTiles[ActiveTileIndex];
						ParticleSimulationResources->FreeTile(TileIndex);
					}
					AllocatedTiles.Reset();
#if TRACK_TILE_ALLOCATIONS
					UE_LOG(LogParticles, VeryVerbose,
						TEXT("%s|%s|0x%016p [ReleaseSimulationResources] %d tiles"),
						*Component->GetName(), *Component->Template->GetName(), (PTRINT)this, AllocatedTiles.Num());
#endif // #if TRACK_TILE_ALLOCATIONS
				}
			}
		}
		else
		{
			if (AllocatedTiles.Num())
			{
				UE_LOG(LogParticles, Warning,
					TEXT("%s|%s|0x%016p [ReleaseSimulationResources] LEAKING %d tiles FXSystem=0x%016x"),
					*Component->GetName(), *Component->Template->GetName(), (PTRINT)this, AllocatedTiles.Num(), (PTRINT)FXSystem);
			}
		}

		ActiveTiles.Reset();
		AllocatedTiles.Reset();
		TileTimeOfDeath.Reset();
		TilesToClear.Reset();
		
		TileToAllocateFrom = INDEX_NONE;
		FreeParticlesInTile = 0;

		Simulation->BeginReleaseResources();
	}

	/**
	 * Allocates space in a particle tile for all new particles.
	 * @param NewParticles - Array in which to store new particles.
	 * @param NumNewParticles - The number of new particles that need an allocation.
	 * @param ActiveTileCount - Number of active tiles, incremented each time a new tile is allocated.
	 * @returns the number of particles which were successfully allocated.
	 */
	int32 AllocateTilesForParticles(TArray<FNewParticle>& InNewParticles, int32 NumNewParticles, int32& ActiveTileCount)
	{
		if (!NumNewParticles)
		{
			return 0;
		}
		// Need to allocate space in tiles for all new particles.
		FParticleSimulationResources* SimulationResources = FXSystem->GetParticleSimulationResources();
		uint32 TileIndex = (AllocatedTiles.IsValidIndex(TileToAllocateFrom)) ? AllocatedTiles[TileToAllocateFrom] : INDEX_NONE;
		FVector2D TileOffset(
			FMath::Fractional((float)TileIndex / (float)GParticleSimulationTileCountX),
			FMath::Fractional(FMath::TruncToFloat((float)TileIndex / (float)GParticleSimulationTileCountX) / (float)GParticleSimulationTileCountY)
			);

		for (int32 ParticleIndex = 0; ParticleIndex < NumNewParticles; ++ParticleIndex)
		{
			if (FreeParticlesInTile <= 0)
			{
				// Start adding particles to the first inactive tile.
				if (ActiveTileCount < AllocatedTiles.Num())
				{
					TileToAllocateFrom = ActiveTiles.FindAndSetFirstZeroBit();
				}
				else
				{
					uint32 NewTile = SimulationResources->AllocateTile();
					if (NewTile == INDEX_NONE)
					{
						// Out of particle tiles.
						UE_LOG(LogParticles,Warning,
							TEXT("Failed to allocate tiles for %s! %d new particles truncated to %d."),
							*Component->Template->GetName(), NumNewParticles, ParticleIndex);
						return ParticleIndex;
					}

					TileToAllocateFrom = AllocatedTiles.Add(NewTile);
					TileTimeOfDeath.Add(0.0f);
					TilesToClear.Add(NewTile);
					ActiveTiles.Add(true);
					Simulation->bDirty_GameThread = true;
				}

				ActiveTileCount++;
				TileIndex = AllocatedTiles[TileToAllocateFrom];
				TileOffset.X = FMath::Fractional((float)TileIndex / (float)GParticleSimulationTileCountX);
				TileOffset.Y = FMath::Fractional(FMath::TruncToFloat((float)TileIndex / (float)GParticleSimulationTileCountX) / (float)GParticleSimulationTileCountY);
				FreeParticlesInTile = GParticlesPerTile;
			}
			FNewParticle& Particle = *new(InNewParticles) FNewParticle();
			const int32 SubTileIndex = GParticlesPerTile - FreeParticlesInTile;
			const int32 SubTileX = SubTileIndex % GParticleSimulationTileSize;
			const int32 SubTileY = SubTileIndex / GParticleSimulationTileSize;
			Particle.Offset.X = TileOffset.X + ((float)SubTileX / (float)GParticleSimulationTextureSizeX);
			Particle.Offset.Y = TileOffset.Y + ((float)SubTileY / (float)GParticleSimulationTextureSizeY);
			Particle.ResilienceAndTileIndex.AllocatedTileIndex = TileToAllocateFrom;
			FreeParticlesInTile--;
		}

		return NumNewParticles;
	}

	/**
	 * Computes how many particles should be spawned this frame. Does not account for bursts.
	 * @param DeltaSeconds - The amount of time for which to spawn.
	 */
	FSpawnInfo GetNumParticlesToSpawn(float DeltaSeconds)
	{
		UParticleModuleRequired* RequiredModule = EmitterInfo.RequiredModule;
		UParticleModuleSpawn* SpawnModule = EmitterInfo.SpawnModule;

		// Determine spawn rate.
		check(SpawnModule && RequiredModule);
		const float RateScale = CurrentLODLevel->SpawnModule->RateScale.GetValue(EmitterTime, Component) * CurrentLODLevel->SpawnModule->GetGlobalRateScale();
		float SpawnRate = CurrentLODLevel->SpawnModule->Rate.GetValue(EmitterTime, Component) * RateScale;
		SpawnRate = FMath::Max<float>(0.0f, SpawnRate);

		if (EmitterInfo.SpawnPerUnitModule)
		{
			int32 Number = 0;
			float Rate = 0.0f;
			if (EmitterInfo.SpawnPerUnitModule->GetSpawnAmount(this, 0, 0.0f, DeltaSeconds, Number, Rate) == false)
			{
				SpawnRate = Rate;
			}
			else
			{
				SpawnRate += Rate;
			}
		}

		// Determine how many to spawn.
		FSpawnInfo Info;
		float AccumSpawnCount = SpawnFraction + SpawnRate * DeltaSeconds;
		Info.Count = FMath::Min(FMath::TruncToInt(AccumSpawnCount), FXConsoleVariables::MaxGPUParticlesSpawnedPerFrame);
		Info.Increment = (SpawnRate > 0.0f) ? (1.f / SpawnRate) : 0.0f;
		Info.StartTime = DeltaSeconds + SpawnFraction * Info.Increment - Info.Increment;
		SpawnFraction = AccumSpawnCount - Info.Count;

		return Info;
	}

	/**
	 * Perform a simple simulation for particles during the warmup period. This
	 * Simulation only takes in to account constant acceleration, initial
	 * velocity, and initial position.
	 * @param InNewParticles - The first new particle to simulate.
	 * @param ParticleCount - The number of particles to simulate.
	 * @param WarmupTime - The amount of warmup time by which to simulate.
	 */
	void SimulateWarmupParticles(
		FNewParticle* InNewParticles,
		int32 ParticleCount,
		float WarmupTime )
	{
		const FVector Acceleration = EmitterInfo.ConstantAcceleration;
		for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex)
		{
			FNewParticle* Particle = InNewParticles + ParticleIndex;
			Particle->Position += (Particle->Velocity + 0.5f * FVector3f(Acceleration) * WarmupTime) * WarmupTime;
			Particle->Velocity += FVector3f(Acceleration) * WarmupTime;
			Particle->RelativeTime += Particle->TimeScale * WarmupTime;
		}
	}

	/**
	 * Builds new particles to be injected in to the GPU simulation.
	 * @param OutNewParticles - Array in which to store new particles.
	 * @param SpawnCount - The number of particles to spawn.
	 * @param SpawnTime - The time at which to begin spawning particles.
	 * @param Increment - The amount by which to increment time for each particle spawned.
	 */
	void BuildNewParticles(FNewParticle* InNewParticles, FSpawnInfo SpawnInfo, TArray<FNewParticle> &ForceSpawned)
	{
		const float OneOverTwoPi = 1.0f / (2.0f * UE_PI);
		UParticleModuleRequired* RequiredModule = EmitterInfo.RequiredModule;

		// Allocate stack memory for a dummy particle.
		const UPTRINT Alignment = 16;
		uint8* Mem = (uint8*)FMemory_Alloca( ParticleSize + (int32)Alignment );
		Mem += Alignment - 1;
		Mem = (uint8*)(UPTRINT(Mem) & ~(Alignment - 1));

		FBaseParticle* TempParticle = (FBaseParticle*)Mem;


		// Figure out if we need to replicate the X channel of size to Y.
		const bool bSquare = (EmitterInfo.ScreenAlignment == PSA_Square)
			|| (EmitterInfo.ScreenAlignment == PSA_FacingCameraPosition)
			|| (EmitterInfo.ScreenAlignment == PSA_FacingCameraDistanceBlend);

		// Compute the distance covered by the emitter during this time period.
		const FVector EmitterLocation = (RequiredModule->bUseLocalSpace) ? FVector::ZeroVector : Location;
		const FVector EmitterDelta = (RequiredModule->bUseLocalSpace) ? FVector::ZeroVector : (OldLocation - Location);

		// Construct new particles.
		for (int32 i = SpawnInfo.Count; i > 0; --i)
		{
			// Reset the temporary particle.
			FMemory::Memzero( TempParticle, ParticleSize );

			// Set the particle's location and invoke each spawn module on the particle.
			TempParticle->Location = EmitterToSimulation.GetOrigin();

			int32 ForceSpawnedOffset = SpawnInfo.Count - ForceSpawned.Num();
			if (ForceSpawned.Num() && i > ForceSpawnedOffset)
			{
				TempParticle->Location = FVector(ForceSpawned[i - ForceSpawnedOffset - 1].Position);
				TempParticle->RelativeTime = ForceSpawned[i - ForceSpawnedOffset - 1].RelativeTime;
				TempParticle->Velocity += ForceSpawned[i - ForceSpawnedOffset - 1].Velocity;
			}

			for (int32 ModuleIndex = 0; ModuleIndex < EmitterInfo.SpawnModules.Num(); ModuleIndex++)
			{
				UParticleModule* SpawnModule = EmitterInfo.SpawnModules[ModuleIndex];
				if (SpawnModule->bEnabled)
				{
					SpawnModule->Spawn(this, /*Offset=*/ 0, SpawnInfo.StartTime, TempParticle);
				}
			}

			const float RandomOrbit = RandomStream.GetFraction();
			FNewParticle* NewParticle = InNewParticles++;
			int32 AllocatedTileIndex = NewParticle->ResilienceAndTileIndex.AllocatedTileIndex;
			float InterpFraction = (float)i / (float)SpawnInfo.Count;

			NewParticle->Velocity = TempParticle->BaseVelocity;
			FVector WSPosition = TempParticle->Location + InterpFraction * EmitterDelta + SpawnInfo.StartTime * (FVector)NewParticle->Velocity + EmitterInfo.OrbitOffsetBase + EmitterInfo.OrbitOffsetRange * RandomOrbit;
			if (RequiredModule->bUseLocalSpace == false && RequiredModule->bSupportLargeWorldCoordinates)
			{
				NewParticle->Position = FVector3f(WSPosition - FLargeWorldRenderScalar::GetTileSize() * FVector(Component->GetLWCTile()));
			}
			else
			{
				NewParticle->Position = (FVector3f)WSPosition;
			}
			NewParticle->RelativeTime = TempParticle->RelativeTime;
			NewParticle->TimeScale = FMath::Max<float>(TempParticle->OneOverMaxLifetime, 0.001f);

			//So here I'm reducing the size to 0-0.5 range and using < 0.5 to indicate flipped UVs.
			FVector BaseSize = (FVector)GetParticleBaseSize(*TempParticle, true);
			FVector2D UVFlipSizeOffset = FVector2D(BaseSize.X < 0.0f ? 0.0f : 0.5f, BaseSize.Y < 0.0f ? 0.0f : 0.5f);
			NewParticle->Size.X = (FMath::Abs(BaseSize.X) * EmitterInfo.InvMaxSize.X * 0.5f);
			NewParticle->Size.Y = bSquare ? (NewParticle->Size.X) : (FMath::Abs(BaseSize.Y) * EmitterInfo.InvMaxSize.Y * 0.5f);
			NewParticle->Size += FVector2f(UVFlipSizeOffset);

			NewParticle->Rotation = FMath::Fractional( TempParticle->Rotation * OneOverTwoPi );
			NewParticle->RelativeRotationRate = TempParticle->BaseRotationRate * OneOverTwoPi * EmitterInfo.InvRotationRateScale / NewParticle->TimeScale;
			NewParticle->RandomOrbit = RandomOrbit;
			EmitterInfo.VectorFieldScale.GetRandomValue(EmitterTime, &NewParticle->VectorFieldScale, RandomStream);
			EmitterInfo.DragCoefficient.GetRandomValue(EmitterTime, &NewParticle->DragCoefficient, RandomStream);
			EmitterInfo.Resilience.GetRandomValue(EmitterTime, &NewParticle->ResilienceAndTileIndex.Resilience, RandomStream);
			SpawnInfo.StartTime -= SpawnInfo.Increment;

			const float PrevTileTimeOfDeath = TileTimeOfDeath[AllocatedTileIndex];
			const float ParticleTimeOfDeath = SecondsSinceCreation + 1.0f / NewParticle->TimeScale;
			const float NewTileTimeOfDeath = FMath::Max(PrevTileTimeOfDeath, ParticleTimeOfDeath);
			TileTimeOfDeath[AllocatedTileIndex] = NewTileTimeOfDeath;
		}
	}

	/**
	 * Update the local vector field.
	 * @param DeltaSeconds - The amount of time by which to move forward simulation.
	 */
	void TickLocalVectorField(float DeltaSeconds)
	{
		LocalVectorFieldRotation += EmitterInfo.LocalVectorField.RotationRate * DeltaSeconds;
	}

	virtual void UpdateBoundingBox(float DeltaSeconds) override
	{
		// Setup a bogus bounding box at the origin. GPU emitters must use fixed bounds.
		FVector Origin = Component ? Component->GetComponentToWorld().GetTranslation() : FVector::ZeroVector;
		ParticleBoundingBox = FBox::BuildAABB(Origin, FVector(1.0f));
	}

	virtual bool Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount = true) override
	{
		return false;
	}

	virtual float Tick_SpawnParticles(float DeltaTime, UParticleLODLevel* InCurrentLODLevel, bool bSuppressSpawning, bool bFirstTime) override
	{
		return 0.0f;
	}

	virtual void Tick_ModulePreUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
	{
	}

	virtual void Tick_ModuleUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel) override
	{
		// We cannot update particles that have spawned, but modules such as BoneSocket and Skel Vert/Surface may need to perform calculations each tick.
		UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels[0];
		check(HighestLODLevel);
		for (int32 ModuleIndex = 0; ModuleIndex < InCurrentLODLevel->UpdateModules.Num(); ModuleIndex++)
		{
			UParticleModule* CurrentModule	= InCurrentLODLevel->UpdateModules[ModuleIndex];
			if (CurrentModule && CurrentModule->bEnabled && CurrentModule->bUpdateModule && CurrentModule->bUpdateForGPUEmitter)
			{
				CurrentModule->Update(this, GetModuleDataOffset(HighestLODLevel->UpdateModules[ModuleIndex]), DeltaTime);
			}
		}
	}

	virtual void Tick_ModulePostUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel) override
	{
	}

	virtual void Tick_ModuleFinalUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel) override
	{
		// We cannot update particles that have spawned, but modules such as BoneSocket and Skel Vert/Surface may need to perform calculations each tick.
		UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels[0];
		check(HighestLODLevel);
		for (int32 ModuleIndex = 0; ModuleIndex < InCurrentLODLevel->UpdateModules.Num(); ModuleIndex++)
		{
			UParticleModule* CurrentModule	= InCurrentLODLevel->UpdateModules[ModuleIndex];
			if (CurrentModule && CurrentModule->bEnabled && CurrentModule->bFinalUpdateModule && CurrentModule->bUpdateForGPUEmitter)
			{
				CurrentModule->FinalUpdate(this, GetModuleDataOffset(HighestLODLevel->UpdateModules[ModuleIndex]), DeltaTime);
			}
		}
	}

	virtual void SetCurrentLODIndex(int32 InLODIndex, bool bInFullyProcess) override
	{
		bool bDifferent = (InLODIndex != CurrentLODLevelIndex);
		FParticleEmitterInstance::SetCurrentLODIndex(InLODIndex, bInFullyProcess);
	}

	virtual uint32 RequiredBytes() override
	{
		return 0;
	}

	virtual uint8* GetTypeDataModuleInstanceData() override
	{
		return NULL;
	}

	virtual uint32 CalculateParticleStride(uint32 InParticleSize) override
	{
		return InParticleSize;
	}

	virtual void ResetParticleParameters(float DeltaTime) override
	{
	}

	void CalculateOrbitOffset(FOrbitChainModuleInstancePayload& Payload, 
		FVector& AccumOffset, FVector& AccumRotation, FVector& AccumRotationRate, 
		float DeltaTime, FVector& Result, FMatrix& RotationMat)
	{
	}

	virtual void UpdateOrbitData(float DeltaTime) override
	{

	}

	virtual void ParticlePrefetch() override
	{
	}

	virtual float Spawn(float DeltaTime) override
	{
		return 0.0f;
	}

	virtual void ForceSpawn(float DeltaTime, int32 InSpawnCount, int32 InBurstCount, FVector& InLocation, FVector& InVelocity) override
	{
		const bool bUseLocalSpace = GetCurrentLODLevelChecked()->RequiredModule->bUseLocalSpace;
		FVector SpawnLocation = bUseLocalSpace ? FVector::ZeroVector : InLocation;

		float Increment = DeltaTime / InSpawnCount;
		if (InSpawnCount && !(ForceSpawnedParticles.Num() + ForceSpawnedParticles.GetSlack()))
		{
			GetNewParticleArray(ForceSpawnedParticles, InSpawnCount);
		}
		for (int32 i = 0; i < InSpawnCount; i++)
		{

			FNewParticle Particle;
			Particle.Position = (FVector3f)SpawnLocation;
			Particle.Velocity = (FVector3f)InVelocity;
			Particle.RelativeTime = Increment*i;
			ForceSpawnedParticles.Add(Particle);
		}
		if (InBurstCount && !(ForceBurstSpawnedParticles.Num() + ForceBurstSpawnedParticles.GetSlack()))
		{
			GetNewParticleArray(ForceBurstSpawnedParticles, InBurstCount);
		}
		for (int32 i = 0; i < InBurstCount; i++)
		{
			FNewParticle Particle;
			Particle.Position = (FVector3f)SpawnLocation;
			Particle.Velocity = (FVector3f)InVelocity;
			Particle.RelativeTime = 0.0f;
			ForceBurstSpawnedParticles.Add(Particle);
		}
	}

	virtual void PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity) override
	{
	}

	virtual void PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime) override
	{
	}

	virtual void KillParticles() override
	{
	}

	virtual void KillParticle(int32 Index) override
	{
	}

	virtual void RemovedFromScene()
	{
	}

	virtual FBaseParticle* GetParticle(int32 Index) override
	{
		return NULL;
	}

	virtual FBaseParticle* GetParticleDirect(int32 InDirectIndex) override
	{
		return NULL;
	}

protected:

	virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutData) override
	{
		return true;
	}
};

#if TRACK_TILE_ALLOCATIONS
void DumpTileAllocations()
{
	for (TMap<FFXSystem*,TSet<FGPUSpriteParticleEmitterInstance*> >::TConstIterator It(GPUSpriteParticleEmitterInstances); It; ++It)
	{
		FFXSystem* FXSystem = It.Key();
		const TSet<FGPUSpriteParticleEmitterInstance*>& Emitters = It.Value();
		int32 TotalAllocatedTiles = 0;

		UE_LOG(LogParticles,Display,TEXT("---------- GPU Particle Tile Allocations : FXSystem=0x%016x ----------"), (PTRINT)FXSystem);
		for (TSet<FGPUSpriteParticleEmitterInstance*>::TConstIterator It(Emitters); It; ++It)
		{
			FGPUSpriteParticleEmitterInstance* Emitter = *It;
			int32 TileCount = Emitter->GetAllocatedTileCount();
			UE_LOG(LogParticles,Display,
				TEXT("%s|%s|0x%016x %d tiles"),
				*Emitter->Component->GetName(),*Emitter->Component->Template->GetName(),(PTRINT)Emitter, TileCount);
			TotalAllocatedTiles += TileCount;
		}

		UE_LOG(LogParticles,Display,TEXT("---"));
		UE_LOG(LogParticles,Display,TEXT("Total Allocated: %d"), TotalAllocatedTiles);
		UE_LOG(LogParticles,Display,TEXT("Free (est.): %d"), GParticleSimulationTileCount - TotalAllocatedTiles);
		if (FXSystem)
		{
			UE_LOG(LogParticles,Display,TEXT("Free (actual): %d"), FXSystem->GetParticleSimulationResources()->GetFreeTileCount());
			UE_LOG(LogParticles,Display,TEXT("Leaked: %d"), GParticleSimulationTileCount - TotalAllocatedTiles - FXSystem->GetParticleSimulationResources()->GetFreeTileCount());
		}
	}
}

FAutoConsoleCommand DumpTileAllocsCommand(
	TEXT("FX.DumpTileAllocations"),
	TEXT("Dump GPU particle tile allocations."),
	FConsoleCommandDelegate::CreateStatic(DumpTileAllocations)
	);
#endif // #if TRACK_TILE_ALLOCATIONS

/*-----------------------------------------------------------------------------
	Internal interface.
-----------------------------------------------------------------------------*/

void FFXSystem::InitGPUSimulation()
{
	LLM_SCOPE(ELLMTag::Particles);

	check(ParticleSimulationResources == NULL);
	ensure(GParticleSimulationTextureSizeX > 0 && GParticleSimulationTextureSizeY > 0);
	/** How many tiles are in the simulation textures. */
	GParticleSimulationTileCountX = GParticleSimulationTextureSizeX / GParticleSimulationTileSize;
	GParticleSimulationTileCountY = GParticleSimulationTextureSizeY / GParticleSimulationTileSize;
	GParticleSimulationTileCount = GParticleSimulationTileCountX * GParticleSimulationTileCountY;

	ParticleSimulationResources = new FParticleSimulationResources();

	InitGPUResources();
}

void FFXSystem::DestroyGPUSimulation()
{
	UE_LOG(LogParticles,Verbose,
		TEXT("Destroying %d GPU particle simulations for FXSystem 0x%p"),
		GPUSimulations.Num(),
		this
		);
	for ( TSparseArray<FParticleSimulationGPU*>::TIterator It(GPUSimulations); It; ++It )
	{
		FParticleSimulationGPU* Simulation = *It;
		Simulation->SimulationIndex = INDEX_NONE;
	}
	GPUSimulations.Empty();
	for (int32 PhaseIndex = 0; PhaseIndex < UE_ARRAY_COUNT(NumGPUSimulations); PhaseIndex++)
	{
		NumGPUSimulations[PhaseIndex] = 0;
	}
	ReleaseGPUResources();
	ParticleSimulationResources->Destroy();
	ParticleSimulationResources = NULL;
}

void FFXSystem::InitGPUResources()
{
	if (RHISupportsGPUParticles())
	{
		check(ParticleSimulationResources);
		ParticleSimulationResources->Init();
	}
}

void FFXSystem::ReleaseGPUResources()
{
	if (RHISupportsGPUParticles())
	{
		if(ParticleSimulationResources)
		{
			ParticleSimulationResources->Release();
		}
	}
}

void FFXSystem::AddGPUSimulation(FParticleSimulationGPU* Simulation)
{
	if (!IsPendingKill())
	{
		LLM_SCOPE(ELLMTag::Particles);

		FFXSystem* FXSystem = this;
		ENQUEUE_RENDER_COMMAND(FAddGPUSimulationCommand)(
			[FXSystem, Simulation](FRHICommandListImmediate& RHICmdList)
		{
			if (Simulation->SimulationIndex == INDEX_NONE)
			{
				FSparseArrayAllocationInfo Allocation = FXSystem->GPUSimulations.AddUninitialized();
				Simulation->SimulationIndex = Allocation.Index;
				FXSystem->GPUSimulations[Allocation.Index] = Simulation;
				FXSystem->NumGPUSimulations[Simulation->SimulationPhase]++;
			}
			check(FXSystem->GPUSimulations[Simulation->SimulationIndex] == Simulation);
		});
	}
}

void FFXSystem::RemoveGPUSimulation(FParticleSimulationGPU* Simulation)
{
	if (!IsPendingKill())
	{
		FFXSystem* FXSystem = this;
		ENQUEUE_RENDER_COMMAND(FRemoveGPUSimulationCommand)(
			[FXSystem, Simulation](FRHICommandListImmediate& RHICmdList)
		{
			if (Simulation->SimulationIndex != INDEX_NONE)
			{
				check(FXSystem->GPUSimulations[Simulation->SimulationIndex] == Simulation);
				FXSystem->GPUSimulations.RemoveAt(Simulation->SimulationIndex);
				FXSystem->NumGPUSimulations[Simulation->SimulationPhase]--;
			}
			Simulation->SimulationIndex = INDEX_NONE;
		});
	}
}

bool FFXSystem::AddSortedGPUSimulation(FParticleSimulationGPU* Simulation, const FVector& ViewOrigin, bool bIsTranslucent, FGPUSortManager::FAllocationInfo& OutInfo)
{
	LLM_SCOPE(ELLMTag::Particles);

	const EGPUSortFlags SortFlags = 
		EGPUSortFlags::KeyGenAfterPostRenderOpaque |
		EGPUSortFlags::ValuesAsG16R16F | 
		EGPUSortFlags::LowPrecisionKeys | 
		EGPUSortFlags::SortAfterPostRenderOpaque;

	// Currently opaque materials would need SortAfterPreRender but this is incompatible with KeyGenAfterPostRenderOpaque
	if (bIsTranslucent && GPUSortManager && GPUSortManager->AddTask(OutInfo, Simulation->VertexBuffer.ParticleCount, SortFlags))
	{
		SimulationsToSort.Emplace(Simulation->VertexBuffer.VertexBufferSRV, ViewOrigin, (uint32)Simulation->VertexBuffer.ParticleCount, OutInfo);
		return true;
	}
	else
	{
		return false;
	}
}

void FFXSystem::GenerateSortKeys(FRHICommandListImmediate& RHICmdList, int32 BatchId, int32 NumElementsInBatch, EGPUSortFlags Flags, FRHIUnorderedAccessView* KeysUAV, FRHIUnorderedAccessView* ValuesUAV)
{
	check(EnumHasAnyFlags(Flags, EGPUSortFlags::KeyGenAfterPostRenderOpaque));

	// Cascade does not support high precision keys so we can safely ignore this as we will not have a batch which conatins anything but low precision
	if (EnumHasAnyFlags(Flags, EGPUSortFlags::LowPrecisionKeys) == false)
	{
		return;
	}

	// First generate keys for each emitter to be sorted.
	const int32 TotalParticleCount = GenerateParticleSortKeys(
		RHICmdList,
		KeysUAV,
		ValuesUAV,
		ParticleSimulationResources->GetVisualizeStateTextures().PositionTextureRHI,
		SimulationsToSort,
		FeatureLevel,
		BatchId);
}

void FFXSystem::AdvanceGPUParticleFrame(bool bAllowGPUParticleUpdate)
{
	if (bAllowGPUParticleUpdate)
	{
		// We double buffer, so swap the current and previous textures.
		ParticleSimulationResources->FrameIndex ^= 0x1;
	}

	// Reset the list of sorted simulations. As PreRenderView is called on GPU simulations we'll
	// allocate space for them in the sorted index buffer.
	SimulationsToSort.Reset();
}

/**
 * Sets parameters for the vector field instance.
 * @param OutParameters - The uniform parameters structure.
 * @param VectorFieldInstance - The vector field instance.
 * @param EmitterScale - Amount to scale the vector field by.
 * @param EmitterTightness - Tightness override for the vector field.
 * @param Index - Index of the vector field.
 */
static void SetParametersForVectorField(FVectorFieldUniformParameters& OutParameters, FVectorFieldInstance* VectorFieldInstance, float EmitterScale, float EmitterTightness, int32 Index)
{
	check(VectorFieldInstance && VectorFieldInstance->Resource);
	check(Index < MAX_VECTOR_FIELDS);

	FVectorFieldResource* Resource = VectorFieldInstance->Resource.GetReference();
	if (Resource)
	{
		const float Intensity = VectorFieldInstance->Intensity * Resource->Intensity * EmitterScale;

		// Override vector field tightness if value is set (greater than 0). This override is only used for global vector fields.
		float Tightness = EmitterTightness;
		if(EmitterTightness == -1)
		{
			Tightness = FMath::Clamp<float>(VectorFieldInstance->Tightness, 0.0f, 1.0f);
		}

		OutParameters.WorldToVolume[Index] = FMatrix44f(VectorFieldInstance->WorldToVolume);		// LWC_TODO: Precision loss
		OutParameters.VolumeToWorld[Index] = FMatrix44f(VectorFieldInstance->VolumeToWorldNoScale);
		OutParameters.VolumeSize[Index] = FVector4f(Resource->SizeX, Resource->SizeY, Resource->SizeZ, 0);
		OutParameters.IntensityAndTightness[Index] = FVector4f(Intensity, Tightness, 0, 0 );
		OutParameters.TilingAxes[Index].X = VectorFieldInstance->bTileX ? 1.0f : 0.0f;
		OutParameters.TilingAxes[Index].Y = VectorFieldInstance->bTileY ? 1.0f : 0.0f;
		OutParameters.TilingAxes[Index].Z = VectorFieldInstance->bTileZ ? 1.0f : 0.0f;
	}
}

bool FFXSystem::UsesGlobalDistanceFieldInternal() const
{
	for (TSparseArray<FParticleSimulationGPU*>::TConstIterator It(GPUSimulations); It; ++It)
	{
		const FParticleSimulationGPU* Simulation = *It;

		if (Simulation->SimulationPhase == EParticleSimulatePhase::CollisionDistanceField
			&& Simulation->TileVertexBuffer.AlignedTileCount > 0)
		{
			return true;
		}
	}

	return false;
}

bool FFXSystem::UsesDepthBufferInternal() const
{
	for (TSparseArray<FParticleSimulationGPU*>::TConstIterator It(GPUSimulations); It; ++It)
	{
		const FParticleSimulationGPU* Simulation = *It;

		if (Simulation->SimulationPhase == EParticleSimulatePhase::CollisionDepthBuffer
			&& Simulation->TileVertexBuffer.AlignedTileCount > 0)
		{
			return true;
		}
	}

	return false;
}

bool FFXSystem::RequiresEarlyViewUniformBufferInternal() const
{
	return false;
}

bool FFXSystem::RequiresRayTracingSceneInternal() const
{
	return false;
}

void FFXSystem::PrepareGPUSimulation(FRHICommandListImmediate& RHICmdList)
{
	// Grab resources.
	FParticleStateTextures& CurrentStateTextures = ParticleSimulationResources->GetCurrentStateTextures();

	// We delay this transition in AFR to coincide with the first time the simulation
	// texture is actually used.
	if (GNumAlternateFrameRenderingGroups == 1)
	{
		// Setup render states.
		RHICmdList.Transition({
			FRHITransitionInfo(CurrentStateTextures.PositionTextureRHI, ERHIAccess::SRVMask, ERHIAccess::RTV),
			FRHITransitionInfo(CurrentStateTextures.VelocityTextureRHI, ERHIAccess::SRVMask, ERHIAccess::RTV)
		});
	}
}

void FFXSystem::FinalizeGPUSimulation(FRHICommandListImmediate& RHICmdList)
{
}

void FFXSystem::SimulateGPUParticles(
	FRHICommandListImmediate& RHICmdList,
	EParticleSimulatePhase::Type Phase,
	FRHIUniformBuffer* ViewUniformBuffer,
	const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData
	)
{
	LLM_SCOPE(ELLMTag::Particles);
	check(IsInRenderingThread());
	SCOPE_CYCLE_COUNTER(STAT_GPUParticleTickTime);

	const float FixDeltaSeconds = CVarGPUParticleFixDeltaSeconds.GetValueOnRenderThread();

	// Grab resources.
	FParticleStateTextures& CurrentStateTextures = ParticleSimulationResources->GetCurrentStateTextures();
	FParticleStateTextures& PrevStateTextures = ParticleSimulationResources->GetPreviousStateTextures();	

	// Setup render states.
	FRHITexture* CurrentStateRenderTargets[2] = { CurrentStateTextures.PositionTextureRHI, CurrentStateTextures.VelocityTextureRHI };
	FRHITexture* PreviousStateRenderTargets[2] = { PrevStateTextures.PositionTextureRHI, PrevStateTextures.VelocityTextureRHI };


#if WITH_MGPU
	static const FName TemporalEffectName("SimulateGPUParticles");
	TArray<FRHITexture*, TFixedAllocator<4>> TemporalEffectTextures;
	if (GNumAlternateFrameRenderingGroups > 1)
	{
		if (Phase == PhaseToWaitForTemporalEffect)
		{
			SCOPED_GPU_STAT(RHICmdList, AFRWaitForParticleSimulation);
			RHICmdList.WaitForTemporalEffect(TemporalEffectName);

			// Only the previous state textures are actually being copied, but due to the data
			// race mentioned in the BroadcastTemporalEffect block below we need to delay
			// transitioning the current textures to writable state as well.
			RHICmdList.Transition({
				FRHITransitionInfo(CurrentStateRenderTargets[0], ERHIAccess::Unknown, ERHIAccess::RTV),
				FRHITransitionInfo(CurrentStateRenderTargets[1], ERHIAccess::Unknown, ERHIAccess::RTV)});
		}
		TemporalEffectTextures.Add(CurrentStateRenderTargets[0]);
		TemporalEffectTextures.Add(CurrentStateRenderTargets[1]);
	}

	TArray<FTransferResourceParams, TFixedAllocator<4>> CrossGPUTransferResources;
	const bool bCrossTransferEnabled = GNumAlternateFrameRenderingGroups == 1 && GNumExplicitGPUsForRendering > 1;
	auto AddCrossGPUTransferResource =
		[&](FRHITexture* TextureToTransfer)
		{
			const bool bPullData = false;
			const bool bLockStep = false;

			const FRHIGPUMask GPUMask = RHICmdList.GetGPUMask();
			for (uint32 GPUIndex : FRHIGPUMask::All())
			{
				if (!GPUMask.Contains(GPUIndex))
				{
					CrossGPUTransferResources.Emplace(TextureToTransfer, GPUMask.GetFirstIndex(), GPUIndex, bPullData, bLockStep);
				}
			}
		};
	if (bCrossTransferEnabled)
	{
		AddCrossGPUTransferResource(CurrentStateRenderTargets[0]);
		AddCrossGPUTransferResource(CurrentStateRenderTargets[1]);
	}
#endif

	{
		
		// On some platforms, the textures are filled with garbage after creation, so we need to clear them to black the first time we use them
		if ( !CurrentStateTextures.bTexturesCleared )
		{
			
			RHICmdList.BeginUpdateMultiFrameResource(CurrentStateRenderTargets[0]);
			RHICmdList.BeginUpdateMultiFrameResource(CurrentStateRenderTargets[1]);

			{
				FRHIRenderPassInfo RenderPassInfo(2, CurrentStateRenderTargets, ERenderTargetActions::Clear_Store);
				RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("GPUParticlesClearStateTextures"));
				RHICmdList.EndRenderPass();
			}
			
			CurrentStateTextures.bTexturesCleared = true;
			
			RHICmdList.EndUpdateMultiFrameResource(CurrentStateRenderTargets[0]);
			RHICmdList.EndUpdateMultiFrameResource(CurrentStateRenderTargets[1]);
		}
		
		if ( !PrevStateTextures.bTexturesCleared )
		{
			RHICmdList.BeginUpdateMultiFrameResource(PreviousStateRenderTargets[0]);
			RHICmdList.BeginUpdateMultiFrameResource(PreviousStateRenderTargets[1]);

			{
				RHICmdList.Transition({
					FRHITransitionInfo(PreviousStateRenderTargets[0], ERHIAccess::SRVMask, ERHIAccess::RTV),
					FRHITransitionInfo(PreviousStateRenderTargets[1], ERHIAccess::SRVMask, ERHIAccess::RTV)});

				FRHIRenderPassInfo RPInfo(2, PreviousStateRenderTargets, ERenderTargetActions::Clear_Store);
				RHICmdList.BeginRenderPass(RPInfo, TEXT("GPUParticlesClearPreviousStateTextures"));
				RHICmdList.EndRenderPass();

				RHICmdList.Transition({
					FRHITransitionInfo(PreviousStateRenderTargets[0], ERHIAccess::RTV, ERHIAccess::SRVMask),
					FRHITransitionInfo(PreviousStateRenderTargets[1], ERHIAccess::RTV, ERHIAccess::SRVMask) });
			}
			
			PrevStateTextures.bTexturesCleared = true;
			
			RHICmdList.EndUpdateMultiFrameResource(PreviousStateRenderTargets[0]);
			RHICmdList.EndUpdateMultiFrameResource(PreviousStateRenderTargets[1]);
		}
	}
	
	// Simulations that don't use vector fields can share some state.
	FVectorFieldUniformBufferRef EmptyVectorFieldUniformBuffer;
	{
		// We would like to perform a null check on the iterator for GPU simulations
		// to avoid usage of CreateUniformBufferImmediate() when we don't need it.
		// TGD shows a large ~15ms gap when this function is called when we don't have 
		// any particles to simulate.
		if (GPUSimulations.Num() > 0)
		{
			FVectorFieldUniformParameters VectorFieldParameters;
			FRHITexture3D* BlackVolumeTextureRHI = (FRHITexture3D*)(FRHITexture*)GBlackVolumeTexture->TextureRHI;
			for (int32 Index = 0; Index < MAX_VECTOR_FIELDS; ++Index)
			{
				VectorFieldParameters.WorldToVolume[Index] = FMatrix44f::Identity;
				VectorFieldParameters.VolumeToWorld[Index] = FMatrix44f::Identity;
				VectorFieldParameters.VolumeSize[Index] = FVector4f(1.0f);
				VectorFieldParameters.IntensityAndTightness[Index] = FVector4f(0.0f);
			}
			VectorFieldParameters.Count = 0;
			EmptyVectorFieldUniformBuffer = FVectorFieldUniformBufferRef::CreateUniformBufferImmediate(VectorFieldParameters, UniformBuffer_SingleFrame);	
		}
	}

	// Gather simulation commands from all active simulations.
	TArray<FSimulationCommandGPU> SimulationCommands;
	TArray<uint32> TilesToClear;
	TArray<FNewParticle> NewParticles;

	// Compute slack to prevent reallocation of potentially big arrays
	{
		int32 SimulationCommandsCount = 0;
		int32 TitlestoClearCount = 0;
		int32 NewParticlesCount = 0;
		for (TSparseArray<FParticleSimulationGPU*>::TIterator It(GPUSimulations); It; ++It)
		{
			FParticleSimulationGPU* Simulation = *It;
			check(Simulation);
			if (Simulation->SimulationPhase == Phase && Simulation->TileVertexBuffer.TileCount > 0 && Simulation->bEnabled)
			{
				SimulationCommandsCount += 1;
				TitlestoClearCount += Simulation->TilesToClear.Num();
				NewParticlesCount += Simulation->NewParticles.Num();
			}
		}
		SimulationCommands.Empty(SimulationCommandsCount);
		TilesToClear.Empty(TitlestoClearCount);
		NewParticles.Empty(NewParticlesCount);
	}

	for (TSparseArray<FParticleSimulationGPU*>::TIterator It(GPUSimulations); It; ++It)
	{
		//SCOPE_CYCLE_COUNTER(STAT_GPUParticleBuildSimCmdsTime);

		FParticleSimulationGPU* Simulation = *It;
		check(Simulation);
		if (Simulation->SimulationPhase == Phase && Simulation->TileVertexBuffer.TileCount > 0 && Simulation->bEnabled)
		{
			FSimulationCommandGPU* SimulationCommand = new(SimulationCommands) FSimulationCommandGPU(
				Simulation->TileVertexBuffer.GetShaderParam(),
				Simulation->TileVertexBuffer.VertexBufferRHI,
				Simulation->EmitterSimulationResources->SimulationUniformBuffer,
				Simulation->PerFrameSimulationParameters,
				EmptyVectorFieldUniformBuffer,
				Simulation->TileVertexBuffer.TileCount
				);

			// Determine which vector fields affect this simulation and build the appropriate parameters.
			{
				FVectorFieldUniformParameters VectorFieldParameters;
				const FBox SimulationBounds = Simulation->Bounds;

				// Add the local vector field.
				VectorFieldParameters.Count = 0;

				
				float LocalIntensity = 0.0f;
				if (Simulation->LocalVectorField.Resource)
				{
					LocalIntensity = Simulation->LocalVectorField.Intensity * Simulation->LocalVectorField.Resource->Intensity;
					if (FMath::Abs(LocalIntensity) > 0.0f)
					{
						Simulation->LocalVectorField.Resource->Update(RHICmdList, Simulation->PerFrameSimulationParameters.DeltaSeconds);
						SimulationCommand->VectorFieldTexturesRHI[0] = Simulation->LocalVectorField.Resource->VolumeTextureRHI;
						SetParametersForVectorField(VectorFieldParameters, &Simulation->LocalVectorField, /*EmitterScale=*/ 1.0f, /*EmitterTightness=*/ -1, VectorFieldParameters.Count++);
					}
				}

#if !GPUPARTICLE_LOCAL_VF_ONLY
				// Add any world vector fields that intersect the simulation.
				const float GlobalVectorFieldScale = Simulation->EmitterSimulationResources->GlobalVectorFieldScale;
				const float GlobalVectorFieldTightness = Simulation->EmitterSimulationResources->GlobalVectorFieldTightness;
				if (FMath::Abs(GlobalVectorFieldScale) > 0.0f)
				{
					for (TSparseArray<FVectorFieldInstance*>::TIterator VectorFieldIt(VectorFields); VectorFieldIt && VectorFieldParameters.Count < MAX_VECTOR_FIELDS; ++VectorFieldIt)
					{
						FVectorFieldInstance* Instance = *VectorFieldIt;
						check(Instance && Instance->Resource);
						const float Intensity = Instance->Intensity * Instance->Resource->Intensity;
						if (SimulationBounds.Intersect(Instance->WorldBounds) &&
							FMath::Abs(Intensity) > 0.0f)
						{
							SimulationCommand->VectorFieldTexturesRHI[VectorFieldParameters.Count] = Instance->Resource->VolumeTextureRHI;
							SetParametersForVectorField(VectorFieldParameters, Instance, GlobalVectorFieldScale, GlobalVectorFieldTightness, VectorFieldParameters.Count++);
						}
					}
				}
#endif

				// Fill out any remaining vector field entries.
				if (VectorFieldParameters.Count > 0)
				{
#if !GPUPARTICLE_LOCAL_VF_ONLY
					int32 PadCount = VectorFieldParameters.Count;
					while (PadCount < MAX_VECTOR_FIELDS)
					{
						const int32 Index = PadCount++;
						VectorFieldParameters.WorldToVolume[Index] = FMatrix44f::Identity;
						VectorFieldParameters.VolumeToWorld[Index] = FMatrix44f::Identity;
						VectorFieldParameters.VolumeSize[Index] = FVector4f(1.0f);
						VectorFieldParameters.IntensityAndTightness[Index] = FVector4f(0.0f);
					}
#endif
		
						
#if GPUPARTICLE_LOCAL_VF_ONLY
					const bool bRecreateBuffer = !Simulation->LocalVectorFieldUniformBuffer || LocalIntensity != Simulation->LocalIntensity;
					if (bRecreateBuffer)
					{
						Simulation->LocalVectorFieldUniformBuffer = FVectorFieldUniformBufferRef::CreateUniformBufferImmediate(VectorFieldParameters, UniformBuffer_MultiFrame);
						Simulation->LocalIntensity = LocalIntensity;
					}
					SimulationCommand->VectorFieldsUniformBuffer = Simulation->LocalVectorFieldUniformBuffer;
#else
					SimulationCommand->VectorFieldsUniformBuffer = FVectorFieldUniformBufferRef::CreateUniformBufferImmediate(VectorFieldParameters, UniformBuffer_SingleFrame);
#endif
				}
			}
		
			// Add to the list of tiles to clear.
			TilesToClear.Append(Simulation->TilesToClear);
			Simulation->TilesToClear.Reset();

			// Add to the list of new particles.
			NewParticles.Append(Simulation->NewParticles);
			FreeNewParticleArray(Simulation->NewParticles);

			// Reset pending simulation time. This prevents an emitter from simulating twice if we don't get an update from the game thread, e.g. the component didn't tick last frame.
			Simulation->PerFrameSimulationParameters.ResetDeltaSeconds();
		}
	}

	RHICmdList.BeginUpdateMultiFrameResource(CurrentStateRenderTargets[0]);
	RHICmdList.BeginUpdateMultiFrameResource(CurrentStateRenderTargets[1]);
	
	if ( SimulationCommands.Num() || TilesToClear.Num())
	{
		FRHIRenderPassInfo RPInfo(2, CurrentStateRenderTargets, ERenderTargetActions::Load_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("GPUParticles_SimulateAndClear"));
		{
			SCOPED_DRAW_EVENT(RHICmdList, GPUParticles_SimulateAndClear);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, (float)GParticleSimulationTextureSizeX, (float)GParticleSimulationTextureSizeY, 1.0f);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

			// Simulate particles in all active tiles.
			if (SimulationCommands.Num())
			{
				SCOPED_DRAW_EVENT(RHICmdList, ParticleSimulationCommands);

				ExecuteSimulationCommands(
					RHICmdList,
					GraphicsPSOInit,
					FeatureLevel,
					SimulationCommands,
					ParticleSimulationResources,
					ViewUniformBuffer,
					GlobalDistanceFieldParameterData,
					SceneTexturesUniformParams,
					Phase,
					FixDeltaSeconds > 0
				);
			}

			// Clear any newly allocated tiles.
			if (TilesToClear.Num())
			{
				SCOPED_DRAW_EVENT(RHICmdList, ParticleTilesClear);

				ClearTiles(RHICmdList, GraphicsPSOInit, FeatureLevel, TilesToClear);
			}
		}
		RHICmdList.EndRenderPass();
	}

	// Inject any new particles that have spawned into the simulation.
	if (NewParticles.Num())
	{
		SCOPE_CYCLE_COUNTER(STAT_GPUParticlesInjectionTime);
		SCOPED_DRAW_EVENT(RHICmdList, ParticleInjection);
		SCOPED_GPU_STAT(RHICmdList, ParticleSimulation);

		// Set render targets.
		FRHITexture* InjectRenderTargets[4] =
		{
			CurrentStateTextures.PositionTextureRHI,
			CurrentStateTextures.VelocityTextureRHI,
			ParticleSimulationResources->RenderAttributesTexture.TextureRHI,
			ParticleSimulationResources->SimulationAttributesTexture.TextureRHI
		};
		RHICmdList.BeginUpdateMultiFrameResource(ParticleSimulationResources->RenderAttributesTexture.TextureRHI);
		RHICmdList.BeginUpdateMultiFrameResource(ParticleSimulationResources->SimulationAttributesTexture.TextureRHI);

		FRHIRenderPassInfo RPInfo(4, InjectRenderTargets, ERenderTargetActions::Load_Store);
		{
			// Transition attribute textures to writeble, particle state texture are in writeble state already
			RHICmdList.Transition({ 
				FRHITransitionInfo(InjectRenderTargets[2], ERHIAccess::SRVMask, ERHIAccess::RTV),
				FRHITransitionInfo(InjectRenderTargets[3], ERHIAccess::SRVMask, ERHIAccess::RTV)
			});

			RHICmdList.BeginRenderPass(RPInfo, TEXT("ParticleInjection"));

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, (float)GParticleSimulationTextureSizeX, (float)GParticleSimulationTextureSizeY, 1.0f);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

			// Inject particles.
			InjectNewParticles<false>(RHICmdList, GraphicsPSOInit, this->FeatureLevel, NewParticles);

			RHICmdList.EndRenderPass();

			RHICmdList.Transition({
				FRHITransitionInfo(InjectRenderTargets[2], ERHIAccess::RTV, ERHIAccess::SRVMask),
				FRHITransitionInfo(InjectRenderTargets[3], ERHIAccess::RTV, ERHIAccess::SRVMask)
			});
		}

		if (GNumAlternateFrameRenderingGroups > 1)
		{
			if (CVarGPUParticleAFRReinject.GetValueOnRenderThread() == 1)
			{
				ensureMsgf(GNumAlternateFrameRenderingGroups == 2, TEXT("GPU Particles running on an AFR depth > 2 not supported.  Currently: %i"), GNumAlternateFrameRenderingGroups);

				// Place these particles into the multi-gpu update queue
				LastFrameNewParticles.Append(NewParticles);
			}
			else
			{
#if WITH_MGPU
				TemporalEffectTextures.Add(ParticleSimulationResources->RenderAttributesTexture.TextureRHI);
				TemporalEffectTextures.Add(ParticleSimulationResources->SimulationAttributesTexture.TextureRHI);
#endif
			}
		}
#if WITH_MGPU
		if (bCrossTransferEnabled)
		{
			AddCrossGPUTransferResource(ParticleSimulationResources->RenderAttributesTexture.TextureRHI);
			AddCrossGPUTransferResource(ParticleSimulationResources->SimulationAttributesTexture.TextureRHI);
		}
#endif
		RHICmdList.EndUpdateMultiFrameResource(ParticleSimulationResources->RenderAttributesTexture.TextureRHI);
		RHICmdList.EndUpdateMultiFrameResource(ParticleSimulationResources->SimulationAttributesTexture.TextureRHI);
	}
	
	// finish current state render
	RHICmdList.Transition({ 
		FRHITransitionInfo(CurrentStateRenderTargets[0], ERHIAccess::RTV, ERHIAccess::SRVMask),
		FRHITransitionInfo(CurrentStateRenderTargets[1], ERHIAccess::RTV, ERHIAccess::SRVMask)});

	RHICmdList.EndUpdateMultiFrameResource(CurrentStateRenderTargets[0]);
	RHICmdList.EndUpdateMultiFrameResource(CurrentStateRenderTargets[1]);

	if (SimulationCommands.Num() && FixDeltaSeconds > 0)
	{
		//the fixed timestep works in two stages.  A first stage which simulates the fixed timestep and this second stage which simulates any remaining time from the actual delta time.  e.g.  fixed timestep of 16ms and actual dt of 23ms
		//will make this second step simulate an interpolated extra 7ms.  This second interpolated step is what we render on THIS frame, but it is NOT fed into the next frame's simulation.  Thus we do not need to transfer it between GPUs in AFR mode.
		FParticleStateTextures& VisualizeStateTextures = ParticleSimulationResources->GetPreviousStateTextures();
		
		RHICmdList.Transition({ 
			FRHITransitionInfo(VisualizeStateTextures.PositionTextureRHI, ERHIAccess::SRVMask, ERHIAccess::RTV),
			FRHITransitionInfo(VisualizeStateTextures.VelocityTextureRHI, ERHIAccess::SRVMask, ERHIAccess::RTV)
		});
				
		FRHITexture* VisualizeStateRHIs[2] = { VisualizeStateTextures.PositionTextureRHI, VisualizeStateTextures.VelocityTextureRHI };
		FRHIRenderPassInfo RPInfo(2, VisualizeStateRHIs, ERenderTargetActions::Load_Store);
		{
			RHICmdList.BeginRenderPass(RPInfo, TEXT("ExecuteSimulationCommands"));
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

			ExecuteSimulationCommands(
				RHICmdList,
				GraphicsPSOInit,
				FeatureLevel,
				SimulationCommands,
				ParticleSimulationResources,
				ViewUniformBuffer,
				GlobalDistanceFieldParameterData,
				SceneTexturesUniformParams,
				Phase,
				false
			);

			RHICmdList.EndRenderPass();
		}

		RHICmdList.Transition({ 
			FRHITransitionInfo(VisualizeStateTextures.PositionTextureRHI, ERHIAccess::RTV, ERHIAccess::SRVMask),
			FRHITransitionInfo(VisualizeStateTextures.VelocityTextureRHI, ERHIAccess::RTV, ERHIAccess::SRVMask)
		});
	}

#if WITH_MGPU
	// Previously, this broadcast was before the above extra stage simulate work. This
	// is because that work is temporary, only applied visually to the current frame
	// rather than fed back through the simulation for the next frame. However, this
	// led to a possible data race in AFR under certain circumstances: Let's say GPU0
	// writes to StateTextures[0][GPU0] on this frame, then initiates the copy from
	// StateTextures[0][GPU0] to StateTextures[0][GPU1] and signals GPU1 that it can
	// start particle work. GPU0 then starts writing to StateTextures[1][GPU0] as a
	// temporary for extra work on this frame. GPU1 Completes its work on
	// StateTextures[1][GPU1], and initiates the copy from StateTextures[1][GPU1] to
	// StateTextures[1][GPU0] at the end of its frame. However, GPU0 may still be
	// writing to StateTextures[1][GPU0], and this leads to a data race. For now, we're
	// moving the broadcast to after any extra work to block the next AFR group from
	// using the textures until we're done, however for a potential AFR performance
	// boost, we can put the broadcast back to where it was, and use a third buffer for
	// temporary extra particle simulation work.
	if (Phase == PhaseToBroadcastTemporalEffect)
	{
		if (GNumAlternateFrameRenderingGroups > 1)
		{
			RHICmdList.BroadcastTemporalEffect(TemporalEffectName, TemporalEffectTextures);
		}
		if (CrossGPUTransferResources.Num() > 0)
		{
			RHICmdList.TransferResources(CrossGPUTransferResources);
		}
	}
#endif

	// Stats.
	if (Phase == GetLastParticleSimulationPhase(GetShaderPlatform()))
	{
		INC_DWORD_STAT_BY(STAT_FreeGPUTiles,ParticleSimulationResources->GetFreeTileCount());
	}
}

void FFXSystem::OnSimulationPhaseChanged(const FParticleSimulationGPU* GPUSimulation, EParticleSimulatePhase::Type PrevPhase)
{
	// We keep track of the number of simulations of each phase type to more
	// efficiently synchronize temporal effects. We want to avoid having any long
	// stretches of time where the AFR frames can't run in parallel.
	NumGPUSimulations[PrevPhase]--;
	NumGPUSimulations[GPUSimulation->SimulationPhase]++;
}

void FFXSystem::UpdateMultiGPUResources(FRHICommandListImmediate& RHICmdList)
{
	if (LastFrameNewParticles.Num())
	{		
		//Inject particles spawned in the last frame, but only update the attribute textures
		SCOPED_DRAW_EVENT(RHICmdList, ParticleInjection);
		SCOPED_GPU_STAT(RHICmdList, ParticleSimulation);

		// Set render targets.
		FRHITexture* InjectRenderTargets[2] =
		{
			ParticleSimulationResources->RenderAttributesTexture.TextureRHI,
			ParticleSimulationResources->SimulationAttributesTexture.TextureRHI
		};

		FRHIRenderPassInfo RPInfo(2, InjectRenderTargets, ERenderTargetActions::Load_Store);
		RPInfo.ColorRenderTargets[0].ResolveTarget = ParticleSimulationResources->RenderAttributesTexture.TextureRHI;
		RPInfo.ColorRenderTargets[1].ResolveTarget = ParticleSimulationResources->SimulationAttributesTexture.TextureRHI;
		{
			RHICmdList.Transition({
				FRHITransitionInfo(ParticleSimulationResources->RenderAttributesTexture.TextureRHI, ERHIAccess::Unknown, ERHIAccess::RTV),
				FRHITransitionInfo(ParticleSimulationResources->SimulationAttributesTexture.TextureRHI, ERHIAccess::Unknown, ERHIAccess::RTV)
			});
			RHICmdList.BeginRenderPass(RPInfo, TEXT("UpdateMultiGPUResources"));

			RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, (float)GParticleSimulationTextureSizeX, (float)GParticleSimulationTextureSizeY, 1.0f);
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

			// Inject particles.
			InjectNewParticles<true>(RHICmdList, GraphicsPSOInit, this->FeatureLevel, this->LastFrameNewParticles);

			RHICmdList.EndRenderPass();
		}
	}

	// Clear out particles from last frame
	LastFrameNewParticles.Reset();

#if WITH_MGPU
	PhaseToBroadcastTemporalEffect = EParticleSimulatePhase::Last;
	while (PhaseToBroadcastTemporalEffect > EParticleSimulatePhase::First && NumGPUSimulations[PhaseToBroadcastTemporalEffect] == 0)
	{
		PhaseToBroadcastTemporalEffect = static_cast<EParticleSimulatePhase::Type>(PhaseToBroadcastTemporalEffect - 1);
	}

	PhaseToWaitForTemporalEffect = EParticleSimulatePhase::First;
	while (PhaseToWaitForTemporalEffect < PhaseToBroadcastTemporalEffect && NumGPUSimulations[PhaseToWaitForTemporalEffect] == 0)
	{
		PhaseToWaitForTemporalEffect = static_cast<EParticleSimulatePhase::Type>(PhaseToWaitForTemporalEffect + 1);
	}
#endif
}

void FFXSystem::VisualizeGPUParticles(FCanvas* Canvas)
{
	if (!IsPendingKill())
	{
		FFXSystem* FXSystem = this;
		int32 VisualizationMode = FXConsoleVariables::VisualizeGPUSimulation;
		FRenderTarget* RenderTarget = Canvas->GetRenderTarget();
		ERHIFeatureLevel::Type InFeatureLevel = FeatureLevel;
		ENQUEUE_RENDER_COMMAND(FVisualizeGPUParticlesCommand)(
			[FXSystem, VisualizationMode, RenderTarget, InFeatureLevel](FRHICommandList& RHICmdList)
		{
			FParticleSimulationResources* Resources = FXSystem->GetParticleSimulationResources();
			FParticleStateTextures& CurrentStateTextures = Resources->GetVisualizeStateTextures();
			VisualizeGPUSimulation(RHICmdList, InFeatureLevel, VisualizationMode, RenderTarget, CurrentStateTextures, GParticleCurveTexture.GetCurveTexture());
		});
	}
}

/*-----------------------------------------------------------------------------
	External interface.
-----------------------------------------------------------------------------*/

FParticleEmitterInstance* FFXSystem::CreateGPUSpriteEmitterInstance( FGPUSpriteEmitterInfo& EmitterInfo )
{
	return new FGPUSpriteParticleEmitterInstance( this, EmitterInfo );
}

/**
 * Sets GPU sprite resource data.
 * @param Resources - Sprite resources to update.
 * @param InResourceData - Data with which to update resources.
 */
static void SetGPUSpriteResourceData( FGPUSpriteResources* Resources, const FGPUSpriteResourceData& InResourceData )
{
	// Allocate texels for all curves.
	Resources->ColorTexelAllocation = GParticleCurveTexture.AddCurve( InResourceData.QuantizedColorSamples );
	Resources->MiscTexelAllocation = GParticleCurveTexture.AddCurve( InResourceData.QuantizedMiscSamples );
	Resources->SimulationAttrTexelAllocation = GParticleCurveTexture.AddCurve( InResourceData.QuantizedSimulationAttrSamples );

	// Setup uniform parameters for the emitter.
	Resources->UniformParameters.ColorCurve = GParticleCurveTexture.ComputeCurveScaleBias(Resources->ColorTexelAllocation);
	Resources->UniformParameters.ColorScale = (FVector4f)InResourceData.ColorScale; // LWC_TODO: change property to FVector4f
	Resources->UniformParameters.ColorBias = (FVector4f)InResourceData.ColorBias; // LWC_TODO: change property to FVector4f

	Resources->UniformParameters.MiscCurve = GParticleCurveTexture.ComputeCurveScaleBias(Resources->MiscTexelAllocation);
	Resources->UniformParameters.MiscScale = (FVector4f)InResourceData.MiscScale; // LWC_TODO: change property to FVector4f
	Resources->UniformParameters.MiscBias = (FVector4f)InResourceData.MiscBias; // LWC_TODO: change property to FVector4f

	Resources->UniformParameters.SizeBySpeed = (FVector4f)InResourceData.SizeBySpeed; // LWC_TODO: change property to FVector4f
	Resources->UniformParameters.SubImageSize = (FVector4f)InResourceData.SubImageSize; // LWC_TODO: change property to FVector4f

	// Setup tangent selector parameter.
	const EParticleAxisLock LockAxisFlag = (EParticleAxisLock)InResourceData.LockAxisFlag;
	const bool bRotationLock = (LockAxisFlag >= EPAL_ROTATE_X) && (LockAxisFlag <= EPAL_ROTATE_Z);

	Resources->UniformParameters.TangentSelector = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	Resources->UniformParameters.RotationBias = 0.0f;

	if (InResourceData.ScreenAlignment == PSA_Velocity)
	{
		Resources->UniformParameters.TangentSelector.Y = 1;
	}
	else if(LockAxisFlag == EPAL_NONE )
	{
		if (InResourceData.ScreenAlignment == PSA_Square)
		{
			Resources->UniformParameters.TangentSelector.X = 1;
		}
		else if (InResourceData.ScreenAlignment == PSA_FacingCameraPosition)
		{
			Resources->UniformParameters.TangentSelector.W = 1;
		}
	}
	else
	{
		if ( bRotationLock )
		{
			Resources->UniformParameters.TangentSelector.Z = 1.0f;
		}
		else
		{
			Resources->UniformParameters.TangentSelector.X = 1.0f;
		}

		// For locked rotation about Z the particle should be rotated by 90 degrees.
		Resources->UniformParameters.RotationBias = (LockAxisFlag == EPAL_ROTATE_Z) ? (0.5f * UE_PI) : 0.0f;
	}

	// Alignment overrides
	Resources->UniformParameters.RemoveHMDRoll = InResourceData.bRemoveHMDRoll ? 1.f : 0.f;

	if (InResourceData.ScreenAlignment == PSA_FacingCameraDistanceBlend)
	{
		float DistanceBlendMinSq = InResourceData.MinFacingCameraBlendDistance * InResourceData.MinFacingCameraBlendDistance;
		float DistanceBlendMaxSq = InResourceData.MaxFacingCameraBlendDistance * InResourceData.MaxFacingCameraBlendDistance;
		float InvBlendRange = 1.f / FMath::Max(DistanceBlendMaxSq - DistanceBlendMinSq, 1.f);
		float BlendScaledMinDistace = DistanceBlendMinSq * InvBlendRange;

		Resources->UniformParameters.CameraFacingBlend.X = 1.f;
		Resources->UniformParameters.CameraFacingBlend.Y = InvBlendRange;
		Resources->UniformParameters.CameraFacingBlend.Z = BlendScaledMinDistace;

		// Treat as camera facing if needed
		Resources->UniformParameters.TangentSelector.W = 1.0f;
	}
	else
	{
		Resources->UniformParameters.CameraFacingBlend.X = 0.f;
		Resources->UniformParameters.CameraFacingBlend.Y = 0.f;
		Resources->UniformParameters.CameraFacingBlend.Z = 0.f;
	}

	Resources->UniformParameters.RotationRateScale = InResourceData.RotationRateScale;
	Resources->UniformParameters.CameraMotionBlurAmount = InResourceData.CameraMotionBlurAmount;

	Resources->UniformParameters.PivotOffset = FVector2f(InResourceData.PivotOffset);

	Resources->SimulationParameters.AttributeCurve = GParticleCurveTexture.ComputeCurveScaleBias(Resources->SimulationAttrTexelAllocation);
	Resources->SimulationParameters.AttributeCurveScale = (FVector4f)InResourceData.SimulationAttrCurveScale; // LWC_TODO: change property to FVector4f
	Resources->SimulationParameters.AttributeCurveBias = (FVector4f)InResourceData.SimulationAttrCurveBias; // LWC_TODO: change property to FVector4f
	Resources->SimulationParameters.AttributeScale = FVector4f(
		InResourceData.DragCoefficientScale,
		InResourceData.PerParticleVectorFieldScale,
		InResourceData.ResilienceScale,
		1.0f  // OrbitRandom
		);
	Resources->SimulationParameters.AttributeBias = FVector4f(
		InResourceData.DragCoefficientBias,
		InResourceData.PerParticleVectorFieldBias,
		InResourceData.ResilienceBias,
		0.0f  // OrbitRandom
		);
	Resources->SimulationParameters.MiscCurve = Resources->UniformParameters.MiscCurve;
	Resources->SimulationParameters.MiscScale = Resources->UniformParameters.MiscScale;
	Resources->SimulationParameters.MiscBias = Resources->UniformParameters.MiscBias;
	Resources->SimulationParameters.Acceleration = (FVector3f)InResourceData.ConstantAcceleration;
	Resources->SimulationParameters.OrbitOffsetBase = (FVector3f)InResourceData.OrbitOffsetBase;
	Resources->SimulationParameters.OrbitOffsetRange = (FVector3f)InResourceData.OrbitOffsetRange;
	Resources->SimulationParameters.OrbitFrequencyBase = (FVector3f)InResourceData.OrbitFrequencyBase;
	Resources->SimulationParameters.OrbitFrequencyRange = (FVector3f)InResourceData.OrbitFrequencyRange;
	Resources->SimulationParameters.OrbitPhaseBase = (FVector3f)InResourceData.OrbitPhaseBase;
	Resources->SimulationParameters.OrbitPhaseRange = (FVector3f)InResourceData.OrbitPhaseRange;
	Resources->SimulationParameters.CollisionRadiusScale = InResourceData.CollisionRadiusScale;
	Resources->SimulationParameters.CollisionRadiusBias = InResourceData.CollisionRadiusBias;
	Resources->SimulationParameters.CollisionTimeBias = InResourceData.CollisionTimeBias;
	Resources->SimulationParameters.CollisionRandomSpread = InResourceData.CollisionRandomSpread;
	Resources->SimulationParameters.CollisionRandomDistribution = InResourceData.CollisionRandomDistribution;
	Resources->SimulationParameters.OneMinusFriction = InResourceData.OneMinusFriction;
	Resources->EmitterSimulationResources.GlobalVectorFieldScale = InResourceData.GlobalVectorFieldScale;
	Resources->EmitterSimulationResources.GlobalVectorFieldTightness = InResourceData.GlobalVectorFieldTightness;
}

/**
 * Clears GPU sprite resource data.
 * @param Resources - Sprite resources to update.
 * @param InResourceData - Data with which to update resources.
 */
static void ClearGPUSpriteResourceData( FGPUSpriteResources* Resources )
{
	GParticleCurveTexture.RemoveCurve( Resources->ColorTexelAllocation );
	GParticleCurveTexture.RemoveCurve( Resources->MiscTexelAllocation );
	GParticleCurveTexture.RemoveCurve( Resources->SimulationAttrTexelAllocation );
}

FGPUSpriteResources* BeginCreateGPUSpriteResources( const FGPUSpriteResourceData& InResourceData )
{
	FGPUSpriteResources* Resources = NULL;
	if (RHISupportsGPUParticles())
	{
		LLM_SCOPE(ELLMTag::Particles);

		Resources = new FGPUSpriteResources;
		//@TODO Ideally FGPUSpriteEmitterInfo::Resources would be a TRefCountPtr<FGPUSpriteResources>, but
		//since that class is defined in this file, we can't do that, so we just addref here instead
		Resources->AddRef();
		SetGPUSpriteResourceData( Resources, InResourceData );
		BeginInitResource( Resources );
	}
	return Resources;
}

void BeginUpdateGPUSpriteResources( FGPUSpriteResources* Resources, const FGPUSpriteResourceData& InResourceData )
{
	check( Resources );
	ClearGPUSpriteResourceData( Resources );
	SetGPUSpriteResourceData( Resources, InResourceData );
	BeginUpdateResourceRHI( Resources );
}

void BeginReleaseGPUSpriteResources( FGPUSpriteResources* Resources )
{
	if ( Resources )
	{
		ClearGPUSpriteResourceData( Resources );
		// Deletion of this resource is deferred until all particle
		// systems on the render thread have finished with it.
		Resources->Release();
	}
}

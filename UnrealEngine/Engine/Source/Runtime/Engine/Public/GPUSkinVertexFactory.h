// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUSkinVertexFactory.h: GPU skinning vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RHI.h"
#include "RenderResource.h"
#include "BoneIndices.h"
#include "GPUSkinPublicDefs.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "ResourcePool.h"
#include "Matrix3x4.h"
#include "SkeletalMeshTypes.h"

class FRDGPooledBuffer;

template <class T> class TConsoleVariableData;

// Uniform buffer for APEX cloth
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FAPEXClothUniformShaderParameters,)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

enum
{
	// 256 works for real uniform buffers, emulated UB can support up to 75 
	MAX_GPU_BONE_MATRICES_UNIFORMBUFFER = 256,
};

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FBoneMatricesUniformShaderParameters,)
	SHADER_PARAMETER_ARRAY(FMatrix3x4, BoneMatrices, [MAX_GPU_BONE_MATRICES_UNIFORMBUFFER])
END_GLOBAL_SHADER_PARAMETER_STRUCT()

#define SET_BONE_DATA(B, X) B.SetMatrixTranspose(X)

/** Shared data & implementation for the different types of pool */
class FSharedPoolPolicyData
{
public:
	/** Buffers are created with a simple byte size */
	typedef uint32 CreationArguments;
	enum
	{
		NumSafeFrames = 3, /** Number of frames to leaves buffers before reclaiming/reusing */
		NumPoolBucketSizes = 18, /** Number of pool buckets */
		NumToDrainPerFrame = 10, /** Max. number of resources to cull in a single frame */
		CullAfterFramesNum = 30 /** Resources are culled if unused for more frames than this */
	};
	
	/** Get the pool bucket index from the size
	 * @param Size the number of bytes for the resource 
	 * @returns The bucket index.
	 */
	uint32 GetPoolBucketIndex(uint32 Size);
	
	/** Get the pool bucket size from the index
	 * @param Bucket the bucket index
	 * @returns The bucket size.
	 */
	uint32 GetPoolBucketSize(uint32 Bucket);
	
private:
	/** The bucket sizes */
	static uint32 BucketSizes[NumPoolBucketSizes];
};

/** Struct to pool the vertex buffer & SRV together */
struct FVertexBufferAndSRV
{
	void SafeRelease()
	{
		VertexBufferRHI.SafeRelease();
		VertexBufferSRV.SafeRelease();
	}

	FBufferRHIRef VertexBufferRHI;
	FShaderResourceViewRHIRef VertexBufferSRV;
};

/**
 * Helper function to test whether the buffer is valid.
 * @param Buffer Buffer to test
 * @returns True if the buffer is valid otherwise false
 */
inline bool IsValidRef(const FVertexBufferAndSRV& Buffer)
{
	return IsValidRef(Buffer.VertexBufferRHI) && IsValidRef(Buffer.VertexBufferSRV);
}

/** The policy for pooling bone vertex buffers */
class FBoneBufferPoolPolicy : public FSharedPoolPolicyData
{
public:
	enum
	{
		NumSafeFrames = FSharedPoolPolicyData::NumSafeFrames,
		NumPoolBuckets = FSharedPoolPolicyData::NumPoolBucketSizes,
		NumToDrainPerFrame = FSharedPoolPolicyData::NumToDrainPerFrame,
		CullAfterFramesNum = FSharedPoolPolicyData::CullAfterFramesNum
	};
	/** Creates the resource 
	 * @param Args The buffer size in bytes.
	 */
	FVertexBufferAndSRV CreateResource(FSharedPoolPolicyData::CreationArguments Args);
	
	/** Gets the arguments used to create resource
	 * @param Resource The buffer to get data for.
	 * @returns The arguments used to create the buffer.
	 */
	FSharedPoolPolicyData::CreationArguments GetCreationArguments(const FVertexBufferAndSRV& Resource);
	
	/** Frees the resource
	 * @param Resource The buffer to prepare for release from the pool permanently.
	 */
	void FreeResource(FVertexBufferAndSRV Resource);
};

/** A pool for vertex buffers with consistent usage, bucketed for efficiency. */
class FBoneBufferPool : public TRenderResourcePool<FVertexBufferAndSRV, FBoneBufferPoolPolicy, FSharedPoolPolicyData::CreationArguments>
{
public:
	/** Destructor */
	virtual ~FBoneBufferPool();

public: // From FTickableObjectRenderThread
	virtual TStatId GetStatId() const override;
};

/** The policy for pooling bone vertex buffers */
class FClothBufferPoolPolicy : public FBoneBufferPoolPolicy
{
public:
	/** Creates the resource 
	 * @param Args The buffer size in bytes.
	 */
	FVertexBufferAndSRV CreateResource(FSharedPoolPolicyData::CreationArguments Args);
};

/** A pool for vertex buffers with consistent usage, bucketed for efficiency. */
class FClothBufferPool : public TRenderResourcePool<FVertexBufferAndSRV, FClothBufferPoolPolicy, FSharedPoolPolicyData::CreationArguments>
{
public:
	/** Destructor */
	virtual ~FClothBufferPool();
	
public: // From FTickableObjectRenderThread
	virtual TStatId GetStatId() const override;
};

enum GPUSkinBoneInfluenceType
{
	DefaultBoneInfluence,	// up to 8 bones per vertex
	UnlimitedBoneInfluence	// unlimited bones per vertex
};

/** Stream component data bound to GPU skinned vertex factory */
struct FGPUSkinDataType : public FStaticMeshDataType
{
	/** The stream to read the bone indices from */
	FVertexStreamComponent BoneIndices;

	/** The stream to read the extra bone indices from */
	FVertexStreamComponent ExtraBoneIndices;

	/** The stream to read the bone weights from */
	FVertexStreamComponent BoneWeights;

	/** The stream to read the extra bone weights from */
	FVertexStreamComponent ExtraBoneWeights;

	/** The stream to read the blend stream offset and num of influences from */
	FVertexStreamComponent BlendOffsetCount;

	/** Number of bone influences */
	uint32 NumBoneInfluences = 0;

	/** If the bone indices are 16 or 8-bit format */
	bool bUse16BitBoneIndex = 0;

	/** If this is a morph target */
	bool bMorphTarget = false;

	/** Morph target stream which has the position deltas to add to the vertex position */
	FVertexStreamComponent DeltaPositionComponent;

	/** Morph target stream which has the TangentZ deltas to add to the vertex normals */
	FVertexStreamComponent DeltaTangentZComponent;

	/** Morph vertex buffer pool double buffering delta data  */
	class FMorphVertexBufferPool* MorphVertexBufferPool = nullptr;
};

/** Vertex factory with vertex stream components for GPU skinned vertices */
class FGPUBaseSkinVertexFactory : public FVertexFactory
{
public:
	struct FShaderDataType
	{
		FShaderDataType()
			: CurrentBuffer(0)
			, PreviousRevisionNumber(0)
			, CurrentRevisionNumber(0)
		{
			// BoneDataOffset and BoneTextureSize are not set as they are only valid if IsValidRef(BoneTexture)
			MaxGPUSkinBones = GetMaxGPUSkinBones();
			check(MaxGPUSkinBones <= GHardwareMaxGPUSkinBones);
		}

		// @param FrameTime from GFrameTime
		bool UpdateBoneData(FRHICommandListImmediate& RHICmdList, const TArray<FMatrix44f>& ReferenceToLocalMatrices,
			const TArray<FBoneIndexType>& BoneMap, uint32 RevisionNumber, bool bPrevious, ERHIFeatureLevel::Type FeatureLevel, bool bUseSkinCache);

		void ReleaseBoneData()
		{
			ensure(IsInRenderingThread());

			UniformBuffer.SafeRelease();

			for(uint32 i = 0; i < 2; ++i)
			{
				if (IsValidRef(BoneBuffer[i]))
				{
					BoneBufferPool.ReleasePooledResource(BoneBuffer[i]);
				}
				BoneBuffer[i].SafeRelease();
			}
		}
		
		// if FeatureLevel <= ERHIFeatureLevel::ES3_1
		FRHIUniformBuffer* GetUniformBuffer() const
		{
			return UniformBuffer;
		}
		
		bool HasBoneBufferForReading(bool bPrevious) const
		{
			const FVertexBufferAndSRV* RetPtr = &GetBoneBufferInternal(bPrevious);
			if (!RetPtr->VertexBufferRHI.IsValid() && bPrevious)
			{
				RetPtr = &GetBoneBufferInternal(false);
			}
			return RetPtr->VertexBufferRHI.IsValid();
		}

		// @param bPrevious true:previous, false:current
		const FVertexBufferAndSRV& GetBoneBufferForReading(bool bPrevious) const
		{
			const FVertexBufferAndSRV* RetPtr = &GetBoneBufferInternal(bPrevious);

			if(!RetPtr->VertexBufferRHI.IsValid())
			{
				// this only should happen if we request the old data
				check(bPrevious);

				// if we don't have any old data we use the current one
				RetPtr = &GetBoneBufferInternal(false);

				// at least the current one needs to be valid when reading
				check(RetPtr->VertexBufferRHI.IsValid());
			}

			return *RetPtr;
		}

		// @param bPrevious true:previous, false:current
		// @return IsValid() can fail, then you have to create the buffers first (or if the size changes)
		FVertexBufferAndSRV& GetBoneBufferForWriting(bool bPrevious)
		{
			const FShaderDataType* This = (const FShaderDataType*)this;
			// non const version maps to const version
			return (FVertexBufferAndSRV&)This->GetBoneBufferInternal(bPrevious);
		}

		// @param bPrevious true:previous, false:current
		// @return returns revision number 
		uint32 GetRevisionNumber(bool bPrevious) const
		{
			return (bPrevious) ? PreviousRevisionNumber : CurrentRevisionNumber;
		}

		int32 InputWeightIndexSize = 0;
		FShaderResourceViewRHIRef InputWeightStream;
		// Frame number of the bone data that is last updated
		uint32 UpdatedFrameNumber = 0;

	private:
		// double buffered bone positions+orientations to support normal rendering and velocity (new-old position) rendering
		FVertexBufferAndSRV BoneBuffer[2];
		// 0 / 1 to index into BoneBuffer
		uint32 CurrentBuffer;
		// RevisionNumber Tracker
		uint32 PreviousRevisionNumber;
		uint32 CurrentRevisionNumber;
		// if FeatureLevel <= ERHIFeatureLevel::ES3_1
		FUniformBufferRHIRef UniformBuffer;
		
		static TConsoleVariableData<int32>* MaxBonesVar;
		static uint32 MaxGPUSkinBones;
		
		// @param RevisionNumber - updated last revision number
		// This flips revision number to previous if this is new
		// otherwise, it keeps current version
		void SetCurrentRevisionNumber(uint32 RevisionNumber)
		{
			if (CurrentRevisionNumber != RevisionNumber)
			{
				PreviousRevisionNumber = CurrentRevisionNumber;
				CurrentRevisionNumber = RevisionNumber;
				CurrentBuffer = 1 - CurrentBuffer;
			}
		}
		// to support GetBoneBufferForWriting() and GetBoneBufferForReading()
		// @param bPrevious true:previous, false:current
		// @return might not pass the IsValid() 
		const FVertexBufferAndSRV& GetBoneBufferInternal(bool bPrevious) const
		{
			check(IsInParallelRenderingThread());

			if ((CurrentRevisionNumber - PreviousRevisionNumber) > 1)
			{
				bPrevious = false;
			}

			uint32 BufferIndex = CurrentBuffer ^ (uint32)bPrevious;

			const FVertexBufferAndSRV& Ret = BoneBuffer[BufferIndex];
			return Ret;
		}
	};

	FGPUBaseSkinVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, uint32 InNumVertices)
		: FVertexFactory(InFeatureLevel)
		, NumVertices(InNumVertices)
	{
	}

	virtual ~FGPUBaseSkinVertexFactory() {}

	/** accessor */
	FORCEINLINE FShaderDataType& GetShaderData()
	{
		return ShaderData;
	}

	FORCEINLINE const FShaderDataType& GetShaderData() const
	{
		return ShaderData;
	}

	/**
	* An implementation of the interface used by TSynchronizedResource to
	* update the resource with new data from the game thread.
	* @param	InData - new stream component data
	*/
	virtual void SetData(const FGPUSkinDataType* InData);

	uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	/*
	 * Return the smallest platform MaxGPUSkinBones value.
	 */
	ENGINE_API static int32 GetMinimumPerPlatformMaxGPUSkinBonesValue();
	ENGINE_API static int32 GetMaxGPUSkinBones(const class ITargetPlatform* TargetPlatform = nullptr);

	static const uint32 GHardwareMaxGPUSkinBones = 65536;
	
	ENGINE_API static bool UseUnlimitedBoneInfluences(uint32 MaxBoneInfluences);
	ENGINE_API static bool GetUnlimitedBoneInfluences();

	/** Morph vertex factory functions */
	virtual void UpdateMorphVertexStream(const class FMorphVertexBuffer* MorphVertexBuffer) {}
	virtual const class FMorphVertexBuffer* GetMorphVertexBuffer(bool bPrevious, uint32 FrameNumber) const { return nullptr; }
	/** Cloth vertex factory access. */
	virtual class FGPUBaseSkinAPEXClothVertexFactory* GetClothVertexFactory() { return nullptr; }
	virtual class FGPUBaseSkinAPEXClothVertexFactory const* GetClothVertexFactory() const { return nullptr; }

	virtual GPUSkinBoneInfluenceType GetBoneInfluenceType() const				{ return DefaultBoneInfluence; }
	virtual uint32 GetNumBoneInfluences() const									{ return Data.IsValid() ? Data->NumBoneInfluences : 0; }
	virtual bool Use16BitBoneIndex() const										{ return Data.IsValid() ? Data->bUse16BitBoneIndex : false; }
	virtual const FShaderResourceViewRHIRef GetPositionsSRV() const				{ return Data.IsValid() ? Data->PositionComponentSRV : nullptr; }
	virtual const FShaderResourceViewRHIRef GetTangentsSRV() const				{ return Data.IsValid() ? Data->TangentsSRV : nullptr; }
	virtual const FShaderResourceViewRHIRef GetTextureCoordinatesSRV() const	{ return Data.IsValid() ? Data->TextureCoordinatesSRV : nullptr; }
	virtual const FShaderResourceViewRHIRef GetColorComponentsSRV() const		{ return Data.IsValid() ? Data->ColorComponentsSRV : nullptr; }
	virtual uint32 GetNumTexCoords() const										{ return Data.IsValid() ? Data->NumTexCoords : 0; }
	virtual const uint32 GetColorIndexMask() const								{ return Data.IsValid() ? Data->ColorIndexMask : 0; }
	virtual bool IsMorphTarget() const											{ return Data.IsValid() ? Data->bMorphTarget : false; }

	inline const FVertexStreamComponent& GetPositionStreamComponent() const
	{
		check(Data.IsValid() && Data->PositionComponent.VertexBuffer != nullptr);
		return Data->PositionComponent;
	}
	
	inline const FVertexStreamComponent& GetTangentStreamComponent(int Index) const
	{
		check(Data.IsValid() && Data->TangentBasisComponents[Index].VertexBuffer != nullptr);
		return Data->TangentBasisComponents[Index];
	}

	void CopyDataTypeForPassthroughFactory(class FGPUSkinPassthroughVertexFactory* PassthroughVertexFactory);

protected:
	/**
	* Add the decl elements for the streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/
	virtual void AddVertexElements(FVertexDeclarationElementList& OutElements) = 0;

	/** dynamic data need for setting the shader */ 
	FShaderDataType ShaderData;
	/** Pool of buffers for bone matrices. */
	static TGlobalResource<FBoneBufferPool> BoneBufferPool;

	/** stream component data bound to this vertex factory */
	TUniquePtr<FGPUSkinDataType> Data;

private:
	uint32 NumVertices;
};

/** Vertex factory with vertex stream components for GPU skinned vertices */
template<GPUSkinBoneInfluenceType BoneInfluenceType>
class ENGINE_API TGPUSkinVertexFactory : public FGPUBaseSkinVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(TGPUSkinVertexFactory<BoneInfluenceType>);

public:
	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	TGPUSkinVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, uint32 InNumVertices)
		: FGPUBaseSkinVertexFactory(InFeatureLevel, InNumVertices)
	{}

	virtual GPUSkinBoneInfluenceType GetBoneInfluenceType() const override
	{ return BoneInfluenceType; }

	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);	
	
	/** FGPUBaseSkinVertexFactory overrides */
	virtual void UpdateMorphVertexStream(const class FMorphVertexBuffer* MorphVertexBuffer) override;
	virtual const class FMorphVertexBuffer* GetMorphVertexBuffer(bool bPrevious, uint32 FrameNumber) const override;


	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

protected:
	/**
	* Add the decl elements for the streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/
	virtual void AddVertexElements(FVertexDeclarationElementList& OutElements) override;

private:
	int32 MorphDeltaStreamIndex = -1;
};

/** 
 * Vertex factory with vertex stream components for GPU-skinned streams, enabled for passthrough mode when vertices have been pre-skinned 
 */
class FGPUSkinPassthroughVertexFactory : public FLocalVertexFactory
{
	typedef FLocalVertexFactory Super;

public:
	FGPUSkinPassthroughVertexFactory(ERHIFeatureLevel::Type InFeatureLevel);
	~FGPUSkinPassthroughVertexFactory();

	inline int32 GetPositionStreamIndex() const
	{
		check(PositionStreamIndex > -1);
		return PositionStreamIndex;
	}

	inline int32 GetTangentStreamIndex() const
	{
		return TangentStreamIndex;
	}

	void SetData(const FDataType& InData);

	uint32 GetUpdatedFrameNumber() const { return UpdatedFrameNumber; }

	//TODO should be supported
	bool SupportsPositionOnlyStream() const override { return false; }
	bool SupportsPositionAndNormalOnlyStream() const override { return false; }

	inline void InvalidateStreams()
	{
		PositionStreamIndex = -1;
		TangentStreamIndex = -1;

		PositionVBAlias.ReleaseRHI();
		TangentVBAlias.ReleaseRHI();
		ColorVBAlias.ReleaseRHI();
	}

	virtual void ReleaseRHI() override;

	/** Poke values into the vertex factory. */
	inline void UpdateVertexDeclaration(
		FGPUBaseSkinVertexFactory const* SourceVertexFactory, 
		struct FRWBuffer* PositionRWBuffer, 
		struct FRWBuffer* TangentRWBuffer)
	{
		UpdatedFrameNumber = SourceVertexFactory->GetShaderData().UpdatedFrameNumber;
		if (PositionStreamIndex == -1)
		{
			InternalUpdateVertexDeclaration(SourceVertexFactory, PositionRWBuffer, TangentRWBuffer);
		}
	}

	/** Flags for override bitmask passed to UpdateVertexDeclaration(). */
	enum class EOverrideFlags
	{
		None		= 0,
		Position	= 1 << 0,
		Tangent		= 1 << 1,
		Color		= 1 << 2,
		All			= 0xff,
	};

	/** Poke values into the vertex factory. */
	inline void UpdateVertexDeclaration(
		EOverrideFlags OverrideFlags,
		FGPUBaseSkinVertexFactory const* SourceVertexFactory,
		TRefCountPtr<FRDGPooledBuffer> const& PositionBuffer,
		TRefCountPtr<FRDGPooledBuffer> const& TangentBuffer,
		TRefCountPtr<FRDGPooledBuffer> const& ColorBuffer)
	{
		UpdatedFrameNumber = SourceVertexFactory->GetShaderData().UpdatedFrameNumber;
		if (PositionStreamIndex == -1)
		{
			InternalUpdateVertexDeclaration(OverrideFlags, SourceVertexFactory, PositionBuffer, TangentBuffer, ColorBuffer);
		}
	}

	inline FRHIShaderResourceView* GetPreviousPositionsSRV() const
	{
		return PrevPositionSRVAlias;
	}

protected:
	friend class FLocalVertexFactoryShaderParameters;
	friend class FSkeletalMeshSceneProxy;

	// Reference holders for RDG buffers
	TRefCountPtr<FRDGPooledBuffer> PositionRDG;
	TRefCountPtr<FRDGPooledBuffer> PrevPositionRDG;
	TRefCountPtr<FRDGPooledBuffer> TangentRDG;
	TRefCountPtr<FRDGPooledBuffer> ColorRDG;
	// Vertex buffer required for creating the Vertex Declaration
	FVertexBuffer PositionVBAlias;
	FVertexBuffer TangentVBAlias;
	FVertexBuffer ColorVBAlias;
	// SRVs required for binding
	FRHIShaderResourceView* PositionSRVAlias = nullptr;
	FRHIShaderResourceView* PrevPositionSRVAlias = nullptr;
	FRHIShaderResourceView* TangentSRVAlias = nullptr;
	FRHIShaderResourceView* ColorSRVAlias = nullptr;
	// Cached stream indices
	int32 PositionStreamIndex = -1;
	int32 TangentStreamIndex = -1;
	// Frame number of the bone data that is last updated
	uint32 UpdatedFrameNumber = 0;

	void InternalUpdateVertexDeclaration(
		FGPUBaseSkinVertexFactory const* SourceVertexFactory);
	void InternalUpdateVertexDeclaration(
		FGPUBaseSkinVertexFactory const* SourceVertexFactory, 
		struct FRWBuffer* PositionRWBuffer, 
		struct FRWBuffer* TangentRWBuffer);
	void InternalUpdateVertexDeclaration(
		EOverrideFlags OverrideFlags,
		FGPUBaseSkinVertexFactory const* SourceVertexFactory,
		TRefCountPtr<FRDGPooledBuffer> const& PositionBuffer,
		TRefCountPtr<FRDGPooledBuffer> const& TangentBuffer,
		TRefCountPtr<FRDGPooledBuffer> const& ColorBuffer);
};

ENUM_CLASS_FLAGS(FGPUSkinPassthroughVertexFactory::EOverrideFlags)

/** Vertex factory with vertex stream components for GPU-skinned and morph target streams */
class FGPUBaseSkinAPEXClothVertexFactory
{
public:
	struct ClothShaderType
	{
		ClothShaderType()
		{
			Reset();
		}

		bool UpdateClothSimulData(FRHICommandListImmediate& RHICmdList, const TArray<FVector3f>& InSimulPositions, const TArray<FVector3f>& InSimulNormals, uint32 FrameNumber, ERHIFeatureLevel::Type FeatureLevel);

		void ReleaseClothSimulData()
		{
			APEXClothUniformBuffer.SafeRelease();

			for(uint32 i = 0; i < 2; ++i)
			{
				if (IsValidRef(ClothSimulPositionNormalBuffer[i]))
				{
					ClothSimulDataBufferPool.ReleasePooledResource(ClothSimulPositionNormalBuffer[i]);
					ClothSimulPositionNormalBuffer[i].SafeRelease();
				}
			}
			Reset();
		}

		void EnableDoubleBuffer()	{ bDoubleBuffer = true; }

		TUniformBufferRef<FAPEXClothUniformShaderParameters> GetClothUniformBuffer() const
		{
			return APEXClothUniformBuffer;
		}
		
		// @param FrameNumber usually from View.Family->FrameNumber
		// @return IsValid() can fail, then you have to create the buffers first (or if the size changes)
		FVertexBufferAndSRV& GetClothBufferForWriting(uint32 FrameNumber)
		{
			uint32 Index = GetOldestIndex(FrameNumber);
			Index = (BufferFrameNumber[0] == FrameNumber) ? 0 : Index;
			Index = (BufferFrameNumber[1] == FrameNumber) ? 1 : Index;

			// we don't write -1 as that is used to invalidate the entry
			if(FrameNumber == -1)
			{
				// this could cause a 1 frame glitch on wraparound
				FrameNumber = 0;
			}

			BufferFrameNumber[Index] = FrameNumber;

			return ClothSimulPositionNormalBuffer[Index];
		}

		bool HasClothBufferForReading(bool bPrevious, uint32 FrameNumber) const
		{
			int32 Index = GetMostRecentIndex(FrameNumber);
			if (bPrevious && DoWeHavePreviousData())
			{
				Index = 1 - Index;
			}
			return ClothSimulPositionNormalBuffer[Index].VertexBufferRHI.IsValid();
		}

		// @param bPrevious true:previous, false:current
		// @param FrameNumber usually from View.Family->FrameNumber
		const FVertexBufferAndSRV& GetClothBufferForReading(bool bPrevious, uint32 FrameNumber) const
		{
			int32 Index = GetMostRecentIndex(FrameNumber);

			if(bPrevious && DoWeHavePreviousData())
			{
				Index = 1 - Index;
			}

			checkf(ClothSimulPositionNormalBuffer[Index].VertexBufferRHI.IsValid(), TEXT("Index: %i Buffer0: %s Frame0: %i Buffer1: %s Frame1: %i"), Index,  ClothSimulPositionNormalBuffer[0].VertexBufferRHI.IsValid() ? TEXT("true") : TEXT("false"), BufferFrameNumber[0], ClothSimulPositionNormalBuffer[1].VertexBufferRHI.IsValid() ? TEXT("true") : TEXT("false"), BufferFrameNumber[1]);
			return ClothSimulPositionNormalBuffer[Index];
		}
		
		FMatrix44f& GetClothToLocalForWriting(uint32 FrameNumber)
		{
			uint32 Index = GetOldestIndex(FrameNumber);
			Index = (BufferFrameNumber[0] == FrameNumber) ? 0 : Index;
			Index = (BufferFrameNumber[1] == FrameNumber) ? 1 : Index;

			return ClothToLocal[Index];
		}

		const FMatrix44f& GetClothToLocalForReading(bool bPrevious, uint32 FrameNumber) const
		{
			int32 Index = GetMostRecentIndex(FrameNumber);

			if(bPrevious && DoWeHavePreviousData())
			{
				Index = 1 - Index;
			}

			return ClothToLocal[Index];
		}

		/**
		 * weight to blend between simulated positions and key-framed poses
		 * if ClothBlendWeight is 1.0, it shows only simulated positions and if it is 0.0, it shows only key-framed animation
		 */
		float ClothBlendWeight = 1.0f;
		uint32 NumInfluencesPerVertex = 1;

	private:
		// fallback for ClothSimulPositionNormalBuffer if the shadermodel doesn't allow it
		TUniformBufferRef<FAPEXClothUniformShaderParameters> APEXClothUniformBuffer;
		// 
		FVertexBufferAndSRV ClothSimulPositionNormalBuffer[2];
		// from GFrameNumber, to detect pause and old data when an object was not rendered for some time
		uint32 BufferFrameNumber[2];

		/**
		 * Matrix to apply to positions/normals
		 */
		FMatrix44f ClothToLocal[2];

		/** Whether to double buffer. */
		bool bDoubleBuffer = false;

		// @return 0 / 1, index into ClothSimulPositionNormalBuffer[]
		uint32 GetMostRecentIndex(uint32 FrameNumber) const
		{
			if (!bDoubleBuffer)
			{
				return 0;
			}

			if(BufferFrameNumber[0] == -1)
			{
				//ensure(BufferFrameNumber[1] != -1);

				return 1;
			}
			else if(BufferFrameNumber[1] == -1)
			{
				//ensure(BufferFrameNumber[0] != -1);
				return 0;
			}

			// should handle warp around correctly, did some basic testing
			uint32 Age0 = FrameNumber - BufferFrameNumber[0];
			uint32 Age1 = FrameNumber - BufferFrameNumber[1];

			return (Age0 > Age1) ? 1 : 0;
		}

		// @return 0/1, index into ClothSimulPositionNormalBuffer[]
		uint32 GetOldestIndex(uint32 FrameNumber) const
		{
			if (!bDoubleBuffer)
			{
				return 0;
			}

			if(BufferFrameNumber[0] == -1)
			{
				return 0;
			}
			else if(BufferFrameNumber[1] == -1)
			{
				return 1;
			}

			// should handle warp around correctly (todo: test)
			uint32 Age0 = FrameNumber - BufferFrameNumber[0];
			uint32 Age1 = FrameNumber - BufferFrameNumber[1];

			return (Age0 > Age1) ? 0 : 1;
		}

		bool DoWeHavePreviousData() const
		{
			if(BufferFrameNumber[0] == -1 || BufferFrameNumber[1] == -1)
			{
				return false;
			}
			
			int32 Diff = BufferFrameNumber[0] - BufferFrameNumber[1];

			uint32 DiffAbs = FMath::Abs(Diff);

			// threshold is >1 because there could be in between frames e.g. HitProxyRendering
			// We should switch to TickNumber to solve this
			return DiffAbs <= 2;
		}

		void Reset()
		{
			// both are not valid
			BufferFrameNumber[0] = -1;
			BufferFrameNumber[1] = -1;

			ClothToLocal[0] = FMatrix44f::Identity;
			ClothToLocal[1] = FMatrix44f::Identity;

			bDoubleBuffer = false;
		}
	};

	FGPUBaseSkinAPEXClothVertexFactory(uint32 InNumInfluencesPerVertex)
	{
		ClothShaderData.NumInfluencesPerVertex = InNumInfluencesPerVertex;
	}

	virtual ~FGPUBaseSkinAPEXClothVertexFactory() {}

	/** accessor */
	FORCEINLINE ClothShaderType& GetClothShaderData()
	{
		return ClothShaderData;
	}

	FORCEINLINE const ClothShaderType& GetClothShaderData() const
	{
		return ClothShaderData;
	}

	static bool IsClothEnabled(EShaderPlatform Platform);

	virtual FGPUBaseSkinVertexFactory* GetVertexFactory() = 0;
	virtual const FGPUBaseSkinVertexFactory* GetVertexFactory() const = 0;

	/** Get buffer containing cloth influences. */
	virtual FShaderResourceViewRHIRef GetClothBuffer() { return nullptr; }
	virtual const FShaderResourceViewRHIRef GetClothBuffer() const { return nullptr; }
	/** Get offset from vertex index to cloth influence index at a given vertex index. The offset will be constant for all vertices in the same section. */
	virtual uint32 GetClothIndexOffset(uint32 VertexIndex, uint32 LODBias = 0) const { return 0; }

protected:
	ClothShaderType ClothShaderData;

	/** Pool of buffers for clothing simulation data */
	static TGlobalResource<FClothBufferPool> ClothSimulDataBufferPool;
};

/** Stream component data bound to Apex cloth vertex factory */
struct FGPUSkinAPEXClothDataType : public FGPUSkinDataType
{
	FShaderResourceViewRHIRef ClothBuffer;
	// Packed Map: u32 Key, u32 Value
	TArray<FClothBufferIndexMapping> ClothIndexMapping;
};

/** Vertex factory with vertex stream components for GPU-skinned and morph target streams */
template<GPUSkinBoneInfluenceType BoneInfluenceType>
class TGPUSkinAPEXClothVertexFactory : public FGPUBaseSkinAPEXClothVertexFactory, public TGPUSkinVertexFactory<BoneInfluenceType>
{
	DECLARE_VERTEX_FACTORY_TYPE(TGPUSkinAPEXClothVertexFactory<BoneInfluenceType>);

	typedef TGPUSkinVertexFactory<BoneInfluenceType> Super;

public:
	inline FShaderResourceViewRHIRef GetClothBuffer() override
	{
		return ClothDataPtr ? ClothDataPtr->ClothBuffer : nullptr;
	}

	const FShaderResourceViewRHIRef GetClothBuffer() const override
	{
		return ClothDataPtr ? ClothDataPtr->ClothBuffer : nullptr;
	}

	uint32 GetClothIndexOffset(uint32 VertexIndex, uint32 LODBias = 0) const override
	{
		if (ClothDataPtr)
		{
			for (const FClothBufferIndexMapping& Mapping : ClothDataPtr->ClothIndexMapping)
			{
				if (Mapping.BaseVertexIndex == VertexIndex)
				{
					return Mapping.MappingOffset + Mapping.LODBiasStride * LODBias;
				}
			}
		}

		checkf(0, TEXT("Cloth Index Mapping not found for Vertex Index %u"), VertexIndex);
		return 0;
	}

	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	TGPUSkinAPEXClothVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, uint32 InNumVertices, uint32 InNumInfluencesPerVertex)
		: FGPUBaseSkinAPEXClothVertexFactory(InNumInfluencesPerVertex)
		, TGPUSkinVertexFactory<BoneInfluenceType>(InFeatureLevel, InNumVertices)
	{}

	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

	/**
	* An implementation of the interface used by TSynchronizedResource to 
	* update the resource with new data from the game thread.
	* @param	InData - new stream component data
	*/
	virtual void SetData(const FGPUSkinDataType* InData) override;

	virtual FGPUBaseSkinVertexFactory* GetVertexFactory() override
	{
		return this;
	}

	virtual const FGPUBaseSkinVertexFactory* GetVertexFactory() const override
	{
		return this;
	}

	virtual FGPUBaseSkinAPEXClothVertexFactory* GetClothVertexFactory() override 
	{
		return this; 
	}
	
	virtual FGPUBaseSkinAPEXClothVertexFactory const* GetClothVertexFactory() const override 
	{
		return this; 
	}

	// FRenderResource interface.

	/**
	* Creates declarations for each of the vertex stream components and
	* initializes the device resource
	*/
	virtual void InitRHI() override;
	virtual void ReleaseDynamicRHI() override;

protected:
	/** Alias pointer to TUniquePtr<FGPUSkinDataType> Data of FGPUBaseSkinVertexFactory. Note memory isn't managed through this pointer. */
	FGPUSkinAPEXClothDataType* ClothDataPtr = nullptr;
};


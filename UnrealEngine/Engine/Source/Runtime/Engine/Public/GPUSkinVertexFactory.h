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

template <class T> class TConsoleVariableData;

// Uniform buffer for APEX cloth
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FAPEXClothUniformShaderParameters,)
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
	FVertexBufferAndSRV CreateResource(FRHICommandListBase& RHICmdList, FSharedPoolPolicyData::CreationArguments Args);
	
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
	FVertexBufferAndSRV CreateResource(FRHICommandListBase& RHICmdList, FSharedPoolPolicyData::CreationArguments Args);
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
		void UpdateBoneData(FRHICommandList& RHICmdList, const TArray<FMatrix44f>& ReferenceToLocalMatrices,
			const TArray<FBoneIndexType>& BoneMap, uint32 RevisionNumber, ERHIFeatureLevel::Type FeatureLevel, 
			bool bUseSkinCache, bool bForceUpdateImmediately, const FName& AssetPathName);

		void ReleaseBoneData()
		{
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
				checkf(bPrevious, TEXT("Trying to access current bone buffer for reading, but it is null. BoneBuffer[0] = %p, BoneBuffer[1] = %p, CurrentRevisionNumber = %u, PreviousRevisionNumber = %u"),
					BoneBuffer[0].VertexBufferRHI.GetReference(), BoneBuffer[1].VertexBufferRHI.GetReference(), CurrentRevisionNumber, PreviousRevisionNumber);

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
		uint64 UpdatedFrameNumber = 0;

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
				// If the revision number has incremented too much, ignore the request and use the current buffer.
				// With ClearMotionVector calls, we intentionally increment revision number to retrieve current buffer for bPrevious true.
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
	UE_DEPRECATED(5.3, "Use SetData with a command list.")
	void SetData(const FGPUSkinDataType* InData);

	virtual void SetData(FRHICommandListBase& RHICmdList, const FGPUSkinDataType* InData);

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
	
	ENGINE_API static bool UseUnlimitedBoneInfluences(uint32 MaxBoneInfluences, const ITargetPlatform* TargetPlatform = nullptr);
	ENGINE_API static bool GetUnlimitedBoneInfluences(const ITargetPlatform* TargetPlatform = nullptr);

	/*
	 * Returns the maximum number of bone influences that should be used for a skeletal mesh, given
	 * the user-requested limit.
	 * 
	 * If the requested limit is 0, the limit will be determined from the project settings.
	 * 
	 * The return value is guaranteed to be greater than zero, but note that it may be higher than
	 * the maximum supported bone influences.
	 */
	ENGINE_API static int32 GetBoneInfluenceLimitForAsset(int32 AssetProvidedLimit, const ITargetPlatform* TargetPlatform = nullptr);

	/**
	 * Returns true if mesh LODs with Unlimited Bone Influences must always be rendered using a
	 * Mesh Deformer for the given shader platform.
	 */
	ENGINE_API static bool GetAlwaysUseDeformerForUnlimitedBoneInfluences(EShaderPlatform Platform);

	/** Morph vertex factory functions */
	virtual void UpdateMorphVertexStream(const class FMorphVertexBuffer* MorphVertexBuffer) {}
	virtual const class FMorphVertexBuffer* GetMorphVertexBuffer(bool bPrevious) const { return nullptr; }
	virtual uint32 GetMorphVertexBufferUpdatedFrameNumber() const { return 0; }
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

	void CopyDataTypeForLocalVertexFactory(FLocalVertexFactory::FDataType& OutDestData) const;

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
class TGPUSkinVertexFactory : public FGPUBaseSkinVertexFactory
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
	static void GetVertexElements(ERHIFeatureLevel::Type FeatureLevel, EVertexInputStreamType InputStreamType, FGPUSkinDataType& GPUSkinData, FVertexDeclarationElementList& Elements);
	
	/** FGPUBaseSkinVertexFactory overrides */
	virtual void UpdateMorphVertexStream(const class FMorphVertexBuffer* MorphVertexBuffer) override;
	virtual const class FMorphVertexBuffer* GetMorphVertexBuffer(bool bPrevious) const override;
	virtual uint32 GetMorphVertexBufferUpdatedFrameNumber() const override;

	// FRenderResource interface.
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

protected:
	/**
	* Add the decl elements for the streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/
	virtual void AddVertexElements(FVertexDeclarationElementList& OutElements) override;

	static void GetVertexElements(ERHIFeatureLevel::Type FeatureLevel, EVertexInputStreamType InputStreamType, FGPUSkinDataType& GPUSkinData, FVertexDeclarationElementList& Elements, FVertexStreamList& InOutStreams, int32& OutMorphDeltaStreamIndex);

private:
	int32 MorphDeltaStreamIndex = -1;
};


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

		void UpdateClothSimulData(FRHICommandList& RHICmdList, TConstArrayView<FVector3f> InSimulPositions, TConstArrayView<FVector3f> InSimulNormals, uint32 RevisionNumber, 
									ERHIFeatureLevel::Type FeatureLevel, bool bForceUpdateImmediately, const FName& AssetPathName);

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

		void SetCurrentRevisionNumber(uint32 RevisionNumber);

		FVertexBufferAndSRV& GetClothBufferForWriting();
		bool HasClothBufferForReading(bool bPrevious) const;
		const FVertexBufferAndSRV& GetClothBufferForReading(bool bPrevious) const;

		FMatrix44f& GetClothToLocalForWriting();
		const FMatrix44f& GetClothToLocalForReading(bool bPrevious) const;

		/**
		 * weight to blend between simulated positions and key-framed poses
		 * if ClothBlendWeight is 1.0, it shows only simulated positions and if it is 0.0, it shows only key-framed animation
		 */
		float ClothBlendWeight = 1.0f;
		/** Scale of the owner actor */
		FVector3f WorldScale = FVector3f::OneVector;
		uint32 NumInfluencesPerVertex = 1;

	private:
		// Helper for GetClothBufferIndexForWriting and GetClothBufferIndexForReading
		uint32 GetClothBufferIndexInternal(bool bPrevious) const;
		// Helper for GetClothBufferForWriting and GetClothToLocalForWriting
		uint32 GetClothBufferIndexForWriting() const;
		// Helper for GetClothBufferForReading and GetClothToLocalForReading
		uint32 GetClothBufferIndexForReading(bool bPrevious) const;

		// fallback for ClothSimulPositionNormalBuffer if the shadermodel doesn't allow it
		TUniformBufferRef<FAPEXClothUniformShaderParameters> APEXClothUniformBuffer;
		// 
		FVertexBufferAndSRV ClothSimulPositionNormalBuffer[2];

		/**
		 * Matrix to apply to positions/normals
		 */
		FMatrix44f ClothToLocal[2];

		/** Whether to double buffer. */
		bool bDoubleBuffer = false;

		// 0 / 1 to index into BoneBuffer
		uint32 CurrentBuffer = 0;
		// RevisionNumber Tracker
		uint32 PreviousRevisionNumber = 0;
		uint32 CurrentRevisionNumber = 0;

		void Reset()
		{
			CurrentBuffer = 0;
			PreviousRevisionNumber = 0;
			CurrentRevisionNumber = 0;

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

	/**
	 * Destructor takes care of the Data pointer. Since FGPUBaseSkinVertexFactory does not know the real type of the Data,
	 * delete the data here instead.
	 */
	virtual ~TGPUSkinAPEXClothVertexFactory() override
	{
		checkf(!ClothDataPtr->ClothBuffer.IsValid(), TEXT("ClothBuffer RHI resource should have been released in ReleaseRHI"));
		delete ClothDataPtr;
		(void)this->Data.Release();
	}

	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

	/**
	* An implementation of the interface used by TSynchronizedResource to 
	* update the resource with new data from the game thread.
	* @param	InData - new stream component data
	*/
	virtual void SetData(FRHICommandListBase& RHICmdList, const FGPUSkinDataType* InData) override;

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
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

protected:
	/** Alias pointer to TUniquePtr<FGPUSkinDataType> Data of FGPUBaseSkinVertexFactory. Note memory isn't managed through this pointer. */
	FGPUSkinAPEXClothDataType* ClothDataPtr = nullptr;
};


/**
 * Vertex factory with vertex stream components for GPU-skinned streams.
 * This enables Passthrough mode where vertices have been pre-skinned.
 * Individual vertex attributes can be flagged so that they can be overriden by externally owned buffers.
 */
class FGPUSkinPassthroughVertexFactory : public FLocalVertexFactory
{
public:
	FGPUSkinPassthroughVertexFactory(ERHIFeatureLevel::Type InFeatureLevel);

	// Begin FVertexFactory Interface.
	bool SupportsPositionOnlyStream() const override { return false; }
	bool SupportsPositionAndNormalOnlyStream() const override { return false; }
	// End FVertexFactory Interface.

	/**
	 * Reset all added vertex attributes and SRVs.
	 * This doesn't reset the vertex factory itself. Call SetData() to do that.
	 */
	void ResetVertexAttributes();

	/** Vertex attributes that we can override. */
	enum EVertexAtttribute
	{
		VertexPosition,
		VertexTangent,
		VertexColor,
		VertexTexCoord0,
		VertexTexCoord1,
		VertexTexCoord2,
		VertexTexCoord3,
		VertexTexCoord4,
		VertexTexCoord5,
		VertexTexCoord6,
		VertexTexCoord7,
		NumAttributes
	};
	
	/** SRVs that we can provide. */
	enum EShaderResource
	{
		Position,
		PreviousPosition,
		Tangent,
		Color,
		TexCoord,
		NumShaderResources
	};

	/** Structure used for calls to SetVertexAttributes(). */
	struct FAddVertexAttributeDesc
	{
		FAddVertexAttributeDesc() : SRVs(InPlace, nullptr) {}

		/** Frame number at animation update. Used to determine if animation motion is valid and needs to output velocity. */
		uint32 FrameNumber = ~0U;
		/** Vertex attributes to use in vertex declaration. */
		TArray<EVertexAtttribute, TFixedAllocator<EVertexAtttribute::NumAttributes>> VertexAttributes;
		/** SRVs for binding. These are only be used by platforms that support manual vertex fetch. */
		TStaticArray<FRHIShaderResourceView*, EShaderResource::NumShaderResources> SRVs;
	};

	/** 
	 * Set vertex attributes and SRVs to be used. 
	 * The vertex declaration is made by accumulating attributes set here along with all that already been added by previous calls to this function.
	 * If any attributes are being added for the first time then we pay the cost to recreate the vertex declaration here.
	 * The SRVs are cached per attribute. If any passed in SRV is changed from the cached value then we recreate the vertex factory uniform buffer here.
	 * Note that on platforms that support manual vertex fetch, only Position will be in the final vertex stream and other attributes will be read through an SRV.
	 */
	void SetVertexAttributes(FRHICommandListBase& RHICmdList, FGPUBaseSkinVertexFactory const* InSourceVertexFactory, FAddVertexAttributeDesc const& InDesc);

	UE_DEPRECATED(5.4, "SetVertexAttributes requires a command list.")
	void SetVertexAttributes(FGPUBaseSkinVertexFactory const* InSourceVertexFactory, FAddVertexAttributeDesc const& InDesc);

	/** 
	 * Get the vertex stream index for a vertex attribute.
	 * This will be -1 for attributes that haven't been set in AddVertexAttributes().
	 * It may also be -1 for attributes that are read through manual vertex fetch.
	 */
	int32 GetAttributeStreamIndex(EVertexAtttribute InAttribute) const;

private:
	void OverrideAttributeData();
	void OverrideSRVs(FGPUBaseSkinVertexFactory const* InSourceVertexFactory);
	void BuildStreamIndices();
	void CreateUniformBuffer();
	void CreateLooseUniformBuffer(FRHICommandListBase& RHICmdList, FGPUBaseSkinVertexFactory const* InSourceVertexFactory, uint32 InFrameNumber);

	uint32 VertexAttributeMask;
	TStaticArray<int32, EVertexAtttribute::NumAttributes> StreamIndices;
	TStaticArray<FRHIShaderResourceView*, EShaderResource::NumShaderResources> SRVs;
	uint32 UpdatedFrameNumber;

	static TStaticArray<FVertexBuffer, EVertexAtttribute::NumAttributes> DummyVBs;
};

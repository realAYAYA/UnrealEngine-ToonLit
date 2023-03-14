// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NaniteShared.h"
#include "NaniteCullRaster.h"
#include "MeshPassProcessor.h"
#include "GBufferInfo.h"

static constexpr uint32 NANITE_MAX_MATERIALS = 64;

// TODO: Until RHIs no longer set stencil ref to 0 on a PSO change, this optimization 
// is actually worse (forces a context roll per unique material draw, back to back).
#define NANITE_MATERIAL_STENCIL 0

struct FNaniteMaterialPassCommand;
struct FLumenMeshCaptureMaterialPass;
class  FLumenCardPassUniformParameters;
class  FCardPageRenderData;

// VertexCountPerInstance
// InstanceCount
// StartVertexLocation
// StartInstanceLocation
#define NANITE_DRAW_INDIRECT_ARG_COUNT 4

class FNaniteCommandInfo
{
public:
	explicit FNaniteCommandInfo() = default;

	inline void SetStateBucketId(int32 InStateBucketId)
	{
		check(InStateBucketId < NANITE_MAX_STATE_BUCKET_ID);
		StateBucketId = InStateBucketId;
	}

	inline int32 GetStateBucketId() const
	{
		check(StateBucketId < NANITE_MAX_STATE_BUCKET_ID);
		return StateBucketId;
	}

	inline void Reset()
	{
		StateBucketId = INDEX_NONE;
	}

	inline uint32 GetMaterialId() const
	{
		return GetMaterialId(GetStateBucketId());
	}

	inline void SetMaterialSlot(int32 InMaterialSlot)
	{
		MaterialSlot = InMaterialSlot;
	}

	inline int32 GetMaterialSlot() const
	{
		return MaterialSlot;
	}

	static uint32 GetMaterialId(int32 StateBucketId)
	{
		float DepthId = GetDepthId(StateBucketId);
		return *reinterpret_cast<uint32*>(&DepthId);
	}

	static float GetDepthId(int32 StateBucketId)
	{
		return float(StateBucketId + 1) / float(NANITE_MAX_STATE_BUCKET_ID);
	}

private:
	// Stores the index into FScene::NaniteDrawCommands of the corresponding FMeshDrawCommand
	int32 StateBucketId = INDEX_NONE;

	int32 MaterialSlot = INDEX_NONE;
};

struct FNaniteMaterialSlot
{
	struct FPacked
	{
		uint32 Data[2];
	};

	FNaniteMaterialSlot()
	: ShadingId(0xFFFF)
	, RasterId(0xFFFF)
	, SecondaryRasterId(0xFFFF)
	{
	}

	inline FPacked Pack() const
	{
		FPacked Ret;
		Ret.Data[0] = (ShadingId << 16u | RasterId);
		Ret.Data[1] = SecondaryRasterId == 0xFFFFu ? 0xFFFFFFFFu : SecondaryRasterId;
		return Ret;
	}

	uint16 ShadingId;
	uint16 RasterId;
	uint16 SecondaryRasterId;
};

struct FNaniteMaterialPassCommand
{
	FNaniteMaterialPassCommand(const FMeshDrawCommand& InMeshDrawCommand)
	: MeshDrawCommand(InMeshDrawCommand)
	, MaterialDepth(0.0f)
	, MaterialSlot(INDEX_NONE)
	, SortKey(MeshDrawCommand.CachedPipelineId.GetId())
	{
	}

	bool operator < (const FNaniteMaterialPassCommand& Other) const
	{
		return SortKey < Other.SortKey;
	}

	FMeshDrawCommand MeshDrawCommand;
	float MaterialDepth = 0.0f;
	int32 MaterialSlot = INDEX_NONE;
	uint64 SortKey = 0;
};

class FNaniteMultiViewMaterialVS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNaniteMultiViewMaterialVS);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float,   MaterialDepth)
		SHADER_PARAMETER(uint32,  InstanceBaseOffset)
	END_SHADER_PARAMETER_STRUCT()

	FNaniteMultiViewMaterialVS() = default;

	FNaniteMultiViewMaterialVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FNaniteGlobalShader(Initializer)
	{
		BindForLegacyShaderParameters<FParameters>(this, Initializer.PermutationId, Initializer.ParameterMap, false);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NANITE_MATERIAL_MULTIVIEW"), 1);
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
	}

	void GetElementShaderBindings(
		const FShaderMapPointerTable& PointerTable,
		const FScene* Scene,
		const FSceneView* ViewIfDynamicMeshCommand,
		const FVertexFactory* VertexFactory,
		const EVertexInputStreamType InputStreamType,
		const FStaticFeatureLevel FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& MeshBatch,
		const FMeshBatchElement& BatchElement,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
	}

private:
	LAYOUT_FIELD(FShaderParameter, MaterialDepth);
};

class FNaniteIndirectMaterialVS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNaniteIndirectMaterialVS);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float,   MaterialDepth)
		SHADER_PARAMETER(uint32,  MaterialSlot)
		SHADER_PARAMETER(uint32,  TileRemapCount)
	END_SHADER_PARAMETER_STRUCT()

	FNaniteIndirectMaterialVS() = default;

	FNaniteIndirectMaterialVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FNaniteGlobalShader(Initializer)
	{
		BindForLegacyShaderParameters<FParameters>(this, Initializer.PermutationId, Initializer.ParameterMap, false);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NANITE_MATERIAL_MULTIVIEW"), 0);
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
	}

	void GetElementShaderBindings(
		const FShaderMapPointerTable& PointerTable,
		const FScene* Scene,
		const FSceneView* ViewIfDynamicMeshCommand,
		const FVertexFactory* VertexFactory,
		const EVertexInputStreamType InputStreamType,
		const FStaticFeatureLevel FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& MeshBatch,
		const FMeshBatchElement& BatchElement,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
	}

private:
	LAYOUT_FIELD(FShaderParameter, MaterialDepth);
};

struct FNaniteMaterialEntry
{
	FNaniteMaterialEntry()
	: ReferenceCount(0)
	, MaterialId(0)
	, MaterialSlot(INDEX_NONE)
#if WITH_DEBUG_VIEW_MODES
	, InstructionCount(0)
#endif
	, bNeedUpload(false)
	, bWPOEnabled(false)
	{
	}

	FNaniteMaterialEntry(FNaniteMaterialEntry&& Other)
	: ReferenceCount(Other.ReferenceCount)
	, MaterialId(Other.MaterialId)
	, MaterialSlot(Other.MaterialSlot)
#if WITH_DEBUG_VIEW_MODES
	, InstructionCount(Other.InstructionCount)
#endif
	, bNeedUpload(false)
	, bWPOEnabled(Other.bWPOEnabled)
	{
		checkSlow(!Other.bNeedUpload);
	}

	uint32 ReferenceCount;
	uint32 MaterialId;
	int32 MaterialSlot;
#if WITH_DEBUG_VIEW_MODES
	uint32 InstructionCount;
#endif
	bool bNeedUpload;
	bool bWPOEnabled;
};

struct FNaniteMaterialEntryKeyFuncs : TDefaultMapHashableKeyFuncs<FMeshDrawCommand, FNaniteMaterialEntry, false>
{
	static inline bool Matches(KeyInitType A, KeyInitType B)
	{
		return A.MatchesForDynamicInstancing(B);
	}

	static inline uint32 GetKeyHash(KeyInitType Key)
	{
		return Key.GetDynamicInstancingHash();
	}
};

using FNaniteMaterialEntryMap = Experimental::TRobinHoodHashMap<FMeshDrawCommand, FNaniteMaterialEntry, FNaniteMaterialEntryKeyFuncs>;

class FNaniteMaterialCommands
{
public:
	typedef Experimental::FHashType FCommandHash;
	typedef Experimental::FHashElementId FCommandId;

	// The size of one entry in the material slot byte address buffer
	static const uint32 MaterialSlotSize = sizeof(FNaniteMaterialSlot::FPacked);

public:
	FNaniteMaterialCommands(uint32 MaxMaterials = NANITE_MAX_MATERIALS);
	~FNaniteMaterialCommands();

	void Release();

	FNaniteCommandInfo Register(FMeshDrawCommand& Command, FCommandHash CommandHash, uint32 InstructionCount, bool bWPOEnabled);
	FNaniteCommandInfo Register(FMeshDrawCommand& Command, uint32 InstructionCount, bool bWPOEnabled) { return Register(Command, ComputeCommandHash(Command), InstructionCount, bWPOEnabled); }
	void Unregister(const FNaniteCommandInfo& CommandInfo);

	inline const FCommandHash ComputeCommandHash(const FMeshDrawCommand& DrawCommand) const
	{
		return EntryMap.ComputeHash(DrawCommand);
	}

	inline const FCommandId FindIdByHash(const FCommandHash CommandHash, const FMeshDrawCommand& DrawCommand) const
	{
		return EntryMap.FindIdByHash(CommandHash, DrawCommand);
	}

	inline const FCommandId FindIdByCommand(const FMeshDrawCommand& DrawCommand) const
	{
		const FCommandHash CommandHash = ComputeCommandHash(DrawCommand);
		return FindIdByHash(CommandHash, DrawCommand);
	}

	inline const FCommandId FindOrAddIdByHash(const FCommandHash HashValue, const FMeshDrawCommand& DrawCommand)
	{
		return EntryMap.FindOrAddIdByHash(HashValue, DrawCommand, FNaniteMaterialEntry());
	}

	inline void RemoveById(const FCommandId Id)
	{
		EntryMap.RemoveByElementId(Id);
	}

	inline const FMeshDrawCommand& GetCommand(const FCommandId Id) const
	{
		return EntryMap.GetByElementId(Id).Key;
	}

	inline const FNaniteMaterialEntry& GetPayload(const FCommandId Id) const
	{
		return EntryMap.GetByElementId(Id).Value;
	}

	inline FNaniteMaterialEntry& GetPayload(const FCommandId Id)
	{
		return EntryMap.GetByElementId(Id).Value;
	}

	inline const FNaniteMaterialEntryMap& GetCommands() const
	{
		return EntryMap;
	}

	void UpdateBufferState(FRDGBuilder& GraphBuilder, uint32 NumPrimitives);

	class FUploader
	{
	public:
		void Lock(FRHICommandListBase& RHICmdList);

		void* GetMaterialSlotPtr(uint32 PrimitiveIndex, uint32 EntryCount);
#if WITH_EDITOR
		void* GetHitProxyTablePtr(uint32 PrimitiveIndex, uint32 EntryCount);
#endif

		void Unlock(FRHICommandListBase& RHICmdList);

	private:
		struct FMaterialUploadEntry
		{
			FMaterialUploadEntry() = default;
			FMaterialUploadEntry(const FNaniteMaterialEntry& Entry)
				: MaterialId(Entry.MaterialId)
				, MaterialSlot(Entry.MaterialSlot)
#if WITH_DEBUG_VIEW_MODES
				, InstructionCount(Entry.InstructionCount)
#endif
			{}

			uint32 MaterialId;
			int32 MaterialSlot;
#if WITH_DEBUG_VIEW_MODES
			uint32 InstructionCount;
#endif
		};

		TArray<FMaterialUploadEntry, FSceneRenderingArrayAllocator> DirtyMaterialEntries;

		FRDGScatterUploader* MaterialSlotUploader = nullptr;
		FRDGScatterUploader* HitProxyTableUploader = nullptr;
		FRDGScatterUploader* MaterialDepthUploader = nullptr;
		FRDGScatterUploader* MaterialEditorUploader = nullptr;
		int32 MaxMaterials = 0;

		friend FNaniteMaterialCommands;
	};

	FUploader* Begin(FRDGBuilder& GraphBuilder, uint32 NumPrimitives, uint32 NumPrimitiveUpdates);

	void Finish(FRDGBuilder& GraphBuilder, FRDGExternalAccessQueue& ExternalAccessQueue, FUploader* Uploader);

#if WITH_EDITOR
	FRHIShaderResourceView* GetHitProxyTableSRV() const { return HitProxyTableDataBuffer->GetSRV(); }
#endif

	FRHIShaderResourceView* GetMaterialSlotSRV() const { return MaterialSlotDataBuffer->GetSRV(); }
	FRHIShaderResourceView* GetMaterialDepthSRV() const { return MaterialDepthDataBuffer->GetSRV(); }
#if WITH_DEBUG_VIEW_MODES
	FRHIShaderResourceView* GetMaterialEditorSRV() const { return MaterialEditorDataBuffer->GetSRV(); }
#endif

	inline const int32 GetHighestMaterialSlot() const
	{
		return MaterialSlotAllocator.GetMaxSize();
	}

private:
	FNaniteMaterialEntryMap EntryMap;

	uint32 MaxMaterials = 0;
	uint32 NumPrimitiveUpdates = 0;
	uint32 NumHitProxyTableUpdates = 0;
	uint32 NumMaterialSlotUpdates = 0;
	uint32 NumMaterialDepthUpdates = 0;

	FRDGAsyncScatterUploadBuffer MaterialSlotUploadBuffer;
	TRefCountPtr<FRDGPooledBuffer> MaterialSlotDataBuffer;

	FRDGAsyncScatterUploadBuffer HitProxyTableUploadBuffer;
	TRefCountPtr<FRDGPooledBuffer> HitProxyTableDataBuffer;

	FGrowOnlySpanAllocator	MaterialSlotAllocator;

	FRDGAsyncScatterUploadBuffer MaterialDepthUploadBuffer; // 1 uint per slot (Depth Value)
	TRefCountPtr<FRDGPooledBuffer> MaterialDepthDataBuffer;

#if WITH_DEBUG_VIEW_MODES
	FRDGAsyncScatterUploadBuffer MaterialEditorUploadBuffer; // 1 uint per slot (VS and PS instruction count)
	TRefCountPtr<FRDGPooledBuffer> MaterialEditorDataBuffer;
#endif
};

inline void LockIfValid(FRHICommandListBase& RHICmdList, FNaniteMaterialCommands::FUploader* Uploader)
{
	if (Uploader)
	{
		Uploader->Lock(RHICmdList);
	}
}

inline void UnlockIfValid(FRHICommandListBase& RHICmdList, FNaniteMaterialCommands::FUploader* Uploader)
{
	if (Uploader)
	{
		Uploader->Unlock(RHICmdList);
	}
}

extern bool UseComputeDepthExport();

namespace Nanite
{

void EmitDepthTargets(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	const FIntVector4& PageConstants,
	FRDGBufferRef VisibleClustersSWHW,
	FRDGBufferRef ViewsBuffer,
	FRDGTextureRef SceneDepth,
	FRDGTextureRef VisBuffer64,
	FRDGTextureRef VelocityBuffer,
	FRDGTextureRef& OutMaterialDepth,
	FRDGTextureRef& OutMaterialResolve,
	bool bPrePass,
	bool bStencilMask
);

void DrawBasePass(
	FRDGBuilder& GraphBuilder,
	TArray<FNaniteMaterialPassCommand, SceneRenderingAllocator>& NaniteMaterialPassCommands,
	const FSceneRenderer& SceneRenderer,
	const FSceneTextures& SceneTextures,
	const FDBufferTextures& DBufferTextures,
	const FScene& Scene,
	const FViewInfo& View,
	const FRasterResults& RasterResults
);

void DrawLumenMeshCapturePass(
	FRDGBuilder& GraphBuilder,
	FScene& Scene,
	FViewInfo* SharedView,
	TArrayView<const FCardPageRenderData> CardPagesToRender,
	const FCullingContext& CullingContext,
	const FRasterContext& RasterContext,
	FLumenCardPassUniformParameters* PassUniformParameters,
	FRDGBufferSRVRef RectMinMaxBufferSRV,
	uint32 NumRects,
	FIntPoint ViewportSize,
	FRDGTextureRef AlbedoAtlasTexture,
	FRDGTextureRef NormalAtlasTexture,
	FRDGTextureRef EmissiveAtlasTexture,
	FRDGTextureRef DepthAtlasTexture
);

EGBufferLayout GetGBufferLayoutForMaterial(const FMaterial& Material);

} // namespace Nanite
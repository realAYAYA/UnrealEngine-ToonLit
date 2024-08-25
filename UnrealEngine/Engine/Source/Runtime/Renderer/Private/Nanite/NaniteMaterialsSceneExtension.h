// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpanAllocator.h"
#include "Containers/Map.h"
#include "SceneExtensions.h"
#include "NaniteDefinitions.h"
#include "SpanAllocator.h"
#include "NaniteMaterials.h"
#include "RendererPrivateUtils.h"

class FNaniteMaterialsParameters;

namespace Nanite
{

class FMaterialsSceneExtension : public ISceneExtension
{
	DECLARE_SCENE_EXTENSION(FMaterialsSceneExtension);

public:
	class FUpdater : public ISceneExtensionUpdater
	{
		DECLARE_SCENE_EXTENSION_UPDATER(FUpdater, FMaterialsSceneExtension);

	public:
		FUpdater(FMaterialsSceneExtension& InSceneData);
		virtual void End();
		virtual void PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet) override;
		virtual void PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet) override;
		
		void PostCacheNaniteMaterialBins(
			FRDGBuilder& GraphBuilder,
			const TConstArrayView<FPrimitiveSceneInfo*>& SceneInfosWithStaticDrawListUpdate
		);

	private:
		FMaterialsSceneExtension* SceneData = nullptr;
		TConstArrayView<FPrimitiveSceneInfo*> AddedList;
		TConstArrayView<FPrimitiveSceneInfo*> MaterialUpdateList;
		TArray<int32, FSceneRenderingArrayAllocator> DirtyPrimitiveList;
		const bool bEnableAsync = true;
		bool bForceFullUpload = false;
		bool bDefragging = false;
	};

	class FRenderer : public ISceneExtensionRenderer
	{
		DECLARE_SCENE_EXTENSION_RENDERER(FRenderer, FMaterialsSceneExtension);
	
	public:
		FRenderer(FMaterialsSceneExtension& InSceneData) : SceneData(&InSceneData) {}
		virtual void UpdateSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& Buffer) override;

	private:
		FMaterialsSceneExtension* SceneData = nullptr;
	};

	friend class FUpdater;

	static bool ShouldCreateExtension(FScene& InScene);

	virtual void InitExtension(FScene& InScene) override;
	virtual ISceneExtensionUpdater* CreateUpdater() override;
	virtual ISceneExtensionRenderer* CreateRenderer() override;

#if WITH_EDITOR
	FRDGBufferRef CreateHitProxyIDBuffer(FRDGBuilder& GraphBuilder) const;
#endif

private:
	enum ETask : uint32
	{
		FreeBufferSpaceTask,
		InitPrimitiveDataTask,
		AllocMaterialBufferTask,
		UploadPrimitiveDataTask,
		UploadMaterialDataTask,
	#if WITH_EDITOR
		UpdateHitProxyIDsTask,
	#endif

		NumTasks
	};

	struct FPackedPrimitiveData
	{
		uint32 MaterialBufferOffset;
		uint32 MaterialMaxIndex : 8;
		uint32 MeshPassMask : 8;
		uint32 bHasUVDensities : 1;
	#if WITH_EDITOR
		uint32 HitProxyBufferOffset;
	#endif
	};

	struct FPrimitiveData
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = nullptr;
		uint32 MaterialBufferOffset = INDEX_NONE;
		uint32 MaterialBufferSizeDwords = 0;
		uint8 NumMaterials = 0;
		uint8 NumMeshPasses = 0;
		uint8 MeshPassMask = 0;
		bool bHasUVDensities = false;
	#if WITH_EDITOR
		uint32 HitProxyBufferOffset = INDEX_NONE;
	#endif

		FPackedPrimitiveData Pack() const
		{
			FPackedPrimitiveData Output;
			Output.MaterialBufferOffset = MaterialBufferOffset;
			Output.MaterialMaxIndex = NumMaterials - 1;
			Output.MeshPassMask = MeshPassMask;
			Output.bHasUVDensities = bHasUVDensities;
		#if WITH_EDITOR
			Output.HitProxyBufferOffset = HitProxyBufferOffset;
		#endif
			return Output;
		}
	};

	class FMaterialBuffers
	{
	public:
		FMaterialBuffers();

		TPersistentByteAddressBuffer<FPackedPrimitiveData> PrimitiveDataBuffer;
		TPersistentByteAddressBuffer<uint32> MaterialDataBuffer;
	};
	
	class FUploader
	{
	public:
		static constexpr int32 MaterialScatterStride = 2;
		TByteAddressBufferScatterUploader<FPackedPrimitiveData> PrimitiveDataUploader;
		TByteAddressBufferScatterUploader<uint32, MaterialScatterStride> MaterialDataUploader;
	};
	
	bool IsEnabled() const { return MaterialBuffers.IsValid(); }
	void SetEnabled(bool bEnabled);
	void SyncAllTasks() const { UE::Tasks::Wait(TaskHandles); }
	void FinishMaterialBufferUpload(
		FRDGBuilder& GraphBuilder,
		FNaniteMaterialsParameters* OutParams = nullptr
	);
	bool ProcessBufferDefragmentation();

	FScene* Scene = nullptr;
	FSpanAllocator MaterialBufferAllocator;
	TSparseArray<FPrimitiveData> PrimitiveData;
	TUniquePtr<FMaterialBuffers> MaterialBuffers;
	TUniquePtr<FUploader> MaterialUploader;
#if WITH_EDITOR
	FSpanAllocator HitProxyIDAllocator;
	TArray<uint32> HitProxyIDs;
#endif
	TStaticArray<UE::Tasks::FTask, NumTasks> TaskHandles;
};

} // namespace Nanite


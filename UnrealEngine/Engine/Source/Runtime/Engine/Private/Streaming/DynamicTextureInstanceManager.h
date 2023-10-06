// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DynamicTextureInstanceManager.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Streaming/TextureInstanceManager.h"
#include "Streaming/TextureInstanceTask.h"
#include "ContentStreaming.h"

/** 
 * A texture/mesh instance manager to manage dynamic components. 
 * The async view generated is duplicated so that the state can change freely.
 */
class FDynamicRenderAssetInstanceManager : public IRenderAssetInstanceManager
{
public:

	using FOnSyncDoneDelegate = TFunction<void (const FRemovedRenderAssetArray&)>;

	/** Contructor. */
	FDynamicRenderAssetInstanceManager(FOnSyncDoneDelegate&& InOnSyncDoneDelegate);
	~FDynamicRenderAssetInstanceManager();

	void RegisterTasks(RenderAssetInstanceTask::FDoWorkTask& AsyncTask);

	void IncrementalUpdate(FRemovedRenderAssetArray& RemovedRenderAssets, float Percentage);

	// Get all (non removed) components refered by the manager. Debug only.
	void GetReferencedComponents(TArray<const UPrimitiveComponent*>& Components);

	/** Remove all pending components that are marked for delete. This prevents searching in the pending list for each entry. */
	void OnPreGarbageCollect(FRemovedRenderAssetArray& RemovedRenderAssets);

	/*-----------------------------------
	------ IRenderAssetInstanceManager ------
	-----------------------------------*/

	/** Return whether this component can be managed by this manager. */
	bool IsReferenced(const UPrimitiveComponent* Component) const final;

	/** Return whether this component is be managed by this manager. */
	bool CanManage(const UPrimitiveComponent* Component) const final override;

	/** Add a component streaming data, the LevelContext gives support for precompiled data. */
	EAddComponentResult Add(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext, float MaxAllowedUIDensity = 0) final override;

	/** Remove a component, the RemovedRenderAssets is the list of textures or meshes not referred anymore. */
	 void Remove(const UPrimitiveComponent* Component, FRemovedRenderAssetArray* RemovedRenderAssets) final override;

	/** Notify the manager that an async view will be requested on the next frame. */
	void PrepareAsyncView() final override;

	/** Return a view of the data that has to be 100% thread safe. The content is allowed to be updated, but not memory must be reallocated. */
	const FRenderAssetInstanceView* GetAsyncView(bool bCreateIfNull) final override;

	/** Return the size taken for sub-allocation. */
	uint32 GetAllocatedSize() const final override;

	const FRenderAssetInstanceView* GetGameThreadView();

protected:
	
	/** Refresh component data (bounds, last render time, min and max view distance) - see TextureInstanceView. */
	void Refresh(float Percentage) final override;

	void OnCreateViewDone(FRenderAssetInstanceView* InView);
	void OnRefreshVisibilityDone(int32 BeginIndex, int32 EndIndex, const TArray<int32>& SkippedIndices, int32 FirstFreeBound, int32 LastUsedBound);

private:

	typedef RenderAssetInstanceTask::FCreateViewWithUninitializedBoundsTask FCreateViewTask;
	typedef RenderAssetInstanceTask::FRefreshFullTask FRefreshFullTask;

	struct FTasks
	{
		~FTasks()
		{
			SyncResults();
		}
		
		void SyncResults();
		
		void SyncRefreshFullTask();

		TRefCountPtr<FCreateViewTask> CreateViewTask;
		TRefCountPtr<FRefreshFullTask> RefreshFullTask;
	};

	/** The texture/mesh instances. Shared with the async task. */
	FRenderAssetDynamicInstanceStateTaskSync<FTasks> StateSync;

	/** A duplicate view for the async streaming task. */
	TRefCountPtr<const FRenderAssetInstanceView> AsyncView;

	/** Ranges from 0 to Bounds4Components.Num(). Used in the incremental update to update bounds and visibility. */
	int32 DirtyIndex;

	/** The valid bound index to be moved for defrag. */
	int32 PendingDefragSrcBoundIndex;
	/** The free bound index to be used as defrag destination. */
	int32 PendingDefragDstBoundIndex;

	/** The list of components to be processed (could have duplicates). */
	TArray<const UPrimitiveComponent*> PendingComponents;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSimCache.h"
#include "SNiagaraSimCacheOverview.h"

class SNiagaraSimCacheTreeView;
struct FNiagaraSimCacheComponentTreeItem;
struct FNiagaraSimCacheTreeItem;
struct FNiagaraSimCacheOverviewItem;
struct FNiagaraSystemSimCacheCaptureReply;
struct FNiagaraSimCacheDataBuffersLayout;
class UNiagaraComponent;

class NIAGARAEDITOR_API FNiagaraSimCacheViewModel : public TSharedFromThis<FNiagaraSimCacheViewModel>
{
public:
	struct FComponentInfo
	{
		FName Name = NAME_None;
		uint32 ComponentOffset = INDEX_NONE;
		bool bIsFloat = false;
		bool bIsHalf = false;
		bool bIsInt32 = false;
		bool bShowAsBool = false;
		UEnum* Enum = nullptr;
	};

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnViewDataChanged, bool)
	DECLARE_MULTICAST_DELEGATE(FOnSimCacheChanged)
	DECLARE_MULTICAST_DELEGATE(FOnBufferChanged)
	
	FNiagaraSimCacheViewModel();
	~FNiagaraSimCacheViewModel();

	void Initialize(TWeakObjectPtr<UNiagaraSimCache> SimCache);

	void UpdateSimCache(const FNiagaraSystemSimCacheCaptureReply& Reply);

	void SetupPreviewComponentAndInstance();

	TConstArrayView<FComponentInfo> GetCurrentComponentInfos() const { return *ComponentInfos; }

	TConstArrayView<FComponentInfo> GetComponentInfos(int32 InEmitterIndex) const { return  InEmitterIndex == INDEX_NONE ? SystemComponentInfos : EmitterComponentInfos[InEmitterIndex]; }

	TConstArrayView<FString> GetComponentFilters() const { return ComponentFilterArray; }

	void SetComponentFilters(const TArray<FString>& NewComponentFilterArray)
	{
		ComponentFilterArray.Empty();
		ComponentFilterArray.Append(NewComponentFilterArray);
		OnViewDataChangedDelegate.Broadcast(true);
	}

	bool IsComponentFilterActive() const { return bComponentFilterActive; }

	void SetComponentFilterActive(bool bNewActive);
	
	int32 GetNumInstances() const { return NumInstances; }

	int32 GetNumFrames() const;

	int32 GetFrameIndex() const { return FrameIndex; }

	void SetFrameIndex(const int32 InFrameIndex);

	int32 GetEmitterIndex() const { return EmitterIndex; };

	void SetEmitterIndex(int32 InEmitterIndex);

	FText GetComponentText(FName ComponentName, int32 InstanceIndex) const;

	bool IsCacheValid();

	int32 GetNumEmitterLayouts();

	FName GetEmitterLayoutName(int32 Index);

	FOnViewDataChanged& OnViewDataChanged();

	FOnSimCacheChanged& OnSimCacheChanged();

	FOnBufferChanged& OnBufferChanged();

	void OnCacheModified(UNiagaraSimCache* SimCache);

	void UpdateCachedFrame();

	// Build out component infos for each buffer in this cache
	void UpdateComponentInfos();

	void BuildTreeItemChildren(TSharedPtr<FNiagaraSimCacheTreeItem> InTreeItem, TWeakPtr<SNiagaraSimCacheTreeView> OwningTreeView);

	void RecursiveBuildTreeItemChildren(FNiagaraSimCacheTreeItem* Root, TSharedRef<FNiagaraSimCacheComponentTreeItem> Parent,
	FNiagaraTypeDefinition TypeDefinition, TWeakPtr<SNiagaraSimCacheTreeView> OwningTreeView);
	
	// Construct entries for the tree view.
	void BuildEntries(TWeakPtr<SNiagaraSimCacheTreeView> OwningTreeView);

	void UpdateCurrentEntries();

	TArray<TSharedRef<FNiagaraSimCacheTreeItem>>* GetCurrentRootEntries();


	TArray<TSharedRef<FNiagaraSimCacheOverviewItem>>* GetBufferEntries();

	UNiagaraComponent* GetPreviewComponent() { return PreviewComponent; }

private:
	void BuildComponentInfos(const FName Name, const UScriptStruct* Struct, TArray<FComponentInfo>& ComponentInfos);
	
	// The sim cache being viewed
	TWeakObjectPtr<UNiagaraSimCache>	WeakSimCache;

	// Component for preview scene
	UNiagaraComponent* PreviewComponent = nullptr;

	// Which frame of the cached sim is being viewed
	int32 FrameIndex = 0;
	
	// Which Emitter of the cached sim is being viewed
	int32 EmitterIndex = INDEX_NONE;

	// Number of particles in the given frame
	int32 NumInstances = 0;

	// Info about the variables in the cache that are currently being viewed in the spreadsheet
	TArray<FComponentInfo>* ComponentInfos = &SystemComponentInfos;

	// Cached Component infos for this system
	TArray<FComponentInfo>				SystemComponentInfos;
	TArray<TArray<FComponentInfo>>		EmitterComponentInfos;
	// TODO: Component infos for Data Interfaces?

	TArray<TSharedRef<FNiagaraSimCacheTreeItem>> RootEntries;
	TArray<TSharedRef<FNiagaraSimCacheTreeItem>> CurrentRootEntries;

	TArray<TSharedRef<FNiagaraSimCacheOverviewItem>> BufferEntries;

	// String filters gathered from the overview tree
	TArray<FString>						ComponentFilterArray;
	bool								bComponentFilterActive = false;
	
	TArray<float>						FloatComponents;
	TArray<FFloat16>					HalfComponents;
	TArray<int32>						Int32Components;

	int32								FoundFloatComponents = 0;
	int32								FoundHalfComponents = 0;
	int32								FoundInt32Components = 0;

	bool								bDelegatesAdded = false;
	FOnViewDataChanged					OnViewDataChangedDelegate;
	FOnSimCacheChanged					OnSimCacheChangedDelegate;
	FOnBufferChanged					OnBufferChangedDelegate;
};

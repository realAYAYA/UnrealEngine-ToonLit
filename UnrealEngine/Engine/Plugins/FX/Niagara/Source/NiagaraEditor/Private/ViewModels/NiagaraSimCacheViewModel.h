// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSimCache.h"
#include "Widgets/SNiagaraSimCacheOverview.h"

class SNiagaraSimCacheTreeView;
struct FNiagaraSimCacheComponentTreeItem;
struct FNiagaraSimCacheTreeItem;
struct FNiagaraSimCacheOverviewItem;
struct FNiagaraSystemSimCacheCaptureReply;
struct FNiagaraSimCacheDataBuffersLayout;
class UNiagaraComponent;

class FNiagaraSimCacheViewModel : public TSharedFromThis<FNiagaraSimCacheViewModel>, public FGCObject
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
	
	NIAGARAEDITOR_API FNiagaraSimCacheViewModel();
	NIAGARAEDITOR_API ~FNiagaraSimCacheViewModel();

	NIAGARAEDITOR_API void Initialize(TWeakObjectPtr<UNiagaraSimCache> SimCache);

	NIAGARAEDITOR_API void UpdateSimCache(const FNiagaraSystemSimCacheCaptureReply& Reply);

	NIAGARAEDITOR_API void SetupPreviewComponentAndInstance();

	TConstArrayView<FComponentInfo> GetCurrentComponentInfos() const { return GetComponentInfos(EmitterIndex); }

	NIAGARAEDITOR_API TConstArrayView<FComponentInfo> GetComponentInfos(int32 InEmitterIndex) const;

	TConstArrayView<FString> GetComponentFilters() const { return ComponentFilterArray; }

	void SetComponentFilters(const TArray<FString>& NewComponentFilterArray)
	{
		bComponentFilterActive = true;
		ComponentFilterArray.Empty();
		ComponentFilterArray.Append(NewComponentFilterArray);
		OnViewDataChangedDelegate.Broadcast(true);
	}

	bool IsComponentFilterActive() const { return bComponentFilterActive; }
	
	int32 GetNumInstances() const { return NumInstances; }

	NIAGARAEDITOR_API int32 GetNumFrames() const;

	int32 GetFrameIndex() const { return FrameIndex; }

	NIAGARAEDITOR_API void SetFrameIndex(const int32 InFrameIndex);

	int32 GetEmitterIndex() const { return EmitterIndex; };

	NIAGARAEDITOR_API void SetEmitterIndex(int32 InEmitterIndex);

	NIAGARAEDITOR_API FText GetComponentText(FName ComponentName, int32 InstanceIndex) const;

	NIAGARAEDITOR_API bool IsCacheValid();

	NIAGARAEDITOR_API int32 GetNumEmitterLayouts();

	NIAGARAEDITOR_API FName GetEmitterLayoutName(int32 Index);

	NIAGARAEDITOR_API FOnViewDataChanged& OnViewDataChanged();

	NIAGARAEDITOR_API FOnSimCacheChanged& OnSimCacheChanged();

	NIAGARAEDITOR_API FOnBufferChanged& OnBufferChanged();

	NIAGARAEDITOR_API void OnCacheModified(UNiagaraSimCache* SimCache);

	NIAGARAEDITOR_API void UpdateCachedFrame();

	// Build out component infos for each buffer in this cache
	NIAGARAEDITOR_API void UpdateComponentInfos();

	NIAGARAEDITOR_API void BuildTreeItemChildren(TSharedPtr<FNiagaraSimCacheTreeItem> InTreeItem, TWeakPtr<SNiagaraSimCacheTreeView> OwningTreeView);

	NIAGARAEDITOR_API void RecursiveBuildTreeItemChildren(FNiagaraSimCacheTreeItem* Root, TSharedRef<FNiagaraSimCacheComponentTreeItem> Parent,
	FNiagaraTypeDefinition TypeDefinition, TWeakPtr<SNiagaraSimCacheTreeView> OwningTreeView);
	
	// Construct entries for the tree view.
	NIAGARAEDITOR_API void BuildEntries(TWeakPtr<SNiagaraSimCacheTreeView> OwningTreeView);

	NIAGARAEDITOR_API void UpdateCurrentEntries();

	NIAGARAEDITOR_API TArray<TSharedRef<FNiagaraSimCacheTreeItem>>* GetCurrentRootEntries();


	NIAGARAEDITOR_API TArray<TSharedRef<FNiagaraSimCacheOverviewItem>>* GetBufferEntries();

	UNiagaraComponent* GetPreviewComponent() { return PreviewComponent; }

	//~ FGCObject interface
	NIAGARAEDITOR_API virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FNiagaraSimCacheViewModel");
	}

private:
	NIAGARAEDITOR_API void BuildComponentInfos(const FName Name, const UScriptStruct* Struct, TArray<FComponentInfo>& ComponentInfos);
	
	// The sim cache being viewed
	TObjectPtr<UNiagaraSimCache> SimCache = nullptr;

	// Component for preview scene
	TObjectPtr<UNiagaraComponent> PreviewComponent = nullptr;

	// Which frame of the cached sim is being viewed
	int32 FrameIndex = 0;
	
	// Which Emitter of the cached sim is being viewed
	int32 EmitterIndex = INDEX_NONE;

	// Number of particles in the given frame
	int32 NumInstances = 0;

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

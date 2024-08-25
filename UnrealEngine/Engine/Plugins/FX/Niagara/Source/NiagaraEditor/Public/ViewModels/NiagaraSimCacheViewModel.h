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
	NIAGARAEDITOR_API virtual ~FNiagaraSimCacheViewModel() override;
	
	NIAGARAEDITOR_API TConstArrayView<FComponentInfo> GetCurrentComponentInfos() const { return GetComponentInfos(EmitterIndex); }
	NIAGARAEDITOR_API TConstArrayView<FComponentInfo> GetComponentInfos(int32 InEmitterIndex) const;

	NIAGARAEDITOR_API int32 GetNumInstances() const { return NumInstances; }
	NIAGARAEDITOR_API int32 GetNumFrames() const;
	NIAGARAEDITOR_API int32 GetFrameIndex() const { return FrameIndex; }
	NIAGARAEDITOR_API void SetFrameIndex(const int32 InFrameIndex);

	NIAGARAEDITOR_API FNiagaraVariableBase GetActiveDataInterface() const { return ActiveDataInterface; };
	NIAGARAEDITOR_API UObject* GetActiveDataInterfaceStorage() const;

	NIAGARAEDITOR_API FOnViewDataChanged& OnViewDataChanged();
	NIAGARAEDITOR_API FOnSimCacheChanged& OnSimCacheChanged();
	NIAGARAEDITOR_API FOnBufferChanged& OnBufferChanged();

	void Initialize(TWeakObjectPtr<UNiagaraSimCache> SimCache);

	TConstArrayView<FString> GetComponentFilters() const { return ComponentFilterArray; }
	void SetComponentFilters(const TArray<FString>& NewComponentFilterArray);
	bool IsComponentFilterActive() const { return bComponentFilterActive; }
	
	int32 GetEmitterIndex() const { return EmitterIndex; };
	void SetEmitterIndex(int32 InEmitterIndex, FNiagaraVariableBase ActiveDataInterface = FNiagaraVariableBase());

	FText GetComponentText(FName ComponentName, int32 InstanceIndex) const;
	bool IsCacheValid();
	int32 GetNumEmitterLayouts();
	FName GetEmitterLayoutName(int32 Index);

	// Construct entries for the tree view.
	void BuildEntries(TWeakPtr<SNiagaraSimCacheTreeView> OwningTreeView);
	TArray<TSharedRef<FNiagaraSimCacheTreeItem>>* GetCurrentRootEntries();
	TArray<TSharedRef<FNiagaraSimCacheOverviewItem>>* GetBufferEntries();

	UNiagaraComponent* GetPreviewComponent() { return PreviewComponent; }

	//~ FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FNiagaraSimCacheViewModel");
	}

private:
	void SetupPreviewComponentAndInstance();
	void BuildComponentInfos(const FName Name, const UScriptStruct* Struct, TArray<FComponentInfo>& ComponentInfos);
	void OnCacheModified(UNiagaraSimCache* SimCache);
	void UpdateCachedFrame();
	void UpdateCurrentEntries();

	void BuildTreeItemChildren(TSharedPtr<FNiagaraSimCacheTreeItem> InTreeItem, TWeakPtr<SNiagaraSimCacheTreeView> OwningTreeView);
	void RecursiveBuildTreeItemChildren(FNiagaraSimCacheTreeItem* Root, TSharedRef<FNiagaraSimCacheComponentTreeItem> Parent, FNiagaraTypeDefinition TypeDefinition, TWeakPtr<SNiagaraSimCacheTreeView> OwningTreeView);

	// Build out component infos for each buffer in this cache
	void UpdateComponentInfos();
	
	// The sim cache being viewed
	TObjectPtr<UNiagaraSimCache> SimCache = nullptr;

	// Component for preview scene
	TObjectPtr<UNiagaraComponent> PreviewComponent = nullptr;

	// Which frame of the cached sim is being viewed
	int32 FrameIndex = 0;
	
	// Which Emitter of the cached sim is being viewed
	int32 EmitterIndex = INDEX_NONE;

	// Which data interface of the cached sim is being viewed
	FNiagaraVariableBase ActiveDataInterface;

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

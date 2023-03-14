// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/GCObject.h"
#include "TickableEditorObject.h"
#include "EditorUndoClient.h"
#include "Toolkits/IToolkitHost.h"
#include "ISkeletalMeshEditor.h"
#include "Containers/ArrayView.h"

class IDetailLayoutBuilder;
class IDetailsView;
class IPersonaToolkit;
class IPersonaViewport;
class ISkeletonTree;
class USkeletalMesh;
class UClothingAssetBase;
class ISkeletonTreeItem;
struct HActor;
struct FViewportClick;
struct FSkeletalMeshClothBuildParams;
struct FToolMenuContext;
class UToolMenu;
class SSkeletalMeshEditorToolbox;


namespace SkeletalMeshEditorModes
{
	// Mode identifiers
	extern const FName SkeletalMeshEditorMode;
}

namespace SkeletalMeshEditorTabs
{
	// Tab identifiers
	extern const FName DetailsTab;
	extern const FName SkeletonTreeTab;
	extern const FName ViewportTab;
	extern const FName AdvancedPreviewTab;
	extern const FName AssetDetailsTab;
	extern const FName MorphTargetsTab;
	extern const FName MeshDetailsTab;
	extern const FName AnimationMappingTab;
	extern const FName ToolboxDetailsTab;
}

class FSkeletalMeshEditor : public ISkeletalMeshEditor, public FGCObject, public FEditorUndoClient, public FTickableEditorObject
{
public:
	FSkeletalMeshEditor();

	virtual ~FSkeletalMeshEditor();

	/** Edits the specified Skeleton object */
	void InitSkeletalMeshEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, class USkeletalMesh* InSkeletalMesh);

	/** IHasPersonaToolkit interface */
	virtual TSharedRef<class IPersonaToolkit> GetPersonaToolkit() const override { return PersonaToolkit.ToSharedRef(); }

	/** IToolkit interface */
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void InitToolMenuContext(FToolMenuContext& MenuContext) override;
	// IToolkitHost Interface
	virtual void OnToolkitHostingStarted(const TSharedRef<class IToolkit>& Toolkit) override;
	virtual void OnToolkitHostingFinished(const TSharedRef<class IToolkit>& Toolkit) override;

	virtual void AddViewportOverlayWidget(TSharedRef<SWidget> InOverlaidWidget) override;
	virtual void RemoveViewportOverlayWidget(TSharedRef<SWidget> InOverlaidWidget) override;
	
	/** FBaseToolkit overrides */
	virtual bool ProcessCommandBindings(const FKeyEvent& InKeyEvent) const override;
	
	//~ Begin FAssetEditorToolkit Interface.
	virtual bool OnRequestClose() override;
	//~ End FAssetEditorToolkit Interface.

	/** FEditorUndoClient interface */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	/** FTickableEditorObject Interface */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }

	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const override
	{
		return FString(TEXT("AnimatingObjects/SkeletalMeshAnimation/Persona/Modes/Mesh"));
	}

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FSkeletalMeshEditor");
	}

	/** Get the skeleton tree widget */
	TSharedRef<class ISkeletonTree> GetSkeletonTree() const { return SkeletonTree.ToSharedRef(); }

	void HandleDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView);

	void HandleMeshDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView);

	void HandleViewportCreated(const TSharedRef<class IPersonaViewport>& InViewport);

	UObject* HandleGetAsset();

	// Returns the currently hosted toolkit. Can be invalid if no toolkit is being hosted.
	TSharedPtr<IToolkit> GetHostedToolkit() const { return HostedToolkit; }

private:
	void HandleObjectsSelected(const TArray<UObject*>& InObjects);

	void HandleObjectSelected(UObject* InObject);

	void HandleSelectionChanged(const TArrayView<TSharedPtr<ISkeletonTreeItem>>& InSelectedItems, ESelectInfo::Type InSelectInfo);

	void HandleReimportMesh(int32 SourceFileIndex = INDEX_NONE);
	void HandleReimportMeshWithNewFile(int32 SourceFileIndex = INDEX_NONE);
	
	bool HandleReimportMeshInternal(int32 SourceFileIndex = INDEX_NONE, bool bWithNewFile = false);
	void HandleReimportAllMesh(int32 SourceFileIndex = INDEX_NONE);
	void HandleReimportAllMeshWithNewFile(int32 SourceFileIndex = INDEX_NONE);

	void HandleOnPreviewSceneSettingsCustomized(IDetailLayoutBuilder& DetailBuilder);

	/** Callback for toggling UV drawing in the viewport */
	void ToggleMeshSectionSelection();

	/** Callback for checking whether the UV drawing is switched on. */
	bool IsMeshSectionSelectionChecked() const;

	void HandleMeshClick(HActor* HitProxy, const FViewportClick& Click);

	// Clothing menu handlers (builds and handles clothing context menu options)
	void FillApplyClothingAssetMenu(FMenuBuilder& MenuBuilder, int32 InLodIndex, int32 InSectionIndex);
	void FillCreateClothingMenu(FMenuBuilder& MenuBuilder, int32 InLodIndex, int32 InSectionIndex);
	void FillCreateClothingLodMenu(FMenuBuilder& MenuBuilder, int32 InLodIndex, int32 InSectionIndex);
	void OnRemoveClothingAssetMenuItemClicked(int32 InLodIndex, int32 InSectionIndex);
	void OnCreateClothingAssetMenuItemClicked(FSkeletalMeshClothBuildParams& Params);
	void OnApplyClothingAssetClicked(UClothingAssetBase* InAssetToApply, int32 InMeshLodIndex, int32 InMeshSectionIndex, int32 InClothLodIndex);

	bool CanApplyClothing(int32 InLodIndex, int32 InSectionIndex);
	bool CanRemoveClothing(int32 InLodIndex, int32 InSectionIndex);
	bool CanCreateClothing(int32 InLodIndex, int32 InSectionIndex);
	bool CanCreateClothingLod(int32 InLodIndex, int32 InSectionIndex);

	void ApplyClothing(UClothingAssetBase* InAsset, int32 InLodIndex, int32 InSectionIndex, int32 InClothingLod);
	void RemoveClothing(int32 InLodIndex, int32 InSectionIndex);
	//////////////////////////////////////////////////////////////////////////

	// Generate LOD sections menu handlers
	void OnRemoveSectionFromLodAndBelowMenuItemClicked(int32 LodIndex, int32 SectionIndex);
	//////////////////////////////////////////////////////////////////////////

	void RegisterReimportContextMenu(const FName InBaseMenuName);

	static TSharedPtr<FSkeletalMeshEditor> GetSkeletalMeshEditor(const FToolMenuContext& InMenuContext);

	void OnEditorModeIdChanged(const FEditorModeID& ModeChangedID, bool bIsEnteringMode);

	bool CanSpawnToolboxTab(const FSpawnTabArgs& InArgs) const;
	TSharedRef<SDockTab> SpawnToolboxTab(const FSpawnTabArgs& InArgs);
	void OnToolboxTabClosed(TSharedRef<SDockTab> InClosedTab);

private:
	void ExtendMenu();

	void BakeMaterials();
	void ExtendToolbar();

	void BindCommands();

public:
	/** Multicast delegate fired on global undo/redo */
	FSimpleMulticastDelegate OnPostUndo;

	/** Multicast delegate fired on curves changing */
	FSimpleMulticastDelegate OnCurvesChanged;

private:
	/** The skeleton we are editing */
	USkeletalMesh* SkeletalMesh;

	/** Toolbar extender */
	TSharedPtr<FExtender> ToolbarExtender;

	/** Menu extender */
	TSharedPtr<FExtender> MenuExtender;

	/** Persona toolkit */
	TSharedPtr<class IPersonaToolkit> PersonaToolkit;

	/** Skeleton tree */
	TSharedPtr<class ISkeletonTree> SkeletonTree;

	/** Viewport */
	TSharedPtr<class IPersonaViewport> Viewport;

	/** Details panel */
	TSharedPtr<class IDetailsView> DetailsView;

	// The toolkit we're currently hosting.
	TSharedPtr<IToolkit> HostedToolkit;

	// The toolbox widget
	TSharedPtr<SSkeletalMeshEditorToolbox> ToolboxWidget;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TickableEditorObject.h"
#include "Templates/DMXPixelMappingComponentTemplate.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "DMXPixelMappingComponentReference.h"

class FDMXPixelMappingToolbar;
class SDMXPixelMappingHierarchyView;
class SDMXPixelMappingDesignerView;
class SDMXPixelMappingDetailsView;
class SDMXPixelMappingLayoutView;
class SDMXPixelMappingPaletteView;
class SDMXPixelMappingPreviewView;
class FDMXPixelMappingPaletteViewModel;
class UDMXPixelMapping;
class UDMXPixelMappingBaseComponent;
class UDMXPixelMappingMatrixComponent;
class UDMXPixelMappingOutputComponent;
class UDMXPixelMappingRendererComponent;

struct FScopedSlowTask;
class FTabManager;
class SDockableTab;
class SWidget;


/**
 * Implements an Editor toolkit for Pixel Mapping.
 */
class FDMXPixelMappingToolkit
	: public FAssetEditorToolkit
	, public FTickableEditorObject
{
	friend class FDMXPixelMappingToolbar;

public:
	DECLARE_MULTICAST_DELEGATE(FOnSelectedComponentsChangedDelegate)
	FOnSelectedComponentsChangedDelegate& GetOnSelectedComponentsChangedDelegate() { return OnSelectedComponentsChangedDelegate; }

public:
	/** Default constructor */
	FDMXPixelMappingToolkit();

	/**
	 * Destructor.
	 */
	virtual ~FDMXPixelMappingToolkit();

public:

	/**
	 * Edits the specified Texture object.
	 *
	 * @param Mode The tool kit mode.
	 * @param InitToolkitHost
	 * @param ObjectToEdit The texture object to edit.
	 */
	void InitPixelMappingEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UDMXPixelMapping* InDMXPixelMapping);

public:

	//~ Begin FAssetEditorToolkit Interface
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	//~ End FAssetEditorToolkit Interface

	//~ Begin IToolkit Interface
	virtual FText GetBaseToolkitName() const override;
	virtual FName GetToolkitFName() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.0f, 0.0f, 0.2f, 0.5f); }
	virtual FString GetWorldCentricTabPrefix() const override;
	//~ End IToolkit Interface

	//~ Begin FTickableEditorObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override;
	//~ End FTickableEditorObject Interface

	UDMXPixelMapping* GetDMXPixelMapping() const { return DMXPixelMapping; }

	FDMXPixelMappingComponentReference GetReferenceFromComponent(UDMXPixelMappingBaseComponent* InComponent);

	UDMXPixelMappingRendererComponent* GetActiveRendererComponent() const { return ActiveRendererComponent.Get(); }

	TArray<UDMXPixelMappingOutputComponent*> GetActiveOutputComponents() const
	{
		TArray<UDMXPixelMappingOutputComponent*> ValidatedActiveOutputComponents;
		for (TWeakObjectPtr<UDMXPixelMappingOutputComponent> ActiveOutputComponent : ActiveOutputComponents)
		{
			if (ActiveOutputComponent.IsValid())
			{
				ValidatedActiveOutputComponents.Add(ActiveOutputComponent.Get());
			}
		}
		return ValidatedActiveOutputComponents;
	}

	const TSharedPtr<FUICommandList>& GetDesignerCommandList() const { return DesignerCommandList; }

	const TSharedPtr<FDMXPixelMappingPaletteViewModel>& GetPaletteViewModel() const { return PaletteViewModel; }

	const TSet<FDMXPixelMappingComponentReference>& GetSelectedComponents() const { return SelectedComponents; }

	/** Gets or creates the Palette View for this Pixel Mapping instance */
	TSharedRef<SWidget> CreateOrGetView_PaletteView();

	/** Gets or creates the Hierarchy View for this Pixel Mapping instance */
	TSharedRef<SWidget> CreateOrGetView_HierarchyView();

	/** Gets or creates the Designer View for this Pixel Mapping instance */
	TSharedRef<SWidget> CreateOrGetView_DesignerView();

	/** Gets or creates the Preview View for this Pixel Mapping instance */
	TSharedRef<SWidget> CreateOrGetView_PreviewView();

	/** Gets or creates the Details View for this Pixel Mapping instance */
	TSharedRef<SWidget> CreateOrGetView_DetailsView();

	/** Gets or creates the Layout View for this Pixel Mapping instance */
	TSharedRef<SWidget> CreateOrGetView_LayoutView();

	bool IsPlayingDMX() const { return bIsPlayingDMX; }

	void SetActiveRenderComponent(UDMXPixelMappingRendererComponent* InComponent);

	/** Creates an array of components given specifed component references */
	template <typename ComponentType>
	TArray<ComponentType> MakeComponentArray(const TSet<FDMXPixelMappingComponentReference>& Components) const;

	void SelectComponents(const TSet<FDMXPixelMappingComponentReference>& Components);

	/** Returns true if the component is selected */
	bool IsComponentSelected(UDMXPixelMappingBaseComponent* Component) const;

	void AddRenderer();

	void OnComponentRenamed(UDMXPixelMappingBaseComponent* InComponent);

	/** 
	 * Creates components from the template. Returns the new components.
	 * If many component were created, the first component is the topmost parent.
	 */
	TArray<UDMXPixelMappingBaseComponent*> CreateComponentsFromTemplates(UDMXPixelMappingRootComponent* RootComponent, UDMXPixelMappingBaseComponent* Target, const TArray<TSharedPtr<FDMXPixelMappingComponentTemplate>>& Templates);

	/** Deletes the selected Components */
	void DeleteSelectedComponents();

private:
	/** Called when a Component was added to the pixel mapping */
	void OnComponentAdded(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component);

	/** Called when a Component was removed from the pixel mapping */
	void OnComponentRemoved(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component);

	void PlayDMX();

	void StopPlayingDMX();

	void ExecutebTogglePlayDMXAll();

	/** Returns true if the selected component can be sized to texture */
	bool CanSizeSelectedComponentToTexture() const;

	/** Sizes the component to the render target of the pixelmapping asset */
	void SizeSelectedComponentToTexture();

	void UpdateBlueprintNodes(UDMXPixelMapping* InDMXPixelMapping);

	void OnSaveThumbnailImage();

	void InitializeInternal(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, const FGuid& MessageLogGuid);

	/** Spawns the Pallette View */
	TSharedRef<SDockTab> SpawnTab_PaletteView(const FSpawnTabArgs& Args);

	/** Spawns the Hierarchy View */
	TSharedRef<SDockTab> SpawnTab_HierarchyView(const FSpawnTabArgs& Args);

	/** Spawns the Designer View */
	TSharedRef<SDockTab> SpawnTab_DesignerView(const FSpawnTabArgs& Args);

	/** Spawns the Preview View */
	TSharedRef<SDockTab> SpawnTab_PreviewView(const FSpawnTabArgs& Args);

	/** Spawns the Details View */
	TSharedRef<SDockTab> SpawnTab_DetailsView(const FSpawnTabArgs& Args);

	/** Spawns the Layout View */
	TSharedRef<SDockTab> SpawnTab_LayoutView(const FSpawnTabArgs& Args);

	void SetupCommands();

	void ExtendToolbar();

	void CreateInternalViewModels();

	void CreateInternalViews();

	UDMXPixelMapping* DMXPixelMapping;

	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap<FName, TWeakPtr<SDockableTab>> SpawnedToolPanels;

	/** The Palette View instance */
	TSharedPtr<SDMXPixelMappingPaletteView> PaletteView;
	
	/** The Hierarchy View instance */
	TSharedPtr<SDMXPixelMappingHierarchyView> HierarchyView;

	/** The Designer View instance */
	TSharedPtr<SDMXPixelMappingDesignerView> DesignerView;

	/** The Preview View instance */
	TSharedPtr<SDMXPixelMappingPreviewView> PreviewView;

	/** The Details View instance */
	TSharedPtr<SDMXPixelMappingDetailsView> DetailsView;

	/** The Layout View instance */
	TSharedPtr<SDMXPixelMappingLayoutView> LayoutView;

	/** The Palette View Model instance */
	TSharedPtr<FDMXPixelMappingPaletteViewModel> PaletteViewModel;

	FOnSelectedComponentsChangedDelegate OnSelectedComponentsChangedDelegate;

	TSet<FDMXPixelMappingComponentReference> SelectedComponents;

	TSharedPtr<FDMXPixelMappingToolbar> Toolbar;

	TWeakObjectPtr<UDMXPixelMappingRendererComponent> ActiveRendererComponent;

	TArray<TWeakObjectPtr<UDMXPixelMappingOutputComponent>> ActiveOutputComponents;

	/** Command list for handling widget actions in the PixelMapping Toolkit */
	TSharedPtr<FUICommandList> DesignerCommandList;

	bool bIsPlayingDMX;

	bool bTogglePlayDMXAll;

	bool bRequestStopSendingDMX;

	uint8 RequestStopSendingTicks;

	static const uint8 RequestStopSendingMaxTicks;

	/** True while adding components (to avoid needlessly updating blueprint nodes on each component added via our own methods) */
	bool bAddingComponents = false;

	/** True while removing components (to avoid needlessly updating blueprint nodes on each component removed via our own methods) */
	bool bRemovingComponents = false;

public:
	/** Name of the Palette View Tab */
	static const FName PaletteViewTabID;

	/** Name of the Hierarchy View Tab */
	static const FName HierarchyViewTabID;

	/** Name of the Designer View Tab */
	static const FName DesignerViewTabID;

	/** Name of the Preview View Tab */
	static const FName PreviewViewTabID;

	/** Name of the Details View Tab */
	static const FName DetailsViewTabID;

	/** Name of the Details View Tab */
	static const FName LayoutViewTabID;
};

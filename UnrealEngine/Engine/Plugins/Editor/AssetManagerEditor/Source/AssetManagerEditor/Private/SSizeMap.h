// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "AssetRegistry/AssetData.h"
#include "AssetManagerEditorModule.h"
#include "ITreeMap.h"
#include "STreeMap.h"

class FAssetThumbnailPool;
class FUICommandList;

/**
 * Tree map for displaying the size of assets
 */
class SSizeMap : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS(SSizeMap)
	{}
	SLATE_END_ARGS()

	/** Default constructor for SSizeMap */
	SSizeMap();

	/** Destructor for SSizeMap */
	~SSizeMap();

	/**
	 * Construct the widget
	 *
	 * @param	InArgs				A declaration from which to construct the widget
	 */
	void Construct(const FArguments& InArgs);

	/** Sets the assets to view at the root of the size map.  This will rebuild the map. */
	void SetRootAssetIdentifiers(const TArray<FAssetIdentifier>& NewRootAssetIdentifiers);

	/** Called when the current registry source changes */
	void SetCurrentRegistrySource(const FAssetManagerEditorRegistrySource* RegistrySource);

protected:
	/** This struct will contain size map-specific payload data that we'll associate with tree map nodes using a hash table */
	struct FNodeSizeMapData
	{
		/** How big the asset is */
		SIZE_T AssetSize;

		/** Whether it has a known size or not */
		bool bHasKnownSize;

		/** Data from the asset registry about this asset */
		FAssetData AssetData;
	};

	/** Overrides */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	/** Called after the initial asset registry scan finishes */
	void OnInitialAssetRegistrySearchComplete();

	/** Recursively discovers and loads dependent assets, building up a tree map node hierarchy */
	void GatherDependenciesRecursively(TSharedPtr<class FAssetThumbnailPool>& AssetThumbnailPool, TMap<FAssetIdentifier, TSharedPtr<class FTreeMapNodeData>>& VisitedAssetIdentifiers, const TArray<FAssetIdentifier>& AssetIdentifiers, const FPrimaryAssetId& FilterPrimaryAsset, const TSharedPtr<FTreeMapNodeData>& ParentTreeMapNode, TSharedPtr<FTreeMapNodeData>& SharedRootNode, int32& NumAssetsWhichFailedToLoad);

	/** After the node tree is built up, this function is called to generate nice labels for the nodes and to do a final clean-up pass on the tree */
	void FinalizeNodesRecursively(TSharedPtr<FTreeMapNodeData>& Node, const TSharedPtr<FTreeMapNodeData>& SharedRootNode, int32& TotalAssetCount, SIZE_T& TotalSize, bool& bAnyUnknownSizes);

	/** Refreshes the display */
	void RefreshMap();

	/** Returns the size map data associated with current tree map node. If Consume is true it unsets the current selected node */
	TSharedPtr<FTreeMapNodeData> GetCurrentTreeNode(bool bConsumeSelection = false) const;
	const FNodeSizeMapData* GetCurrentSizeMapData(bool bConsumeSelection = false) const;

	/** Returns package names that are referenced by this node, not including root node */
	void GetReferencedPackages(TSet<FName>& OutPackageNames, TSharedPtr<FTreeMapNodeData> RootNode, bool bRecurse = true) const;

	/** Called when the user right-clicks on an asset in the tree */
	void OnTreeMapNodeRightClicked(class FTreeMapNodeData& TreeMapNodeData, const FPointerEvent& MouseEvent);
	void GetMakeCollectionWithDependenciesSubMenu(class FMenuBuilder& MenuBuilder);

	/** Delegates for context menu */
	void FindInContentBrowser() const;
	bool IsAnythingSelected() const;
	void EditSelectedAssets() const;
	void FindReferencesForSelectedAssets() const;
	void ShowAssetAuditForSelectedAssets() const;
	void ShowAssetAuditForReferencedAssets() const;
	void MakeCollectionWithDependencies(ECollectionShareType::Type ShareType);

	/** Back button */
	FReply OnZoomOut();
	bool CanZoomOut() const;

	/** Combo Box */
	TSharedRef<SWidget> GenerateSizeTypeComboItem(TSharedPtr<FName> InItem) const;
	void HandleSizeTypeComboChanged(TSharedPtr<FName> Item, ESelectInfo::Type SelectInfo);
	FText GetSizeTypeComboText() const;
	FText GetSizeTypeText(FName SizeType) const;
	FText GetOverviewText() const;
	bool IsSizeTypeEnabled() const;

	TArray<TSharedPtr<FName>> SizeTypeComboList;
	FName CurrentSizeType;
	FText OverviewText;
	bool bMemorySizeCached;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> ComboBoxWidget;

	/** Our tree map widget */
	TSharedPtr<class STreeMap> TreeMapWidget;

	/** The assets we were asked to look at */
	TArray<FAssetIdentifier> RootAssetIdentifiers;

	/** Our tree map source data */
	TSharedPtr<class FTreeMapNodeData> RootTreeMapNode;

	/** Thumbnail pool */
	TSharedPtr<class FAssetThumbnailPool> AssetThumbnailPool;

	/** Maps a tree node to our size map-specific user data for that node */
	typedef TMap<TSharedRef<FTreeMapNodeData>, FNodeSizeMapData> FNodeSizeMapDataMap;
	FNodeSizeMapDataMap NodeSizeMapDataMap;
	mutable TWeakPtr<FTreeMapNodeData> CurrentSelectedNode;

	/** The registry source to display information for */
	const FAssetManagerEditorRegistrySource* CurrentRegistrySource;

	/** Commands handled by this widget */
	TSharedPtr<FUICommandList> Commands;

	/** Cached interfaces */
	class IAssetRegistry* AssetRegistry;
	class UAssetManager* AssetManager;
	class IAssetManagerEditorModule* EditorModule;
};


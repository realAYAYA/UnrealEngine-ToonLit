// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "PropertyHandle.h"
#include "AssetRegistry/AssetData.h"
#include "CollectionManagerTypes.h"

class SComboButton;
class FAssetThumbnailPool;
class FAssetThumbnail;
class SBorder;
class SBox;
class ITableRow;
class STableViewBase;
class SToolInputAssetPicker;


/**
* SToolInputAssetComboPanel provides a similar UI to SComboPanel but
* specifically for picking Assets. The standard widget is a SComboButton
* that displays a thumbnail of the selected Asset, and on click a flyout
* panel is shown that has an Asset Picker tile view, as well as (optionally)
* a list of recently-used Assets, and also Collection-based filters.
* 
* Drag-and-drop onto the SComboButton is also supported, and the "selected Asset"
* can be mapped to/from a PropertyHandle. However note that a PropertyHandle is *not* required,
* each time the selection is modified the OnSelectionChanged(AssetData) delegate will also fire.
* 
* Note that "No Selection" is valid option by default
*/
class MODELINGEDITORUI_API SToolInputAssetComboPanel : public SCompoundWidget
{
public:

	DECLARE_DELEGATE_OneParam(FOnSelectedAssetChanged, const FAssetData& AssetData);

	/** List of Collections with associated Name, used to provide pickable Collection filters */
	struct FNamedCollectionList
	{
		FString Name;
		TArray<FCollectionNameType> CollectionNames;
	};

	/** IRecentAssetsProvider allows the Client to specify a set of "recently-used" Assets which the SToolInputAssetComboPanel will try to update as the selected Asset changes */
	class IRecentAssetsProvider
	{
	public:
		virtual ~IRecentAssetsProvider() {}
		// SToolInputAssetComboPanel calls this to get the recent-assets list each time the flyout is opened
		virtual TArray<FAssetData> GetRecentAssetsList() = 0;
		// SToolInputAssetComboPanel calls this whenever the selected asset changes
		virtual void NotifyNewAsset(const FAssetData& NewAsset) = 0;
	};

public:

	SLATE_BEGIN_ARGS( SToolInputAssetComboPanel )
		: _ComboButtonTileSize(50, 50)
		, _FlyoutTileSize(85, 85)
		, _FlyoutSize(600, 400)
		, _AssetClassType(0)
		, _OnSelectionChanged()
	{
		}

		/** The size of the combo button icon tile */
		SLATE_ARGUMENT(FVector2D, ComboButtonTileSize)

		/** The size of the icon tiles in the flyout */
		SLATE_ARGUMENT(FVector2D, FlyoutTileSize)

		/** Size of the Flyout Panel */
		SLATE_ARGUMENT(FVector2D, FlyoutSize)

		/** Target PropertyHandle, selected value will be written here (Note: not required, selected  */
		SLATE_ARGUMENT( TSharedPtr<IPropertyHandle>, Property )

		/** Tooltip for the ComboButton. If Property is defined, this will be ignored. */
		SLATE_ARGUMENT( FText, ToolTipText )

		/** UClass of Asset to pick. Required, and only one class is supported */
		SLATE_ARGUMENT( UClass*, AssetClassType )

		/** (Optional) external provider/tracker of Recently-Used Assets. If not provided, Recent Assets area will not be shown. */
		SLATE_ARGUMENT(TSharedPtr<IRecentAssetsProvider>, RecentAssetsProvider)

		/** (Optional) set of collection-lists, if provided, button bar will be shown with each CollectionSet as an option */
		SLATE_ARGUMENT(TArray<FNamedCollectionList>, CollectionSets)
			
		/** This delegate is executed each time the Selected Asset is modified */
		SLATE_EVENT( FOnSelectedAssetChanged, OnSelectionChanged )

	SLATE_END_ARGS()


	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

protected:

	FVector2D ComboButtonTileSize;
	FVector2D FlyoutTileSize;
	FVector2D FlyoutSize;
	TSharedPtr<IPropertyHandle> Property;
	UClass* AssetClassType = nullptr;


	/** Delegate to invoke when selection changes. */
	FOnSelectedAssetChanged OnSelectionChanged;

	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<FAssetThumbnail> AssetThumbnail;
	TSharedPtr<SBorder> ThumbnailBorder;

	TSharedPtr<IRecentAssetsProvider> RecentAssetsProvider;

	struct FRecentAssetInfo
	{
		int Index;
		FAssetData AssetData;
	};
	TArray<TSharedPtr<FRecentAssetInfo>> RecentAssetData;

	TArray<TSharedPtr<FAssetThumbnail>> RecentThumbnails;
	TArray<TSharedPtr<SBox>> RecentThumbnailWidgets;

	void UpdateRecentAssets();

	virtual void NewAssetSelected(const FAssetData& AssetData);
	virtual FReply OnAssetThumbnailDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	TSharedRef<ITableRow> OnGenerateWidgetForRecentList(TSharedPtr<FRecentAssetInfo> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	TArray<FNamedCollectionList> CollectionSets;
	int32 ActiveCollectionSetIndex = -1;
	TSharedRef<SWidget> MakeCollectionSetsButtonPanel(TSharedRef<SToolInputAssetPicker> AssetPickerView);
};
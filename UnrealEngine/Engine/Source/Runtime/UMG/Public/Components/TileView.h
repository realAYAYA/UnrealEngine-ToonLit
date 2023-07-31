// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ListView.h"
#include "Widgets/Views/STileView.h"
#include "TileView.generated.h"

/**
 * A ListView that presents the contents as a set of tiles all uniformly sized.
 *
 * To make a widget usable as an entry in a TileView, it must inherit from the IUserObjectListEntry interface.
 */
UCLASS()
class UMG_API UTileView : public UListView
{
	GENERATED_BODY()

public:
	UTileView(const FObjectInitializer& ObjectInitializer);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	/** Sets the height of every tile entry */
	UFUNCTION(BlueprintCallable, Category = TileView)
	void SetEntryHeight(float NewHeight);

	/** Sets the width of every tile entry */
	UFUNCTION(BlueprintCallable, Category = TileView)
	void SetEntryWidth(float NewWidth);

	/** Gets the height of tile entries */
	UFUNCTION(BlueprintPure, Category = TileView)
	float GetEntryHeight() const { return EntryHeight; }

	/** Gets the width of tile entries */
	UFUNCTION(BlueprintPure, Category = TileView)
	float GetEntryWidth() const { return EntryWidth; }

protected:
	virtual TSharedRef<STableViewBase> RebuildListWidget() override;
	virtual FMargin GetDesiredEntryPadding(UObject* Item) const override;

	float GetTotalEntryHeight() const;
	float GetTotalEntryWidth() const;

	/** STileView construction helper - useful if using a custom STileView subclass */
	template <template<typename> class TileViewT = STileView>
	TSharedRef<TileViewT<UObject*>> ConstructTileView()
	{
		FTileViewConstructArgs Args;
		Args.bAllowFocus = bIsFocusable;
		Args.SelectionMode = SelectionMode;
		Args.bClearSelectionOnClick = bClearSelectionOnClick;
		Args.ConsumeMouseWheel = ConsumeMouseWheel;
		Args.bReturnFocusToSelection = bReturnFocusToSelection;
		Args.TileAlignment = TileAlignment;
		Args.EntryHeight = EntryHeight;
		Args.EntryWidth = EntryWidth;
		Args.bWrapDirectionalNavigation = bWrapHorizontalNavigation;
		Args.Orientation = Orientation;
		Args.ScrollBarStyle = &ScrollBarStyle;

		MyListView = MyTileView = ITypedUMGListView<UObject*>::ConstructTileView<TileViewT>(this, ListItems, Args);
		MyTileView->SetOnEntryInitialized(SListView<UObject*>::FOnEntryInitialized::CreateUObject(this, &UTileView::HandleOnEntryInitializedInternal));
		return StaticCastSharedRef<TileViewT<UObject*>>(MyTileView.ToSharedRef());
	}

protected:
	/** The height of each tile */
	UPROPERTY(EditAnywhere, Category = ListEntries)
	float EntryHeight = 128.f;

	/** The width of each tile */
	UPROPERTY(EditAnywhere, Category = ListEntries)
	float EntryWidth = 128.f;

	/** The method by which to align the tile entries in the available space for the tile view */
	UPROPERTY(EditAnywhere, Category = ListEntries)
	EListItemAlignment TileAlignment;

	/** True to allow left/right navigation to wrap back to the tile on the opposite edge */
	UPROPERTY(EditAnywhere, Category = Navigation)
	bool bWrapHorizontalNavigation = false;

	TSharedPtr<STileView<UObject*>> MyTileView;
};

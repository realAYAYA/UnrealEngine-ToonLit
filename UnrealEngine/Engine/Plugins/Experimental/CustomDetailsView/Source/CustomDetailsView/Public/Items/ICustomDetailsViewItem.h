// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "CustomDetailsViewArgs.h"
#include "CustomDetailsViewFwd.h"
#include "Templates/SharedPointer.h"

class FResetToDefaultOverride;
class ICustomDetailsView;
class SWidget;

class ICustomDetailsViewItem : public TSharedFromThis<ICustomDetailsViewItem>
{
public:
	virtual ~ICustomDetailsViewItem() = default;

	virtual TSharedPtr<ICustomDetailsView> GetCustomDetailsView() const = 0;

	/** Details view containing this item */
	virtual IDetailsView* GetDetailsView() const = 0;

	/** Called to update the Item's Id */
	virtual void RefreshItemId() = 0;

	/** Retrieves the Item Id last updated from RefreshItemId, to avoid having to recalculate it every time */
	virtual const FCustomDetailsViewItemId& GetItemId() const  = 0;

	/** Regenerates the Children */
	virtual void RefreshChildren(TSharedPtr<ICustomDetailsViewItem> InParentOverride = nullptr) = 0;

	virtual TSharedPtr<ICustomDetailsViewItem> GetRoot() const = 0;

	virtual TSharedPtr<ICustomDetailsViewItem> GetParent() const = 0;

	virtual void SetParent(TSharedPtr<ICustomDetailsViewItem> InParent) = 0;

	virtual const TArray<TSharedPtr<ICustomDetailsViewItem>>& GetChildren() const = 0;

	/**
	 * Instantiates a Widget for the Given Item
	 * @param InPrependWidget: Optional to prepend a Widget to the Name or Whole Row Widget
	 * @param InOwningWidget: Optional Widget to check for attributes like IsHovered()
	 */
	virtual TSharedRef<SWidget> MakeWidget(const TSharedPtr<SWidget>& InPrependWidget = nullptr
		, const TSharedPtr<SWidget>& InOwningWidget = nullptr) = 0;

	/**
	 * Get the one of the widgets that was generated in the MakeWidget
	 * Listen to the OnItemWidgetGenerated Delegate to have this Widget up to date with latest tree view
	 * @param InWidgetType: The type of widget to retrieve
	 */
	virtual TSharedPtr<SWidget> GetWidget(ECustomDetailsViewWidgetType InWidgetType) const = 0;

	/**
	 * Gets the widget set to override the widget automatically generated in the given slot.
	 */
	virtual TSharedPtr<SWidget> GetOverrideWidget(ECustomDetailsViewWidgetType InWidgetType) const = 0;

	/**
	 * Adds a widget to override an automatically generated widget for the given slot.
	 * @param InWidget The widget to override with, or a nullptr or SNullWidget::NullWidget to remove it.
	 */
	virtual void SetOverrideWidget(ECustomDetailsViewWidgetType InWidgetType, TSharedPtr<SWidget> InWidget) = 0;

	/** Override the keyframeability of this item. */
	virtual void SetKeyframeEnabled(bool bInKeyframeEnabled) = 0;

	/** Override the reset to the default information for this item. */
	virtual void SetResetToDefaultOverride(const FResetToDefaultOverride& InOverride) = 0;

	/** Checks to see if widget is visible */
	virtual bool IsWidgetVisible() const = 0;
};

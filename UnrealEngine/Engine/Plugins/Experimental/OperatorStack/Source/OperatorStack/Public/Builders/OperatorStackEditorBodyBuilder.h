// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"

class FCustomDetailsViewItemId;
class SWidget;
struct FOperatorStackEditorItem;

/** Body builder for item */
struct OPERATORSTACKEDITOR_API FOperatorStackEditorBodyBuilder
{
	/** Override the stack generated widget with a custom one */
	FOperatorStackEditorBodyBuilder& SetCustomWidget(TSharedPtr<SWidget> InWidget);
	
	/** Show a details view for the current item */
	FOperatorStackEditorBodyBuilder& SetShowDetailsView(bool bInShowDetailsView);

	FOperatorStackEditorBodyBuilder& SetDetailsViewItem(TSharedPtr<FOperatorStackEditorItem> InItem);
	
	/** Disallow specific property in the details view */
	FOperatorStackEditorBodyBuilder& DisallowProperty(FProperty* InProperty);

	/** Disallow specific category in the details view */
	FOperatorStackEditorBodyBuilder& DisallowCategory(const FName& InCategory);

	/** Allow specific property in the details view */
	FOperatorStackEditorBodyBuilder& AllowProperty(FProperty* InProperty);

	/** Allow specific category in the details view */
	FOperatorStackEditorBodyBuilder& AllowCategory(const FName& InCategory);

	/** Set a text that will be displayed when it is empty */
	FOperatorStackEditorBodyBuilder& SetEmptyBodyText(const FText& InText);

	TSharedPtr<SWidget> GetCustomWidget() const
	{
		return CustomWidget;
	}

	bool GetShowDetailsView() const
	{
		return bShowDetailsView;
	}
	
	TSharedPtr<FOperatorStackEditorItem> GetDetailsViewItem() const
	{
		return DetailsViewItem;	
	}
	
	const TArray<TSharedPtr<FCustomDetailsViewItemId>>& GetDisallowedDetailsViewItems() const
	{
		return DisallowedDetailsViewItems;
	}

	const TArray<TSharedPtr<FCustomDetailsViewItemId>>& GetAllowedDetailsViewItems() const
	{
		return AllowedDetailsViewItems;
	}
	
	const FText& GetEmptyBodyText() const
	{
		return EmptyBodyText;
	}

protected:
	/** Custom widget to replace content */
	TSharedPtr<SWidget> CustomWidget = nullptr;
	
	/** Does this body contains a details view */
	bool bShowDetailsView = false;
	
	/** Override actual item to display detail view */
	TSharedPtr<FOperatorStackEditorItem> DetailsViewItem = nullptr;

	/** When the body is empty, this will be displayed */
	FText EmptyBodyText;

	/** Disallowed items inside details view */
	TArray<TSharedPtr<FCustomDetailsViewItemId>> DisallowedDetailsViewItems;
	
	/** Allowed items inside details view */
	TArray<TSharedPtr<FCustomDetailsViewItemId>> AllowedDetailsViewItems;
};
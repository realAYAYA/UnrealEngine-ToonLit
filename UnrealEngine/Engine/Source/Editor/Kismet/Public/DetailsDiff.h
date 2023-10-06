// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "DiffUtils.h"
#include "PropertyPath.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/LinkableScrollBar.h"

class FPropertyPath;
class IDetailsView;
class UObject;

/** Struct to handle showing details for an object and provide an interface for listing all differences */
class KISMET_API FDetailsDiff
{
public:
	DECLARE_DELEGATE( FOnDisplayedPropertiesChanged );

	FDetailsDiff( const UObject* InObject, FOnDisplayedPropertiesChanged InOnDisplayedPropertiesChanged, bool bScrollbarOnLeft = false );
	~FDetailsDiff();

	/** Attempt to highlight the property with the given path, may not always succeed */
	void HighlightProperty( const FPropertySoftPath& PropertyName );

	/** Returns actual widget that is used to display details */
	TSharedRef< IDetailsView > DetailsWidget() const;

	/** Returns object being displayed */
	const UObject* GetDisplayedObject() const { return DisplayedObject; }

	/** Returns a list of all properties that would be diffed */
	TArray<FPropertySoftPath> GetDisplayedProperties() const;

	/** Perform a diff against another view, ordering either by display order or by remove/add/change */
	void DiffAgainst(const FDetailsDiff& Newer, TArray<FSingleObjectDiffEntry>& OutDifferences, bool bSortByDisplayOrder = false) const;

	/** Link the two details panels so they scroll in sync with one another */
	static void LinkScrolling(FDetailsDiff& LeftPanel, FDetailsDiff& RightPanel, const TAttribute<TArray<FVector2f>>& ScrollRate);

	static TSharedRef<IDetailsView> CreateDetailsView(const UObject* InObject, TSharedPtr<SScrollBar> ExternalScrollbar = nullptr, bool bScrollbarOnLeft = false);

private:
	void HandlePropertiesChanged();

	FOnDisplayedPropertiesChanged OnDisplayedPropertiesChanged;

	TArray< FPropertyPath > DisplayedProperties;
	const UObject* DisplayedObject;

	TSharedPtr< class IDetailsView > DetailsView;

	TSharedPtr<SLinkableScrollBar> ScrollBar;
};

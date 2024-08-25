// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "Templates/SharedPointer.h"

class SWidget;

struct FConcertPropertyChain;
struct FSoftClassPath;

namespace UE::ConcertSharedSlate
{
	/** Represents a tree view displaying properties. */
	class CONCERTSHAREDSLATE_API IPropertyTreeView
	{
	public:
		
		/**
		 * Rebuilds all property data from the property source.
		 *
		 * @param PropertiesToDisplay The properties to display
		 * @param Class The class from which the PropertiesToDisplay come
		 * @param bCanReuseExistingRowItems True, will try to reuse rows for properties in the tree already (retains selected rows).
		 *	Set this to false, if all rows should be regenerated (clears selection).
		 *	In general, always set this to false if you've changed the object for which you're displaying the class.
		 */
		virtual void RefreshPropertyData(const TSet<FConcertPropertyChain>& PropertiesToDisplay, const FSoftClassPath& Class, bool bCanReuseExistingRowItems = true) = 0;
		
		/**
		 * Reapply the filter function to all items at the end of the frame. Call e.g. when the filters have changed.
		 */
		virtual void RequestRefilter() const = 0;
		
		/**
		 * Requests that the given column be resorted, if it currently affects the row sorting (primary or secondary).
		 * Call e.g. when a sortable attribute of the column has changed.
		 */
		virtual void RequestResortForColumn(const FName& ColumnId) = 0;

		/** Scroll the given property into view, if it is contained. */
		virtual void RequestScrollIntoView(const FConcertPropertyChain& PropertyChain) = 0;

		/** Gets the tree view's widget */
		virtual TSharedRef<SWidget> GetWidget() = 0;

		virtual ~IPropertyTreeView() = default;
	};
}

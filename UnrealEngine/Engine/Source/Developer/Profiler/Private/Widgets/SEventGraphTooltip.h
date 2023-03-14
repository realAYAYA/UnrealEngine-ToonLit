// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if STATS

#include "Layout/Visibility.h"
#include "ProfilerDataSource.h"

class SToolTip;

/**
 * An advanced tooltip used to show various informations in the event graph widget.
 */
class SEventGraphTooltip
{
public:

	static TSharedPtr<SToolTip> GetTableCellTooltip( const TSharedPtr<FEventGraphSample> EventSample );

protected:

	static EVisibility GetHotPathIconVisibility(const TSharedPtr<FEventGraphSample> EventSample);
};

#endif // STATS

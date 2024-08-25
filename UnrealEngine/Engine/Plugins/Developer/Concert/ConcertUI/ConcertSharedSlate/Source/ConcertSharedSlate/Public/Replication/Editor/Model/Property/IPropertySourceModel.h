// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Model/Item/IItemSourceModel.h"
#include "Replication/Data/ConcertPropertySelection.h"

namespace UE::ConcertSharedSlate
{
	/** Info about a selectable property */
	struct FSelectablePropertyInfo
	{
		FConcertPropertyChain Property;
	};
	
	/**
	 * A specific object source, e.g. like actors, components from an actor (right-click), etc.
	 * @see IPropertySelectionSourceModel.
	 */
	using IPropertySourceModel = ConcertSharedSlate::IItemSourceModel<FSelectablePropertyInfo>;
}
